// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/target_handler.h"

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_browser_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/devtools_agent_host.h"

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

TargetHandler::TargetHandler(protocol::UberDispatcher* dispatcher) {
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
    return protocol::Response::OK();

  for (const auto& location : *locations) {
    remote_locations_.insert(
        net::HostPortPair(location->GetHost(), location->GetPort()));
  }

  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
  return protocol::Response::OK();
}

protocol::Response TargetHandler::CreateTarget(
    const std::string& url,
    protocol::Maybe<int> width,
    protocol::Maybe<int> height,
    protocol::Maybe<std::string> browser_context_id,
    protocol::Maybe<bool> enable_begin_frame_control,
    protocol::Maybe<bool> new_window,
    protocol::Maybe<bool> background,
    std::string* out_target_id) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (browser_context_id.isJust()) {
    std::string profile_id = browser_context_id.fromJust();
    profile =
        DevToolsBrowserContextManager::GetInstance().GetProfileById(profile_id);
    if (!profile) {
      return protocol::Response::Error(
          "Failed to find browser context with id " + profile_id);
    }
  }
  bool create_new_window = new_window.fromMaybe(false);
  bool create_in_background = background.fromMaybe(false);
  Browser* target_browser = nullptr;

  // Must find target_browser if new_window not explicitly true.
  if (!create_new_window) {
    // Find a browser to open a new tab.
    // We shouldn't use browser that is scheduled to close.
    for (auto* browser : *BrowserList::GetInstance()) {
      if (browser->profile() == profile &&
          !browser->IsAttemptingToCloseBrowser()) {
        target_browser = browser;
        break;
      }
    }
  }

  bool explicit_old_window = !new_window.fromMaybe(true);
  if (explicit_old_window && !target_browser) {
    return protocol::Response::Error(
        "Failed to open new tab - "
        "no browser is open");
  }

  create_new_window = !target_browser;
  NavigateParams params = CreateNavigateParams(
      profile, GURL(url), ui::PAGE_TRANSITION_AUTO_TOPLEVEL, create_new_window,
      create_in_background, target_browser);
  Navigate(&params);
  if (!params.navigated_or_inserted_contents)
    return protocol::Response::Error("Failed to open a new tab");

  *out_target_id = content::DevToolsAgentHost::GetOrCreateFor(
                       params.navigated_or_inserted_contents)
                       ->GetId();
  return protocol::Response::OK();
}
