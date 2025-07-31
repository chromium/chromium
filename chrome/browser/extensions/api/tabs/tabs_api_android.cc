// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include "base/notimplemented.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "url/gurl.h"

namespace extensions {

namespace windows = api::windows;
namespace tabs = api::tabs;

namespace {

constexpr char kNoActiveTab[] = "No active tab";
constexpr char kInvalidArguments[] = "Invalid arguments";
constexpr char kTabsNotImplemented[] = "chrome.tabs not implemented";
constexpr char kWindowsNotImplemented[] = "chrome.windows not implemented";

content::WebContents* GetActiveWebContents() {
  for (TabModel* tab_model : TabModelList::models()) {
    if (!tab_model->IsActiveModel()) {
      continue;
    }
    auto* web_contents = tab_model->GetActiveWebContents();
    if (!web_contents) {
      continue;
    }
    return web_contents;
  }
  return nullptr;
}

}  // namespace

api::tabs::Tab CreateTabObjectHelper(content::WebContents* contents,
                                     const Extension* extension,
                                     mojom::ContextType context) {
  auto scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension, context, contents);
  return ExtensionTabUtil::CreateTabObject(contents, scrub_tab_behavior,
                                           extension);
}

// Windows ---------------------------------------------------------------------

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {
  std::optional<windows::Create::Params> params =
      windows::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

ExtensionFunction::ResponseAction WindowsUpdateFunction::Run() {
  std::optional<windows::Update::Params> params =
      windows::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kWindowsNotImplemented));
}

// Tabs ------------------------------------------------------------------------

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  std::optional<tabs::Query::Params> params =
      tabs::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED() << "Using stub implementation";

  // If a URL pattern is specified, return tabs that match it.
  if (params->query_info.url) {
    return GetTabsMatchingUrl(*params);
  }

  // Otherwise, return the active tab.
  return GetActiveTab(*params);
}

ExtensionFunction::ResponseAction TabsQueryFunction::GetTabsMatchingUrl(
    const api::tabs::Query::Params& params) {
  // See TabsQueryFunction in tabs_api_non_android.cc for details.
  std::vector<std::string> url_pattern_strings;
  if (params.query_info.url->as_string) {
    url_pattern_strings.push_back(*params.query_info.url->as_string);
  } else if (params.query_info.url->as_strings) {
    url_pattern_strings = *params.query_info.url->as_strings;
  }
  // It is OK to use URLPattern::SCHEME_ALL here because this function does
  // not grant access to the content of the tabs, only to seeing their URLs
  // and meta data.
  URLPatternSet url_patterns;
  std::string error;
  if (!url_patterns.Populate(url_pattern_strings, URLPattern::SCHEME_ALL, true,
                             &error)) {
    return RespondNow(Error(std::move(error)));
  }

  // Return all tabs that match the URL pattern.
  base::Value::List result;
  for (TabModel* tab_model : TabModelList::models()) {
    for (int i = 0; i < tab_model->GetTabCount(); ++i) {
      auto* web_contents = tab_model->GetWebContentsAt(i);
      if (!web_contents) {
        continue;
      }
      if (url_patterns.MatchesURL(web_contents->GetVisibleURL())) {
        api::tabs::Tab tab_object = CreateTabObjectHelper(
            web_contents, extension(), source_context_type());
        result.Append(tab_object.ToValue());
      }
    }
  }
  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction TabsQueryFunction::GetActiveTab(
    const api::tabs::Query::Params& params) {
  base::Value::List result;
  auto* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return RespondNow(Error(kNoActiveTab));
  }
  api::tabs::Tab tab_object =
      CreateTabObjectHelper(web_contents, extension(), source_context_type());
  result.Append(tab_object.ToValue());
  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::optional<tabs::Create::Params> params =
      tabs::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!params) {
    return RespondNow(Error(kInvalidArguments));
  }
  NOTIMPLEMENTED() << "Using stub implementation and creating a new tab";

  // Get the tab model of the active tab.
  content::WebContents* parent = GetActiveWebContents();
  if (!parent) {
    return RespondNow(Error(kNoActiveTab));
  }
  TabModel* const tab_model = TabModelList::GetTabModelForWebContents(parent);
  CHECK_EQ(parent, tab_model->GetActiveWebContents());

  // Create a new tab.
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser_context()));
  CHECK(contents);
  content::WebContents* const second_web_contents = contents.release();
  tab_model->CreateTab(TabAndroid::FromWebContents(parent), second_web_contents,
                       /*select=*/true);

  // Kick off navigation. See `TabsUpdateFunction::UpdateURL` for how this is
  // done on Win/Mac/Linux.
  base::expected<GURL, std::string> url =
      ExtensionTabUtil::PrepareURLForNavigation(*params->create_properties.url,
                                                extension(), browser_context());
  if (!url.has_value()) {
    return RespondNow(Error(std::move(url.error())));
  }
  content::NavigationController::LoadURLParams load_params(*url);
  load_params.is_renderer_initiated = true;
  load_params.initiator_origin = extension()->origin();
  load_params.source_site_instance = content::SiteInstance::CreateForURL(
      parent->GetBrowserContext(), load_params.initiator_origin->GetURL());
  load_params.transition_type = ui::PAGE_TRANSITION_FROM_API;

  base::WeakPtr<content::NavigationHandle> navigation_handle =
      second_web_contents->GetController().LoadURLWithParams(load_params);
  if (!navigation_handle) {
    return RespondNow(Error("Navigation rejected."));
  }

  // Add the new tab object to the result.
  auto scrub_tab_behavior = ExtensionTabUtil::GetScrubTabBehavior(
      extension(), source_context_type(), second_web_contents);
  api::tabs::Tab tab_object = ExtensionTabUtil::CreateTabObject(
      second_web_contents, scrub_tab_behavior, extension());
  base::Value::Dict result = tab_object.ToValue();
  return RespondNow(has_callback() ? WithArguments(std::move(result))
                                   : NoArguments());
}

ExtensionFunction::ResponseAction TabsDuplicateFunction::Run() {
  std::optional<tabs::Duplicate::Params> params =
      tabs::Duplicate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  std::optional<tabs::Highlight::Params> params =
      tabs::Highlight::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

TabsUpdateFunction::TabsUpdateFunction() = default;

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  std::optional<tabs::Move::Params> params = tabs::Move::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsReloadFunction::Run() {
  std::optional<tabs::Reload::Params> params =
      tabs::Reload::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsGroupFunction::Run() {
  std::optional<tabs::Group::Params> params =
      tabs::Group::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

ExtensionFunction::ResponseAction TabsUngroupFunction::Run() {
  std::optional<tabs::Ungroup::Params> params =
      tabs::Ungroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

TabsDiscardFunction::TabsDiscardFunction() = default;
TabsDiscardFunction::~TabsDiscardFunction() = default;

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
  std::optional<tabs::Discard::Params> params =
      tabs::Discard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

}  // namespace extensions
