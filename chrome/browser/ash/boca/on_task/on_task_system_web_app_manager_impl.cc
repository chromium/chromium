// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/wm/window_pin_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/boca/on_task/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
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

void MakeWindowResizable(const BrowserWindow* window) {
  views::Widget* const widget =
      views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow());
  if (widget) {
    widget->widget_delegate()->SetCanResize(true);
  }
}
}  // namespace

OnTaskSystemWebAppManagerImpl::OnTaskSystemWebAppManagerImpl(Profile* profile)
    : profile_(profile) {}

OnTaskSystemWebAppManagerImpl::~OnTaskSystemWebAppManagerImpl() = default;

void OnTaskSystemWebAppManagerImpl::LaunchSystemWebAppAsync(
    base::OnceCallback<void(bool)> callback,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Include Boca URL in the SWA launch params so the downstream helper triggers
  // the specified callback on launch.
  SystemAppLaunchParams launch_params;
  launch_params.url = url;
  ash::LaunchSystemWebAppAsync(
      profile_, SystemWebAppType::BOCA, launch_params,
      /*window_info=*/nullptr,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             base::WeakPtr<OnTaskSystemWebAppManagerImpl> instance,
             apps::LaunchResult&& launch_result) {
            if (instance) {
              const SessionID active_window_id =
                  instance->GetActiveSystemWebAppWindowID();
              instance->PrepareSystemWebAppWindowForOnTask(
                  active_window_id, /*close_bundle_content=*/true);
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
    // Skips the tab unload process so that browser closes immediately.
    browser->set_force_skip_warning_user_on_close(true);
    browser->window()->Close();
  }
}

SessionID OnTaskSystemWebAppManagerImpl::GetActiveSystemWebAppWindowID() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO (b/354007279): Filter out SWA window instances that are not managed by
  // OnTask (for instance, those manually spawned by consumers).
  BrowserDelegate* const browser = FindSystemWebAppBrowser(
      profile_, SystemWebAppType::BOCA, BrowserType::kApp);
  // Verify that there is no browser instance and that there is no scheduled
  // task to delete the browser instance following window close.
  if (!browser || browser->IsClosing()) {
    return SessionID::InvalidValue();
  }
  return browser->GetSessionID();
}

void OnTaskSystemWebAppManagerImpl::SetPinStateForSystemWebAppWindow(
    bool pinned,
    SessionID window_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }

  // Verify window pin state before we exit fullscreen mode. This helps us
  // ensure we properly restore the window for subsequent updates.
  bool currently_pinned = platform_util::IsBrowserLockedFullscreen(browser);

  // Exit fullscreen mode if necessary. This is especially needed for certain
  // cases where the web app window is in fullscreen mode but not pinned, like
  // on session restore.
  auto* const fullscreen_controller =
      browser->exclusive_access_manager()->fullscreen_controller();
  if (fullscreen_controller->IsFullscreenForBrowser() && !pinned) {
    fullscreen_controller->ToggleBrowserFullscreenMode(
        /*user_initiated=*/false);
  }
  if (pinned == currently_pinned) {
    // Nothing to do.
    return;
  }

  // Move the browser window to the top of the layer tree before pinning or
  // unpinning it. This fixes a bug on tablets that results in no content being
  // rendered on pinning.
  browser->window()->Show();
  aura::Window* const native_window = browser->window()->GetNativeWindow();
  if (pinned) {
    PinWindow(native_window, /*trusted=*/true);
    browser->command_controller()->LockedFullscreenStateChanged();
    browser->window()->FocusToolbar();
  } else {
    UnpinWindow(native_window);
    browser->command_controller()->LockedFullscreenStateChanged();
    DisableCommandsForDevTools(window_id);
  }
}

void OnTaskSystemWebAppManagerImpl::DisableCommandsForDevTools(
    SessionID window_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }

  // TODO(crbug.com/406052710): Allow dev channel to use commands for devtools.
  chrome::BrowserCommandController* const command_controller =
      browser->command_controller();
  command_controller->UpdateCommandEnabled(IDC_DEV_TOOLS, false);
  command_controller->UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE, false);
  command_controller->UpdateCommandEnabled(IDC_DEV_TOOLS_DEVICES, false);
  command_controller->UpdateCommandEnabled(IDC_DEV_TOOLS_INSPECT, false);
  command_controller->UpdateCommandEnabled(IDC_DEV_TOOLS_TOGGLE, false);
}

