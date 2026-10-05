// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/extensions/api/experimental_ai_data.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
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
constexpr char kInvalidMaxMetaElementsError[] =
    "maxMetaElements must not be negative.";
constexpr char kApcSerializationError[] = "Failed to serialize APC.";
constexpr char kScreenshotDisabledByPreferencesError[] =
    "Failed to capture tab: screenshots disabled";
constexpr char kScreenshotDisabledByDlpError[] =
    "Failed to capture tab: screenshots disabled by DLP";
constexpr char kPageContextCaptureErrorPrefix[] =
    "Failed to capture page context: ";
constexpr char kApcExtractionErrorPrefix[] = "Failed to extract APC: ";
constexpr char kScreenshotCaptureErrorPrefix[] = "Failed to capture tab: ";

blink::mojom::AIPageContentOptionsPtr ToMojoApcOptions(
    const api::experimental_ai_data::ApcOptions& options) {
  const bool content_only = options.exclude_actionable_details.value_or(false);
  blink::mojom::AIPageContentOptionsPtr apc_options =
      content_only ? optimization_guide::DefaultAIPageContentOptions(
                         /*on_critical_path=*/true)
                   : optimization_guide::ActionableAIPageContentOptions(
                         /*on_critical_path=*/true);
  apc_options->include_same_site_only =
      options.exclude_cross_site_frames.value_or(false);
  if (options.exclude_ad_related.value_or(false)) {
    apc_options->non_salient_content_config =
        blink::mojom::NonSalientContentConfig::New();
    apc_options->non_salient_content_config->exclude_ad_related = true;
  }
  apc_options->max_meta_elements = options.max_meta_elements.value_or(0);
  // TODO(aleventhal): Consider exposing node_id_allowlist if callers need it.
  // For now, leave it unset to include all available IDs and avoid the API and
  // enum conversion complexity.
  return apc_options;
}

std::string ScreenshotAccessErrorToString(ScreenshotAccessError error) {
  switch (error) {
    case ScreenshotAccessError::kDisabledByPreferences:
      return kScreenshotDisabledByPreferencesError;
    case ScreenshotAccessError::kDisabledByDlp:
      return kScreenshotDisabledByDlpError;
  }
}

base::expected<base::ListValue, std::string> SerializeSnapshot(
    std::unique_ptr<page_content_annotations::FetchPageContextResult> snapshot,
    bool capture_apc,
    bool capture_screenshot) {
  std::string serialized_proto;
  if (capture_apc &&
      !snapshot->annotated_page_content_result->proto.SerializeToString(
          &serialized_proto)) {
    return base::unexpected(kApcSerializationError);
  }

  std::vector<uint8_t> screenshot_data;
  if (capture_screenshot) {
    screenshot_data = std::move(snapshot->screenshot_result->screenshot_data);
  }

  // The proto can be much larger than its serialized bytes. Release the source
  // data before allocating the base64 strings.
  snapshot.reset();

  api::experimental_ai_data::ApcSnapshot response;
  if (capture_apc) {
    response.apc_base64 = base::Base64Encode(serialized_proto);
  }
  if (capture_screenshot) {
    response.screenshot_base64 = base::Base64Encode(screenshot_data);
  }

  return api::experimental_ai_data::GetApcSnapshot::Results::Create(response);
}

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
  CHECK(ai_data_service, base::NotFatalUntil::M161);

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
  CHECK(web_contents, base::NotFatalUntil::M161);

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  CHECK(ai_data_service, base::NotFatalUntil::M161);

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
  CHECK(web_contents, base::NotFatalUntil::M161);

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  CHECK(ai_data_service, base::NotFatalUntil::M161);

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

ExperimentalAiDataGetApcSnapshotFunction::
    ExperimentalAiDataGetApcSnapshotFunction() = default;
ExperimentalAiDataGetApcSnapshotFunction::
    ~ExperimentalAiDataGetApcSnapshotFunction() = default;

ExtensionFunction::ResponseAction
ExperimentalAiDataGetApcSnapshotFunction::Run() {
  auto params =
      api::experimental_ai_data::GetApcSnapshot::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // This debugging API never collects data from an incognito profile.
  if (browser_context()->IsOffTheRecord()) {
    return RespondNow(Error(kIncognitoNotSupportedError));
  }

  using SnapshotType = api::experimental_ai_data::SnapshotType;
  bool capture_apc = false;
  bool capture_screenshot = false;
  switch (params->options.type) {
    case SnapshotType::kNone:
    case SnapshotType::kApc:
      capture_apc = true;
      break;
    case SnapshotType::kScreenshot:
      capture_screenshot = true;
      break;
    case SnapshotType::kApcAndScreenshot:
      capture_apc = true;
      capture_screenshot = true;
      break;
  }

  const auto& options = params->options;
  if (options.max_meta_elements.value_or(0) < 0) {
    return RespondNow(Error(kInvalidMaxMetaElementsError));
  }
  content::WebContents* web_contents =
      GetTabById(params->tab_id, /*include_incognito=*/false);
  if (!web_contents) {
    return RespondNow(Error(kInvalidTabError));
  }
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return RespondNow(Error(kIncognitoNotSupportedError));
  }

  if (capture_screenshot) {
    auto screenshot_access = CheckScreenshotAccess(web_contents);
    if (!screenshot_access.has_value()) {
      return RespondNow(
          Error(ScreenshotAccessErrorToString(screenshot_access.error())));
    }
  }

  // APC options can omit frames, but screenshots still include their pixels.
  // Apply the same frame policy checks to every snapshot mode.
  if (auto error = StartDataCollection(web_contents)) {
    return RespondNow(Error(*error));
  }

  StartSnapshot(web_contents, capture_apc, capture_screenshot,
                ToMojoApcOptions(options));

  return RespondLater();
}

