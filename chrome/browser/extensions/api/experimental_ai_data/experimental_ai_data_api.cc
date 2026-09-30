// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/extensions/api/experimental_ai_data.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace extensions {
namespace {

constexpr char kApiAccessRestrictedError[] =
    "API access restricted for this extension.";
constexpr char kChannelNotAllowedError[] =
    "API access not allowed on this channel.";
constexpr char kIncognitoNotSupportedError[] =
    "Incognito profile not supported.";
constexpr char kInvalidTabError[] = "Invalid target tab passed in.";
constexpr char kInvalidSpecifierError[] = "Parsing ai data specifier failed.";
constexpr char kDataCollectionError[] =
    "Data collection failed likely due to browser state change.";
constexpr char kPageChangedError[] = "Page changed during data collection.";
constexpr char kPolicyBlockedHostError[] =
    "Host access is restricted by policy.";

bool IsBlockedByPolicy(const Extension& extension,
                       content::WebContents& web_contents) {
  const PermissionsData& permissions = *extension.permissions_data();
  bool blocked = false;
  // Collection reads the primary page, including child frames and embedded
  // contents, not separate pages in the back/forward cache. If the primary
  // page changes during collection, CollectionPageGuard rejects the response.
  // Use live permissions so allowed-host exceptions and policy updates apply.
  web_contents.GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&permissions, &blocked](content::RenderFrameHost* frame) {
        if (blocked || !frame->IsActive() || frame->IsErrorDocument()) {
          return;
        }
        // Check both the document URL and its origin. For opaque origins,
        // retain the precursor restriction so data: or sandboxed documents
        // cannot bypass a policy that blocks their source host.
        blocked = permissions.IsPolicyBlockedHost(
                      util::GetURLForExtensionPermissionCheck(frame)) ||
                  permissions.IsPolicyBlockedHost(
                      frame->GetLastCommittedOrigin()
                          .GetTupleOrPrecursorTupleIfOpaque()
                          .GetURL());
      });
  return blocked;
}

