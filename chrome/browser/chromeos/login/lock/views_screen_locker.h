// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/lock_screen_apps/focus_cycler_delegate.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chromeos/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/dbus/media_perception/media_perception.pb.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace chromeos {

class UserBoardViewMojo;
class UserSelectionScreen;
class MojoSystemInfoDispatcher;

// ViewsScreenLocker acts like LoginScreenClient::Delegate which handles method
// calls coming from ash into chrome.
// It is also a ScreenLocker::Delegate which handles calls from chrome into
// ash (views-based lockscreen).
class ViewsScreenLocker : public LoginScreenClient::Delegate,
                          public ScreenLocker::Delegate,
                          public PowerManagerClient::Observer,
                          public lock_screen_apps::FocusCyclerDelegate,
                          public chromeos::MediaAnalyticsClient::Observer {
 public:
  explicit ViewsScreenLocker(ScreenLocker* screen_locker);
  ~ViewsScreenLocker() override;

  void Init();

  // ScreenLocker::Delegate:
  void ShowErrorMessage(int error_msg_id,
                        HelpAppLauncher::HelpTopic help_topic_id) override;
  void ClearErrors() override;
  void OnAshLockAnimationFinished() override;

  // LoginScreenClient::Delegate
  void HandleAuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void HandleAuthenticateUserWithExternalBinary(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void HandleEnrollUserWithExternalBinary(
      base::OnceCallback<void(bool)> callback) override;
  void HandleAuthenticateUserWithEasyUnlock(
      const AccountId& account_id) override;
  void HandleAuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void HandleHardlockPod(const AccountId& account_id) override;
  void HandleOnFocusPod(const AccountId& account_id) override;
  void HandleOnNoPodFocused() override;
  bool HandleFocusLockScreenApps(bool reverse) override;
  void HandleFocusOobeDialog() override;
  void HandleLaunchPublicSession(const AccountId& account_id,
                                 const std::string& locale,
                                 const std::string& input_method) override;

  // PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // lock_screen_apps::FocusCyclerDelegate:
  void RegisterLockScreenAppFocusHandler(
      const LockScreenAppFocusCallback& focus_handler) override;
  void UnregisterLockScreenAppFocusHandler() override;
  void HandleLockScreenAppFocusOut(bool reverse) override;

  // chromeos::MediaAnalyticsClient::Observer
  void OnDetectionSignal(const mri::MediaPerception& media_perception) override;

 private:
  void UpdatePinKeyboardState(const AccountId& account_id);
  void UpdateChallengeResponseAuthAvailability(const AccountId& account_id);
  void OnAllowedInputMethodsChanged();
  void OnPinCanAuthenticate(const AccountId& account_id, bool can_authenticate);
  void OnExternalBinaryAuthTimeout();
  void OnExternalBinaryEnrollmentTimeout();

  std::unique_ptr<UserBoardViewMojo> user_board_view_mojo_;
  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  // The ScreenLocker that owns this instance.
  ScreenLocker* const screen_locker_ = nullptr;

  // Time when lock was initiated, required for metrics.
  base::TimeTicks lock_time_;

  base::Optional<AccountId> focused_pod_account_id_;

  // Input Method Engine state used at lock screen.
  scoped_refptr<input_method::InputMethodManager::State> ime_state_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      allowed_input_methods_subscription_;

  base::OnceCallback<void(bool)> authenticate_with_external_binary_callback_;

  base::OnceCallback<void(bool)> enroll_user_with_external_binary_callback_;

  // Callback registered as a lock screen apps focus handler - it should be
  // called to hand focus over to lock screen apps.
  LockScreenAppFocusCallback lock_screen_app_focus_handler_;

  // Fetches system information and sends it to the UI over mojo.
  std::unique_ptr<MojoSystemInfoDispatcher> system_info_updater_;

  chromeos::MediaAnalyticsClient* media_analytics_client_;

  // Timer for external binary auth/enrollment attempt. Allows repeated attempts
  // up to a specific timeout.
  base::OneShotTimer external_binary_timer_;

  ScopedObserver<chromeos::MediaAnalyticsClient,
                 chromeos::MediaAnalyticsClient::Observer>
      scoped_observer_{this};

  base::WeakPtrFactory<ViewsScreenLocker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ViewsScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_
