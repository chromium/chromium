// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/target_handler.h"

#include <ranges>
#include <string_view>

#include "base/notreached.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_browser_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace {
NavigateParams CreateNavigateParams(Profile* profile,
                                    const GURL& url,
                                    ui::PageTransition transition,
                                    bool new_window,
                                    bool background,
                                    BrowserWindowInterface* bwi) {
  Browser* browser = nullptr;
  if (!new_window && bwi) {
    browser = bwi->GetBrowserForMigrationOnly();
  }

  DCHECK(new_window || browser);
  NavigateParams params(profile, url, transition);
  if (new_window) {
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
    if (background)
      params.window_action = NavigateParams::WindowAction::kShowWindowInactive;
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
                             bool is_trusted,
                             bool may_read_local_files)
    : is_trusted_(is_trusted), may_read_local_files_(may_read_local_files) {
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
    std::optional<int> left,
    std::optional<int> top,
    std::optional<int> width,
    std::optional<int> height,
    std::optional<std::string> window_state,
    std::optional<std::string> browser_context_id,
    std::optional<bool> enable_begin_frame_control,
    std::optional<bool> new_window,
    std::optional<bool> background,
    std::optional<bool> for_tab,
    std::optional<bool> hidden,
    std::string* out_target_id) {
  if (hidden.value_or(false)) {
    // Rely on web contents implementation.
    return protocol::Response::FallThrough();
  }
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
  BrowserWindowInterface* target_browser_interface = nullptr;

  // Must find target_browser_interface if new_window not explicitly true.
  if (!create_new_window) {
    // Find a browser to open a new tab.
    // We shouldn't use browser that is scheduled to close.
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [profile, &target_browser_interface](
            BrowserWindowInterface* browser_window_interface) {
          if (browser_window_interface->GetProfile() == profile) {
            if (!browser_window_interface->capabilities()
                     ->IsAttemptingToCloseBrowser()) {
              target_browser_interface = browser_window_interface;
              return false;
            }
          }
          return true;
        });
  }

  bool explicit_old_window = !new_window.value_or(true);
  if (explicit_old_window && !target_browser_interface) {
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

  if (!may_read_local_files_ && gurl.SchemeIsFile()) {
    return protocol::Response::ServerError(
        "Creating a target with a local URL is not allowed");
  }

  create_new_window = !target_browser_interface;

  const bool set_window_position = left || top || width || height;
  if (set_window_position && !create_new_window) {
    return protocol::Response::InvalidParams(
        "Target position can only be set for new windows");
  }

  static std::string_view kActionableWindowStates[] = {
      protocol::Target::WindowStateEnum::Minimized,
      protocol::Target::WindowStateEnum::Maximized,
      protocol::Target::WindowStateEnum::Fullscreen,
  };

  bool set_window_state = !!window_state;
  if (set_window_state) {
    if (!create_new_window) {
      return protocol::Response::InvalidParams(
          "Target window state can only be set for new windows");
    }
    if (*window_state == protocol::Target::WindowStateEnum::Normal) {
      set_window_state = false;
    } else if (std::ranges::find(kActionableWindowStates, *window_state) ==
               std::end(kActionableWindowStates)) {
      return protocol::Response::InvalidParams("Invalid target window state: " +
                                               *window_state);
    }
  }

  NavigateParams params = CreateNavigateParams(
      profile, gurl, ui::PAGE_TRANSITION_AUTO_TOPLEVEL, create_new_window,
      create_in_background, target_browser_interface);

  Navigate(&params);
  if (!params.navigated_or_inserted_contents) {
    return protocol::Response::ServerError("Failed to open a new tab");
  }

  if (set_window_position) {
    ui::BaseWindow* browser_window = params.browser->GetWindow();
    CHECK(browser_window);
    gfx::Rect bounds = browser_window->GetBounds();
    if (left) {
      bounds.set_x(left.value());
    }
    if (top) {
      bounds.set_y(top.value());
    }
    if (width) {
      bounds.set_width(width.value());
    }
    if (height) {
      bounds.set_height(height.value());
    }
    browser_window->SetBounds(bounds);
  }

  if (set_window_state) {
    if (*window_state == protocol::Target::WindowStateEnum::Minimized) {
      params.browser->GetWindow()->Minimize();
    } else if (*window_state == protocol::Target::WindowStateEnum::Maximized) {
      params.browser->GetWindow()->Maximize();
    } else if (*window_state == protocol::Target::WindowStateEnum::Fullscreen) {
      params.browser->GetFeatures()
          .exclusive_access_manager()
          ->fullscreen_controller()
          ->ToggleBrowserFullscreenMode(/*user_initiated=*/false);
    } else {
      NOTREACHED();
    }
  }

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