void OnTaskSystemWebAppManagerImpl::SetPauseStateForSystemWebAppWindow(
    bool paused,
    SessionID window_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }

  if (paused) {
    // Focus on the boca homepage in pause mode.
    browser->tab_strip_model()->ActivateTabAt(0);

    // Cache current camera and microphone states before pausing, only if not
    // already cached.
    if (!was_camera_disabled_.has_value()) {
      auto* const camera_controller = ash::CameraPrivacySwitchController::Get();
      if (camera_controller) {
        was_camera_disabled_ = camera_controller->IsCameraAccessForceDisabled();
      }
    }
    if (!was_microphone_disabled_.has_value()) {
      auto* const audio_handler = ash::CrasAudioHandler::Get();
      if (audio_handler) {
        was_microphone_disabled_ = audio_handler->IsInputMuted();
      }
    }

    // Force pause camera and microphone inputs.
    PauseCameraInput(true);
    PauseMicrophoneInput(true);
  } else {
    // Restore inputs to their cached states only if previous states were
    // cached.
    if (was_camera_disabled_.has_value()) {
      PauseCameraInput(was_camera_disabled_.value());
    }
    if (was_microphone_disabled_.has_value()) {
      PauseMicrophoneInput(was_microphone_disabled_.value());
    }

    // Clear the cached states.
    was_camera_disabled_ = std::nullopt;
    was_microphone_disabled_ = std::nullopt;
  }
  EnableOrDisableCommandsForTabSwitch(window_id, !paused);

  // Configure SWA window and the OnTask pod for paused mode. This needs to be
  // done after switching tabs to ensure the pod is not visible when in paused
  // mode.
  LockedSessionWindowTracker* const window_tracker = GetWindowTracker();
  if (window_tracker) {
    window_tracker->OnPauseModeChanged(paused);
  }
}

void OnTaskSystemWebAppManagerImpl::EnableOrDisableCommandsForTabSwitch(
    SessionID window_id,
    bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }

  chrome::BrowserCommandController* const command_controller =
      browser->command_controller();
  command_controller->UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_TAB_0, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_TAB_1, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_TAB_2, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_TAB_3, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_TAB_4, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_TAB_5, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_TAB_6, enabled);
  command_controller->UpdateCommandEnabled(IDC_SELECT_TAB_7, enabled);
}

void OnTaskSystemWebAppManagerImpl::PauseCameraInput(bool paused) {
  auto* const camera_controller = ash::CameraPrivacySwitchController::Get();
  if (camera_controller) {
    camera_controller->SetForceDisableCameraAccess(paused);
  }
}

void OnTaskSystemWebAppManagerImpl::PauseMicrophoneInput(bool paused) {
  auto* const audio_handler = ash::CrasAudioHandler::Get();
  if (audio_handler) {
    audio_handler->SetInputMute(
        paused, ash::CrasAudioHandler::InputMuteChangeMethod::kOther);
  }
}

// TODO(b/367417612): Add unit test for this function.
void OnTaskSystemWebAppManagerImpl::SetWindowTrackerForSystemWebAppWindow(
    SessionID window_id,
    const std::vector<boca::BocaWindowObserver*> observers) {
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }
  LockedSessionWindowTracker* const window_tracker = GetWindowTracker();
  if (!window_tracker) {
    return;
  }
  window_tracker->InitializeBrowserInfoForTracking(browser);
  for (auto* observer : observers) {
    window_tracker->AddObserver(observer);
  }
}

SessionID OnTaskSystemWebAppManagerImpl::CreateBackgroundTabWithUrl(
    SessionID window_id,
    GURL url,
    ::boca::LockedNavigationOptions::NavigationType restriction_level) {
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
  Navigate(&navigate_params);
  content::WebContents* const tab =
      navigate_params.navigated_or_inserted_contents;
  window_tracker->on_task_blocklist()->SetParentURLRestrictionLevel(
      tab, url, restriction_level);
  window_tracker->set_can_start_navigation_throttle(true);
  return sessions::SessionTabHelper::IdForTab(tab);
}