bool IsBlockedByPolicy(const Extension& extension,
                       const AiDataKeyedService::BrowserData& data) {
  const PermissionsData& permissions = *extension.permissions_data();
  if (permissions.IsPolicyBlockedHost(GURL(data.page_context().url()))) {
    return true;
  }
  for (const auto& tab : data.tabs()) {
    // Tabs may contain extracted text as well as their URL and title.
    if (tab.has_page_context() &&
        permissions.IsPolicyBlockedHost(GURL(tab.url()))) {
      return true;
    }
  }
  // History search can return cached page passages without live WebContents.
  // URL/title-only visits and site engagement scores do not expose page
  // content.
  for (const auto& query : data.history_query_result()) {
    for (const auto& visit : query.history_data().visit_item()) {
      if (visit.passages_size() > 0 &&
          permissions.IsPolicyBlockedHost(GURL(visit.page_url()))) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

class ExperimentalAiDataApiFunction::CollectionPageGuard
    : public content::WebContentsObserver {
 public:
  explicit CollectionPageGuard(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  content::WebContents* GetWebContents() const {
    if (page_changed_ || !web_contents() ||
        web_contents()->IsBeingDestroyed()) {
      return nullptr;
    }
    return web_contents();
  }

 private:
  // The primary page was replaced, for example by a cross-document
  // navigation, back/forward cache restore, or prerender activation.
  // DOM edits and same-document navigations do not trigger this callback.
  void PrimaryPageChanged(content::Page&) override { page_changed_ = true; }

  bool page_changed_ = false;
};

ExperimentalAiDataApiFunction::ExperimentalAiDataApiFunction() = default;

ExperimentalAiDataApiFunction::~ExperimentalAiDataApiFunction() = default;

bool ExperimentalAiDataApiFunction::PreRunValidation(std::string* error) {
  // Check the allowlist and return an error if extension is not allow listed.
  if (!AiDataKeyedService::IsExtensionAllowlistedForData(extension_id())) {
    *error = kApiAccessRestrictedError;
    return false;
  }

  if (GetCurrentChannel() == version_info::Channel::STABLE &&
      !AiDataKeyedService::IsExtensionAllowlistedForStable(extension_id())) {
    *error = kChannelNotAllowedError;
    return false;
  }

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  if (!ai_data_service) {
    *error = kIncognitoNotSupportedError;
    return false;
  }
  DCHECK(ai_data_service);

  return true;
}

std::optional<std::string> ExperimentalAiDataApiFunction::StartDataCollection(
    content::WebContents* web_contents,
    int max_tabs_for_text_collection) {
  CHECK(collection_page_guards_.empty());
  // Keep the guards alive until the response, including serialization, so page
  // changes or tab closure invalidate data after collection finishes.
  collection_page_guards_.push_back(
      std::make_unique<CollectionPageGuard>(web_contents));
  if (max_tabs_for_text_collection > 0) {
    tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
    BrowserWindowInterface* browser =
        tab ? tab->GetBrowserWindowInterface() : nullptr;
    TabListInterface* tab_list =
        browser ? TabListInterface::From(browser) : nullptr;
    if (tab_list) {
      // Match the service's tab-index limit. Tabs beyond it contribute only
      // URL/title metadata, so their hosts do not restrict this collection.
      for (int index = 0; index < tab_list->GetTabCount() &&
                          index < max_tabs_for_text_collection;
           ++index) {
        content::WebContents* contents = tab_list->GetTab(index)->GetContents();
        if (contents != web_contents) {
          collection_page_guards_.push_back(
              std::make_unique<CollectionPageGuard>(contents));
        }
      }
    }
  }
  return GetDataCollectionError();
}

std::optional<std::string>
ExperimentalAiDataApiFunction::GetDataCollectionError() const {
  CHECK(!collection_page_guards_.empty());
  for (const auto& guard : collection_page_guards_) {
    content::WebContents* contents = guard->GetWebContents();
    if (!contents) {
      // Deliberately reject the entire response if any observed page changed.
      // The combined result cannot reliably separate data from replaced pages.
      return kPageChangedError;
    }
    if (IsBlockedByPolicy(*extension(), *contents)) {
      return kPolicyBlockedHostError;
    }
  }
  return std::nullopt;
}

content::WebContents* ExperimentalAiDataApiFunction::GetTargetWebContents()
    const {
  CHECK(!collection_page_guards_.empty());
  // StartDataCollection always adds the target before any other tabs.
  return collection_page_guards_.front()->GetWebContents();
}

void ExperimentalAiDataApiFunction::OnDataCollected(
    AiDataKeyedService::AiData browser_collected_data) {
  if (auto error = GetDataCollectionError()) {
    return Respond(Error(*error));
  }
  if (!browser_collected_data) {
    return Respond(Error(kDataCollectionError));
  }
  if (IsBlockedByPolicy(*extension(), *browser_collected_data)) {
    return Respond(Error(kPolicyBlockedHostError));
  }
  // Convert Proto to bytes to send over the API channel.
  const size_t size = browser_collected_data->ByteSizeLong();
  std::vector<uint8_t> data_buffer(size);

  browser_collected_data->SerializeToArray(&data_buffer[0], size);
  Respond(ArgumentList(api::experimental_ai_data::GetAiData::Results::Create(
      std::move(data_buffer))));
}

ExperimentalAiDataGetAiDataFunction::ExperimentalAiDataGetAiDataFunction() =
    default;

ExperimentalAiDataGetAiDataFunction::~ExperimentalAiDataGetAiDataFunction() =
    default;

ExtensionFunction::ResponseAction ExperimentalAiDataGetAiDataFunction::Run() {
  auto params = api::experimental_ai_data::GetAiData::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->tab_id, browser_context(), true,
                                    &web_contents)) {
    return RespondNow(Error(kInvalidTabError));
  }
  DCHECK(web_contents);

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  DCHECK(ai_data_service);

  // Pass the same limit to the guard and service so they cover the same pages.
  constexpr int kMaxTabsForTextCollection = 10;
  if (auto error =
          StartDataCollection(web_contents, kMaxTabsForTextCollection)) {
    return RespondNow(Error(*error));
  }

  ai_data_service->GetAiData(
      params->dom_node_id, web_contents, params->user_input,
      base::BindOnce(&ExperimentalAiDataGetAiDataFunction::OnDataCollected,
                     this),
      kMaxTabsForTextCollection);
  return RespondLater();
}

ExperimentalAiDataGetAiDataWithSpecifierFunction::
    ExperimentalAiDataGetAiDataWithSpecifierFunction() = default;

ExperimentalAiDataGetAiDataWithSpecifierFunction::
    ~ExperimentalAiDataGetAiDataWithSpecifierFunction() = default;

ExtensionFunction::ResponseAction
ExperimentalAiDataGetAiDataWithSpecifierFunction::Run() {
  auto params =
      api::experimental_ai_data::GetAiDataWithSpecifier::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->tab_id, browser_context(), true,
                                    &web_contents)) {
    return RespondNow(Error(kInvalidTabError));
  }
  DCHECK(web_contents);

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  DCHECK(ai_data_service);

  // De-serailizing protos is safe per
  // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md
  optimization_guide::proto::ModelPrototypingCollectionSpecifier specifier;
  if (!specifier.ParseFromArray(params->ai_data_specifier.data(),
                                params->ai_data_specifier.size())) {
    return RespondNow(Error(kInvalidSpecifierError));
  }

  const auto& tab_specifier = specifier.browser_data_collection_specifier()
                                  .tabs_context_specifier()
                                  .general_tab_specifier();
  // The service reads background tab content only when inner text is requested.
  const int max_tabs_for_text_collection =
      tab_specifier.page_context_specifier().inner_text()
          ? tab_specifier.tab_limit()
          : 0;
  if (auto error =
          StartDataCollection(web_contents, max_tabs_for_text_collection)) {
    return RespondNow(Error(*error));
  }

  ai_data_service->GetAiDataWithSpecifier(
      web_contents, specifier,
      base::BindOnce(
          &ExperimentalAiDataGetAiDataWithSpecifierFunction::OnDataCollected,
          this));
  return RespondLater();
}

}  // namespace extensions