content::WebContents* ExperimentalAiDataGetApcSnapshotFunction::GetTabById(
    int tab_id,
    bool include_incognito) {
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(),
                                    include_incognito, &web_contents)) {
    return nullptr;
  }
  return web_contents;
}

base::expected<void, ScreenshotAccessError>
ExperimentalAiDataGetApcSnapshotFunction::CheckScreenshotAccess(
    content::WebContents* web_contents) const {
  return ExtensionsBrowserClient::Get()->IsScreenshotRestricted(web_contents);
}

void ExperimentalAiDataGetApcSnapshotFunction::StartSnapshot(
    content::WebContents* web_contents,
    bool capture_apc,
    bool capture_screenshot,
    blink::mojom::AIPageContentOptionsPtr apc_options) {
  page_content_annotations::FetchPageContextOptions options;
  options.annotated_page_content_options = std::move(apc_options);
  if (capture_screenshot) {
    using ScreenshotOptions = page_content_annotations::ScreenshotOptions;
    ScreenshotOptions::ScreenshotCollectionOptions screenshot_options;
    screenshot_options.screenshot_image_format =
        ScreenshotOptions::ScreenshotImageFormat::kPng;
    screenshot_options.screenshot_compression_quality =
        ScreenshotOptions::ScreenshotCompressionQuality::kNone;
    options.screenshot_options = ScreenshotOptions::ViewportOnly(
        /*paint_preview_options=*/std::nullopt, std::move(screenshot_options));
  }

  // PageContextFetcher starts APC and capture together, rejects navigations,
  // and bounds a hung compositor copy with its screenshot timeout. APC is also
  // requested for screenshot-only calls because it supplies sensitive-field
  // bounds used by the screenshot redaction pipeline.
  FetchSnapshot(
      web_contents, options,
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ExperimentalAiDataGetApcSnapshotFunction::OnSnapshotFetched, this,
          capture_apc, capture_screenshot)));
}

void ExperimentalAiDataGetApcSnapshotFunction::FetchSnapshot(
    content::WebContents* web_contents,
    const page_content_annotations::FetchPageContextOptions& options,
    page_content_annotations::FetchPageContextResultCallback callback) {
  page_content_annotations::FetchPageContext(*web_contents, options,
                                             /*progress_listener=*/nullptr,
                                             std::move(callback));
}

void ExperimentalAiDataGetApcSnapshotFunction::OnSnapshotFetched(
    bool capture_apc,
    bool capture_screenshot,
    page_content_annotations::FetchPageContextResultCallbackArg result) {
  if (!result.has_value()) {
    Respond(Error(base::StrCat(
        {kPageContextCaptureErrorPrefix, result.error().message})));
    return;
  }

  const page_content_annotations::FetchPageContextResult& snapshot = **result;
  // APC supplies the bounds used to redact sensitive content from screenshots.
  // If extraction fails, reject the image rather than risk exposing sensitive
  // content.
  if (!snapshot.annotated_page_content_result.has_value()) {
    Respond(
        Error(base::StrCat({kApcExtractionErrorPrefix,
                            snapshot.annotated_page_content_result.error()})));
    return;
  }
  if (capture_screenshot && !snapshot.screenshot_result.has_value()) {
    Respond(Error(base::StrCat(
        {kScreenshotCaptureErrorPrefix, snapshot.screenshot_result.error()})));
    return;
  }

  // Protobuf serialization and lossless screenshot base64 can both be large.
  // Keep this linear work off the browser UI sequence.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SerializeSnapshot, std::move(result).value(), capture_apc,
                     capture_screenshot),
      base::BindOnce(
          &ExperimentalAiDataGetApcSnapshotFunction::OnSnapshotSerialized, this,
          capture_screenshot));
}

void ExperimentalAiDataGetApcSnapshotFunction::OnSnapshotSerialized(
    bool capture_screenshot,
    base::expected<base::ListValue, std::string> result) {
  if (!result.has_value()) {
    Respond(Error(std::move(result).error()));
    return;
  }
  // Recheck after asynchronous capture and serialization so a policy update
  // or replacement of the target page cannot expose a stale snapshot.
  if (auto error = GetDataCollectionError()) {
    Respond(Error(*error));
    return;
  }
  if (capture_screenshot) {
    auto screenshot_access = CheckScreenshotAccess(GetTargetWebContents());
    if (!screenshot_access.has_value()) {
      Respond(Error(ScreenshotAccessErrorToString(screenshot_access.error())));
      return;
    }
  }
  Respond(ArgumentList(std::move(result).value()));
}

}  // namespace extensions
