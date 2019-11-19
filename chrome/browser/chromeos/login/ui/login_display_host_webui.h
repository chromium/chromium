// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_WEBUI_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_WEBUI_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/multi_user_window_manager_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/oobe_configuration.h"
#include "chrome/browser/chromeos/login/signin_screen_controller.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_common.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace ash {
class FocusRingController;
}

namespace chromeos {

class LoginDisplayWebUI;
class WebUILoginView;

// An implementation class for OOBE and user adding screen host via WebUI.
// For OOBE, it provides wizard screens such as welcome, network, EULA, update,
// GAIA etc. For user adding, it is legacy support and provides the user
// selection screen (aka account picker).
// The WebUI (chrome://oobe) is loaded hidden on start and made visible when
// WebUI signals ready (via NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE) or there
// is a network error (via NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN).
class LoginDisplayHostWebUI : public LoginDisplayHostCommon,
                              public content::WebContentsObserver,
                              public chromeos::SessionManagerClient::Observer,
                              public chromeos::CrasAudioHandler::AudioObserver,
                              public chromeos::OobeConfiguration::Observer,
                              public display::DisplayObserver,
                              public ui::InputDeviceEventObserver,
                              public views::WidgetRemovalsObserver,
                              public views::WidgetObserver,
                              public ash::MultiUserWindowManagerObserver {
 public:
  LoginDisplayHostWebUI();
  ~LoginDisplayHostWebUI() override;

  // LoginDisplayHost:
  LoginDisplay* GetLoginDisplay() override;
  ExistingUserController* GetExistingUserController() override;
  gfx::NativeWindow GetNativeWindow() const override;
  OobeUI* GetOobeUI() const override;
  content::WebContents* GetOobeWebContents() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void OnFinalize() override;
  void SetStatusAreaVisible(bool visible) override;
  void StartWizard(OobeScreenId first_screen) override;
  WizardController* GetWizardController() override;
  void OnStartUserAdding() override;
  void CancelUserAdding() override;
  void OnStartSignInScreen(const LoginScreenContext& context) override;
  void OnPreferencesChanged() override;
  void OnStartAppLaunch() override;
  void OnStartArcKiosk() override;
  void OnStartWebKiosk() override;
  void OnBrowserCreated() override;
  void ShowGaiaDialog(bool can_close,
                      const AccountId& prefilled_account) override;
  void HideOobeDialog() override;
  void UpdateOobeDialogState(ash::OobeDialogState state) override;
  const user_manager::UserList GetUsers() override;
  void ShowFeedback() override;
  void ShowResetScreen() override;
  void HandleDisplayCaptivePortal() override;
  void UpdateAddUserButtonStatus() override;
  void RequestSystemInfoUpdate() override;

  void OnCancelPasswordChangedFlow() override;

  // Trace id for ShowLoginWebUI event (since there exists at most one login
  // WebUI at a time).
  static const int kShowLoginWebUIid;

  views::Widget* login_window_for_test() { return login_window_; }

  // Disable GaiaScreenHandler restrictive proxy check.
  static void DisableRestrictiveProxyCheckForTest();

 protected:
  class KeyboardDrivenOobeKeyHandler;

  // LoginDisplayHost:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;

  // chromeos::SessionManagerClient::Observer:
  void EmitLoginPromptVisibleCalled() override;

  // chromeos::OobeConfiguration::Observer:
  void OnOobeConfigurationChanged() override;

  // chromeos::CrasAudioHandler::AudioObserver:
  void OnActiveOutputNodeChanged() override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ui::InputDeviceEventObserver
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // views::WidgetRemovalsObserver:
  void OnWillRemoveView(views::Widget* widget, views::View* view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // ash::MultiUserWindowManagerObserver:
  void OnUserSwitchAnimationFinished() override;

 private:
  // Way to restore if renderer have crashed.
  enum RestorePath {
    RESTORE_UNKNOWN,
    RESTORE_WIZARD,
    RESTORE_SIGN_IN,
    RESTORE_ADD_USER_INTO_SESSION,
  };

  // Type of animations to run after the login screen.
  enum FinalizeAnimationType {
    ANIMATION_NONE,       // No animation.
    ANIMATION_WORKSPACE,  // Use initial workspace animation (drop and
                          // and fade in workspace). Used for user login.
    ANIMATION_FADE_OUT,   // Fade out login screen. Used for app launch.
    ANIMATION_ADD_USER,   // Use UserSwitchAnimatorChromeOS animation when
                          // adding a user into multi-profile session.
  };

  // Schedules workspace transition animation.
  void ScheduleWorkspaceAnimation();

  // Schedules fade out animation.
  void ScheduleFadeOutAnimation(int animation_speed_ms);

  // Loads given URL. Creates WebUILoginView if needed.
  void LoadURL(const GURL& url);

  // Shows OOBE/sign in WebUI that was previously initialized in hidden state.
  void ShowWebUI();

  // Initializes |login_window_| and |login_view_| fields if needed.
  void InitLoginWindowAndView();

  // Closes |login_window_| and resets |login_window_| and |login_view_| fields.
  void ResetLoginWindowAndView();

  // Toggles OOBE progress bar visibility, the bar is hidden by default.
  void SetOobeProgressBarVisible(bool visible);

  // Tries to play startup sound. If sound can't be played right now,
  // for instance, because cras server is not initialized, playback
  // will be delayed.
  void TryToPlayOobeStartupSound();

  // Called when login-prompt-visible signal is caught.
  void OnLoginPromptVisible();

  // Creates or recreates |existing_user_controller_|.
  void CreateExistingUserController();

  // Plays startup sound if needed and audio device is ready.
  void PlayStartupSoundIfPossible();

  // Sign in screen controller.
  std::unique_ptr<ExistingUserController> existing_user_controller_;

  // OOBE and some screens (camera, recovery) controller.
  std::unique_ptr<WizardController> wizard_controller_;

  std::unique_ptr<SignInScreenController> signin_screen_controller_;

  // Whether progress bar is shown on the OOBE page.
  bool oobe_progress_bar_visible_ = false;

  // Container of the screen we are displaying.
  views::Widget* login_window_ = nullptr;

  // Container of the view we are displaying.
  WebUILoginView* login_view_ = nullptr;

  // Login display we are using.
  std::unique_ptr<LoginDisplayWebUI> login_display_;

  // True if the login display is the current screen.
  bool is_showing_login_ = false;

  // Stores status area current visibility to be applied once login WebUI
  // is shown.
  bool status_area_saved_visibility_ = false;

  // True if WebUI is initialized in hidden state, the OOBE is not completed
  // and we're waiting for OOBE configuration check to finish.
  bool waiting_for_configuration_ = false;

  static bool disable_restrictive_proxy_check_for_test_;

  // How many times renderer has crashed.
  int crash_count_ = 0;

  // Way to restore if renderer have crashed.
  RestorePath restore_path_ = RESTORE_UNKNOWN;

  // Stored parameters for StartWizard, required to restore in case of crash.
  OobeScreenId first_screen_ = OobeScreen::SCREEN_UNKNOWN;

  // A focus ring controller to draw focus ring around view for keyboard
  // driven oobe.
  std::unique_ptr<ash::FocusRingController> focus_ring_controller_;

  // Handles special keys for keyboard driven oobe.
  std::unique_ptr<KeyboardDrivenOobeKeyHandler>
      keyboard_driven_oobe_key_handler_;

  FinalizeAnimationType finalize_animation_type_ = ANIMATION_WORKSPACE;

  // Time when login prompt visible signal is received. Used for
  // calculations of delay before startup sound.
  base::TimeTicks login_prompt_visible_time_;

  // True when request to play startup sound was sent to
  // SoundsManager.
  // After OOBE is completed, this is always initialized with true.
  bool oobe_startup_sound_played_ = false;

  // True if we need to play startup sound when audio device becomes available.
  bool need_to_play_startup_sound_ = false;

  base::WeakPtrFactory<LoginDisplayHostWebUI> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostWebUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_WEBUI_H_
