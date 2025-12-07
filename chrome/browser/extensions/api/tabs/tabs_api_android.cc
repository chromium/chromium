// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include "base/notimplemented.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/navigation_controller.h"
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

// Tabs ------------------------------------------------------------------------

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

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  std::optional<tabs::Highlight::Params> params =
      tabs::Highlight::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error(kTabsNotImplemented));
}

TabsUpdateFunction::TabsUpdateFunction() = default;

// The first half of this function is identical to tabs_api_non_android.cc.
// The rest is simplified so that it can be supported on Android.
ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::optional<tabs::Update::Params> params =
      tabs::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Compute the tab ID if it was not provided. This is identical to
  // tabs_api_non_android.cc.
  int tab_id = -1;
  content::WebContents* contents = nullptr;
  if (!params->tab_id) {
    WindowController* window_controller =
        ChromeExtensionFunctionDetails(this).GetCurrentWindowController();
    if (!window_controller) {
      return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
    }
    if (!ExtensionTabUtil::IsTabStripEditable()) {
      return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
    }
    contents = window_controller->GetActiveTab();
    if (!contents) {
      return RespondNow(Error(tabs_constants::kNoSelectedTabError));
    }
    tab_id = ExtensionTabUtil::GetTabId(contents);
  } else {
    tab_id = *params->tab_id;
  }

  // Find the window, tab index, and web contents. This is identical to
  // tabs_api_non_android.cc.
  int tab_index = -1;
  WindowController* window = nullptr;
  std::string error;
  if (!tabs_internal::GetTabById(tab_id, browser_context(),
                                 include_incognito_information(), &window,
                                 &contents, &tab_index, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  if (DevToolsWindow::IsDevToolsWindow(contents)) {
    return RespondNow(Error(tabs_constants::kNotAllowedForDevToolsError));
  }

  // tabs_internal::GetTabById may return a null window for prerender tabs.
  // This is identical to tabs_api_non_android.cc.
  if (!window || !window->SupportsTabs()) {
    return RespondNow(Error(ExtensionTabUtil::kNoCurrentWindowError));
  }

  // Cache the web contents.
  content::WebContents* original_contents = contents;

  // Because this is a stub, the only parameter we update is the URL (and hence
  // navigate to a new page). This is the most common usage in tests. For other
  // properties, return an error.
  if (params->update_properties.selected) {
    return RespondNow(Error("Updating selected property not supported."));
  }
  if (params->update_properties.active) {
    return RespondNow(Error("Updating active property not supported."));
  }
  if (params->update_properties.highlighted) {
    return RespondNow(Error("Updating highlighted property not supported."));
  }
  if (params->update_properties.opener_tab_id) {
    return RespondNow(Error("Updating opener_tab_id property not supported."));
  }
  if (params->update_properties.auto_discardable) {
    return RespondNow(
        Error("Updating auto_discardable property not supported."));
  }
  if (params->update_properties.muted) {
    return RespondNow(Error("Updating muted property not supported."));
  }
  if (params->update_properties.pinned) {
    return RespondNow(Error("Updating pinned property not supported."));
  }

  // Navigate the tab to a new location if the url is different.
  if (params->update_properties.url) {
    std::string updated_url = *params->update_properties.url;

    // Get last committed or pending URL.
    std::string current_url = contents->GetVisibleURL().is_valid()
                                  ? contents->GetVisibleURL().spec()
                                  : std::string();

    // See tabs_api.cc for the implementation of UpdateURL().
    if (!UpdateURL(original_contents, updated_url, tab_id, &error)) {
      return RespondNow(Error(std::move(error)));
    }
  }

  // See tabs_api.cc for the implementation of GetResult().
  return RespondNow(GetResult(original_contents));
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