void OnTaskSystemWebAppManagerImpl::SetParentTabsRestriction(
    SessionID window_id,
    ::boca::LockedNavigationOptions::NavigationType restriction_level) {
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }
  LockedSessionWindowTracker* const window_tracker = GetWindowTracker();
  if (!window_tracker) {
    return;
  }
  window_tracker->set_can_start_navigation_throttle(false);
  for (int idx = browser->tab_strip_model()->count() - 1; idx >= 0; --idx) {
    content::WebContents* const tab =
        browser->tab_strip_model()->GetWebContentsAt(idx);

    // Use the visible URL if the navigation has not been committed. It is
    // safe to do so because this is normally triggered when the app has been
    // launched.
    const GURL tab_url = tab->GetLastCommittedURL().is_empty()
                             ? tab->GetVisibleURL()
                             : tab->GetLastCommittedURL();
    window_tracker->on_task_blocklist()->SetParentURLRestrictionLevel(
        tab, tab_url, restriction_level);
  }
  window_tracker->set_can_start_navigation_throttle(true);
}

void OnTaskSystemWebAppManagerImpl::RemoveTabsWithTabIds(
    SessionID window_id,
    const std::set<SessionID>& tab_ids_to_remove) {
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

void OnTaskSystemWebAppManagerImpl::PrepareSystemWebAppWindowForOnTask(
    SessionID window_id,
    bool close_bundle_content) {
  Browser* const browser = GetBrowserWindowWithID(window_id);
  if (!browser) {
    return;
  }
  DisableCommandsForDevTools(window_id);

  // Configure the browser window for OnTask. This is required to ensure
  // downstream components (especially UI controls) are setup for locked mode
  // transitions.
  browser->SetLockedForOnTask(true);
  MakeWindowResizable(browser->window());
  browser->set_force_skip_warning_user_on_close(true);

  // Remove the floating button on the browser window for OnTask.
  aura::Window* const native_window = browser->window()->GetNativeWindow();
  native_window->SetProperty(chromeos::kSupportsFloatedStateKey, false);

  // Remove all tabs with pre-existing content if specified. This is to normally
  // de-dupe content and ensure that the tabs are set up for locked mode.
  if (close_bundle_content) {
    std::set<SessionID> tab_ids_to_remove;
    for (int idx = browser->tab_strip_model()->count() - 1; idx > 0; --idx) {
      content::WebContents* const tab =
          browser->tab_strip_model()->GetWebContentsAt(idx);
      const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
      tab_ids_to_remove.insert(tab_id);
    }
    RemoveTabsWithTabIds(window_id, tab_ids_to_remove);
  }
}

SessionID OnTaskSystemWebAppManagerImpl::GetActiveTabID() {
  const Browser* const browser =
      GetBrowserWindowWithID(GetActiveSystemWebAppWindowID());
  if (!browser) {
    return SessionID::InvalidValue();
  }
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(
      browser->tab_strip_model()->GetActiveWebContents());
  return tab_id;
}

void OnTaskSystemWebAppManagerImpl::SwitchToTab(SessionID tab_id) {
  Browser* const browser =
      GetBrowserWindowWithID(GetActiveSystemWebAppWindowID());
  if (!browser || !tab_id.is_valid()) {
    return;
  }
  for (int idx = browser->tab_strip_model()->count() - 1; idx >= 0; --idx) {
    content::WebContents* const tab =
        browser->tab_strip_model()->GetWebContentsAt(idx);
    const SessionID id = sessions::SessionTabHelper::IdForTab(tab);
    if (tab_id == id) {
      browser->tab_strip_model()->ActivateTabAt(idx);
      return;
    }
  }
}

void OnTaskSystemWebAppManagerImpl::SetAllChromeTabsMuted(bool muted) {
  Browser* const boca_browser =
      GetBrowserWindowWithID(GetActiveSystemWebAppWindowID());
  if (!boca_browser) {
    return;
  }
  for (Browser* const browser : *BrowserList::GetInstance()) {
    if (!browser || browser == boca_browser) {
      continue;
    }
    for (int idx = 0; idx < browser->tab_strip_model()->count(); ++idx) {
      content::WebContents* const tab =
          browser->tab_strip_model()->GetWebContentsAt(idx);
      if (tab) {
        tab->SetAudioMuted(muted);
      }
    }
  }
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
