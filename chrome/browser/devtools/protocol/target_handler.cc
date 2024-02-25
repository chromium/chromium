// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/target_handler.h"

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_browser_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"

namespace {
NavigateParams CreateNavigateParams(Profile* profile,
                                    const GURL& url,
                                    ui::PageTransition transition,
                                    bool new_window,
                                    bool background,
                                    Browser* browser) {
  DCHECK(new_window || browser);
  NavigateParams params(profile, url, transition);
  if (new_window) {
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
    if (background)
      params.window_action = NavigateParams::WindowAction::SHOW_WINDOW_INACTIVE;
  } else {
    params.disposition = (background)
                             ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                             : WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.browser = browser;
  }
  return params;
}
}  // namespace

TargetHandler::TargetHandler(protocol::UberDispatcher* dispatcher,
                             bool is_trusted)
    : is_trusted_(is_trusted) {
  protocol::Target::Dispatcher::wire(dispatcher, this);
}

TargetHandler::~TargetHandler() {
  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
}

protocol::Response TargetHandler::SetRemoteLocations(
    std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
        locations) {
  remote_locations_.clear();
  if (!locations)
    return protocol::Response::Success();

  for (const auto& location : *locations) {
    remote_locations_.insert(
        net::HostPortPair(location->GetHost(), location->GetPort()));
  }

  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
  return protocol::Response::Success();
}

protocol::Response TargetHandler::CreateTarget(
    const std::string& url,
    protocol::Maybe<int> width,
    protocol::Maybe<int> height,
    protocol::Maybe<std::string> browser_context_id,
    protocol::Maybe<bool> enable_begin_frame_control,
    protocol::Maybe<bool> new_window,
    protocol::Maybe<bool> background,
    protocol::Maybe<bool> for_tab,
    std::string* out_target_id) {
  Profile* profile = nullptr;
  if (browser_context_id.has_value()) {
    std::string profile_id = browser_context_id.value();
    profile =
        DevToolsBrowserContextManager::GetInstance().GetProfileById(profile_id);
    if (!profile) {
      return protocol::Response::ServerError(
          "Failed to find browser context with id " + profile_id);
    }
  } else {
    profile = ProfileManager::GetLastUsedProfile();
    DCHECK(profile);
  }

  bool create_new_window = new_window.value_or(false);
  bool create_in_background = background.value_or(false);
  Browser* target_browser = nullptr;

  // Must find target_browser if new_window not explicitly true.
  if (!create_new_window) {
    // Find a browser to open a new tab.
    // We shouldn't use browser that is scheduled to close.
    for (Browser* browser : *BrowserList::GetInstance()) {
      if (browser->profile() == profile &&
          !browser->IsAttemptingToCloseBrowser()) {
        target_browser = browser;
        break;
      }
    }
  }

  bool explicit_old_window = !new_window.value_or(true);
  if (explicit_old_window && !target_browser) {
    return protocol::Response::ServerError(
        "Failed to open new tab - "
        "no browser is open");
  }

  GURL gurl(url);
  if (gurl.is_empty()) {
    gurl = GURL(url::kAboutBlankURL);
  }

  if (!is_trusted_ && gurl.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return protocol::Response::ServerError(
        "Refusing to create a target with the specified URL");
  }

  create_new_window = !target_browser;
  NavigateParams params = CreateNavigateParams(
      profile, gurl, ui::PAGE_TRANSITION_AUTO_TOPLEVEL, create_new_window,
      create_in_background, target_browser);
  Navigate(&params);
  if (!params.navigated_or_inserted_contents)
    return protocol::Response::ServerError("Failed to open a new tab");

  if (!create_in_background) {
    params.navigated_or_inserted_contents->Focus();
  }

  if (for_tab.value_or(false)) {
    *out_target_id = content::DevToolsAgentHost::GetOrCreateForTab(
                         params.navigated_or_inserted_contents)
                         ->GetId();
  } else {
    *out_target_id = content::DevToolsAgentHost::GetOrCreateFor(
                         params.navigated_or_inserted_contents)
                         ->GetId();
  }
  return protocol::Response::Success();
}
