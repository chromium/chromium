// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"

#include "ash/webui/boca_ui/url_constants.h"
#include "ash/wm/window_pin_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/ash/components/boca/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

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

  // Include Boca URL in the SWA launch params so the downstream helper triggers
  // the specified callback on launch.
  SystemAppLaunchParams launch_params;
  launch_params.url = GURL(kChromeBocaAppUntrustedIndexURL);
  ash::LaunchSystemWebAppAsync(
      profile_, SystemWebAppType::BOCA, launch_params,
      /*window_info=*/nullptr,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             base::WeakPtr<OnTaskSystemWebAppManagerImpl> instance,
             apps::LaunchResult&& launch_result) {
            if (instance) {
              // Configure the browser window for OnTask. This is required to
              // ensure downstream components (especially UI controls) are setup
              // for locked mode transitions.
              const SessionID active_window_id =
                  instance->GetActiveSystemWebAppWindowID();
              Browser* const browser = GetBrowserWindowWithID(active_window_id);
              if (browser) {
                browser->SetLockedForOnTask(true);
              }
            }
            std::move(callback).Run(launch_result.state ==
                                    apps::LaunchResult::State::kSuccess);
          },
          std::move(callback), weak_ptr_factory_.GetWeakPtr()));
}

void OnTaskSystemWebAppManagerImpl::CloseSystemWebAppWindow(
    SessionID window_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser = GetBrowserWindowWithID(window_id);
  LockedSessionWindowTracker* const window_tracker = GetWindowTracker();
  if (window_tracker) {
    window_tracker->InitializeBrowserInfoForTracking(nullptr);
  }
  if (browser) {
    browser->window()->Close();
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

// TODO(b/367417612): Add unit test for this function.
void OnTaskSystemWebAppManagerImpl::SetWindowTrackerForSystemWebAppWindow(
    SessionID window_id,
    ActiveTabTracker* active_tab_tracker) {
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }
  LockedSessionWindowTracker* const window_tracker = GetWindowTracker();
  if (!window_tracker) {
    return;
  }
  window_tracker->InitializeBrowserInfoForTracking(browser);
  window_tracker->SetActiveTabTracker(active_tab_tracker);
}

SessionID OnTaskSystemWebAppManagerImpl::CreateBackgroundTabWithUrl(
    SessionID window_id,
    GURL url,
    OnTaskBlocklist::RestrictionLevel restriction_level) {
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return SessionID::InvalidValue();
  }
  LockedSessionWindowTracker* const window_tracker = GetWindowTracker();
  if (!window_tracker) {
    return SessionID::InvalidValue();
  }
  // Stop the window tracker while adding tabs before resuming it.
  window_tracker->set_can_start_navigation_throttle(false);
  NavigateParams navigate_params(browser, url, ui::PAGE_TRANSITION_FROM_API);
  navigate_params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&navigate_params);
  content::WebContents* const tab = navigation_handle->GetWebContents();
  window_tracker->on_task_blocklist()->SetParentURLRestrictionLevel(
      tab, url, restriction_level);
  window_tracker->set_can_start_navigation_throttle(true);
  return sessions::SessionTabHelper::IdForTab(tab);
}

void OnTaskSystemWebAppManagerImpl::RemoveTabsWithTabIds(
    SessionID window_id,
    const base::flat_set<SessionID>& tab_ids_to_remove) {
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }
  LockedSessionWindowTracker* const window_tracker = GetWindowTracker();
  if (!window_tracker) {
    return;
  }
  // Stop the window tracker while removing tabs before resuming it.
  window_tracker->set_can_start_navigation_throttle(false);
  // TODO (b/358197253): Add logic to prevent force closing tabs that are
  // actively being used by consumers.
  for (int idx = browser->tab_strip_model()->count() - 1; idx >= 0; --idx) {
    content::WebContents* const tab =
        browser->tab_strip_model()->GetWebContentsAt(idx);
    const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
    if (tab_ids_to_remove.contains(tab_id)) {
      browser->tab_strip_model()->DetachAndDeleteWebContentsAt(idx);
    }
  }
  window_tracker->set_can_start_navigation_throttle(true);
}

void OnTaskSystemWebAppManagerImpl::SetWindowTrackerForTesting(
    LockedSessionWindowTracker* window_tracker) {
  window_tracker_for_testing_ = window_tracker;
}

LockedSessionWindowTracker* OnTaskSystemWebAppManagerImpl::GetWindowTracker() {
  if (window_tracker_for_testing_) {
    return window_tracker_for_testing_;
  }
  return LockedSessionWindowTrackerFactory::GetForBrowserContext(profile_);
}

}  // namespace ash::boca
