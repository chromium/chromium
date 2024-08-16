// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chromeos/window_pin_util.h"
#include "content/public/browser/browser_thread.h"

namespace ash::boca {
namespace {

// Returns a pointer to the browser window with the specified id. Returns
// nullptr if there is no match.
Browser* GetBrowserWindowWithID(SessionID window_id) {
  if (!window_id.is_valid()) {
    return nullptr;
  }
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->session_id() == window_id) {
      return browser;
    }
  }

  // No window found with specified ID.
  return nullptr;
}
}  // namespace

OnTaskSystemWebAppManagerImpl::OnTaskSystemWebAppManagerImpl(Profile* profile)
    : profile_(profile) {}

OnTaskSystemWebAppManagerImpl::~OnTaskSystemWebAppManagerImpl() = default;

void OnTaskSystemWebAppManagerImpl::LaunchSystemWebAppAsync(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::LaunchSystemWebAppAsync(
      profile_, SystemWebAppType::BOCA, SystemAppLaunchParams(),
      /*window_info=*/nullptr,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             apps::LaunchResult&& launch_result) {
            std::move(callback).Run(launch_result.state ==
                                    apps::LaunchResult::State::kSuccess);
          },
          std::move(callback)));
}

void OnTaskSystemWebAppManagerImpl::CloseSystemWebAppWindow(
    SessionID window_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser = GetBrowserWindowWithID(window_id);
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile_);
  if (window_tracker) {
    window_tracker->InitializeBrowserInfoForTracking(nullptr);
  }
  if (browser) {
    browser->TryToCloseWindow(/*skip_beforeunload=*/true, base::DoNothing());
  }
}

SessionID OnTaskSystemWebAppManagerImpl::GetActiveSystemWebAppWindowID() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO (b/354007279): Filter out SWA window instances that are not managed by
  // OnTask (for instance, those manually spawned by consumers).
  Browser* const browser =
      FindSystemWebAppBrowser(profile_, SystemWebAppType::BOCA);
  if (!browser) {
    return SessionID::InvalidValue();
  }
  return browser->session_id();
}

void OnTaskSystemWebAppManagerImpl::SetPinStateForSystemWebAppWindow(
    bool pinned,
    SessionID window_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }
  aura::Window* const native_window = browser->window()->GetNativeWindow();
  bool currently_pinned = IsWindowPinned(native_window);
  if (pinned == currently_pinned) {
    // Nothing to do.
    return;
  }
  if (pinned) {
    PinWindow(native_window, /*trusted=*/true);
    browser->command_controller()->LockedFullscreenStateChanged();
  } else {
    UnpinWindow(native_window);
    browser->command_controller()->LockedFullscreenStateChanged();
  }
}

void OnTaskSystemWebAppManagerImpl::SetWindowTrackerForSystemWebAppWindow(
    SessionID window_id) {
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }
  LockedSessionWindowTracker* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile_);
  if (!window_tracker) {
    return;
  }
  window_tracker->InitializeBrowserInfoForTracking(browser);
}

}  // namespace ash::boca
