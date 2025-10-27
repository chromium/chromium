// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_IMPL_H_

#include <optional>

#include "ash/webui/boca_ui/url_constants.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "url/gurl.h"

// Forward declaration of the browser profile and `SessionID`.
class Profile;
class SessionID;

namespace ash::boca {
class BocaWindowObserver;

// `OnTaskSystemWebAppManager` implementation that is essentially a thin wrapper
// around SWA window management APIs, specifically launch, close, and window
// pinning.
class OnTaskSystemWebAppManagerImpl : public OnTaskSystemWebAppManager {
 public:
  explicit OnTaskSystemWebAppManagerImpl(Profile* profile);
  ~OnTaskSystemWebAppManagerImpl() override;

  // OnTaskSystemWebAppManager:
  void LaunchSystemWebAppAsync(
      base::OnceCallback<void(bool)> callback,
      const GURL& url = GURL(kChromeBocaAppUntrustedIndexURL)) override;
  void CloseSystemWebAppWindow(SessionID window_id) override;
  SessionID GetActiveSystemWebAppWindowID() override;
  void SetPinStateForSystemWebAppWindow(bool pinned,
                                        SessionID window_id) override;
  void SetPauseStateForSystemWebAppWindow(bool paused,
                                          SessionID window_id) override;
  void SetWindowTrackerForSystemWebAppWindow(
      SessionID window_id,
      const std::vector<BocaWindowObserver*> observers) override;
  SessionID CreateBackgroundTabWithUrl(
      SessionID window_id,
      GURL url,
      ::boca::LockedNavigationOptions::NavigationType restriction_level)
      override;
  void SetParentTabsRestriction(SessionID window_id,
                                ::boca::LockedNavigationOptions::NavigationType
                                    restriction_level) override;
  void RemoveTabsWithTabIds(
      SessionID window_id,
      const std::set<SessionID>& tab_ids_to_remove) override;
  void PrepareSystemWebAppWindowForOnTask(SessionID window_id,
                                          bool close_bundle_content) override;
  SessionID GetActiveTabID() override;
  void SwitchToTab(SessionID tab_id) override;
  void SetAllChromeTabsMuted(bool muted) override;

  void SetWindowTrackerForTesting(LockedSessionWindowTracker* window_tracker);

 private:
  LockedSessionWindowTracker* GetWindowTracker();

  void DisableCommandsForDevTools(SessionID window_id);

  void EnableOrDisableCommandsForTabSwitch(SessionID window_id, bool enabled);

  void PauseCameraInput(bool paused);

  void PauseMicrophoneInput(bool paused);

  raw_ptr<Profile> profile_;

  std::optional<bool> was_camera_disabled_;

  std::optional<bool> was_microphone_disabled_;

  raw_ptr<LockedSessionWindowTracker> window_tracker_for_testing_;

  base::WeakPtrFactory<OnTaskSystemWebAppManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_IMPL_H_
