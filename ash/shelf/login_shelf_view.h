// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_LOGIN_SHELF_VIEW_H_
#define ASH_SHELF_LOGIN_SHELF_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller.h"
#include "ash/lock_screen_action/lock_screen_action_background_observer.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/scoped_guest_button_blocker.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/kiosk_app_instruction_bubble.h"
#include "ash/shelf/shelf_shutdown_confirmation_bubble.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/tray_action/tray_action.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}

namespace session_manager {
enum class SessionState;
}

namespace ash {

enum class LockScreenActionBackgroundState;

class LoginShelfButton;
class KioskAppsButton;
class TrayBackgroundView;

// LoginShelfView contains the shelf buttons visible outside of an active user
// session. ShelfView and LoginShelfView should never be shown together.
class ASH_EXPORT LoginShelfView : public views::View,
                                  public TrayActionObserver,
                                  public LockScreenActionBackgroundObserver,
                                  public ShutdownControllerImpl::Observer,
                                  public LoginDataDispatcher::Observer,
                                  public EnterpriseDomainObserver,
                                  public ShelfConfig::Observer {
  METADATA_HEADER(LoginShelfView, views::View)

 public:
  enum ButtonId {
    kShutdown = 1,          // Shut down the device.
    kRestart,               // Restart the device.
    kSignOut,               // Sign out the active user session.
    kCloseNote,             // Close the lock screen note.
    kCancel,                // Cancel multiple user sign-in.
    kBrowseAsGuest,         // Use in guest mode.
    kAddUser,               // Add a new user.
    kApps,                  // Show list of available kiosk apps.
    kParentAccess,          // Unlock child device with Parent Access Code.
    kEnterpriseEnrollment,  // Start enterprise enrollment flow.
    kSignIn,                // Start signin.
    kOsInstall,             // Start OS Install flow.
    kSchoolEnrollment,      // Start enterprise enrollment flow for child setup.
  };

  // Stores and notifies UiUpdate test callbacks.
  class TestUiUpdateDelegate {
   public:
    virtual ~TestUiUpdateDelegate();
    virtual void OnUiUpdate() = 0;
  };

  explicit LoginShelfView(
      LockScreenActionBackgroundController* lock_screen_action_background);

  LoginShelfView(const LoginShelfView&) = delete;
  LoginShelfView& operator=(const LoginShelfView&) = delete;

  ~LoginShelfView() override;

  // ShelfWidget observes SessionController for higher-level UI changes and
  // then notifies LoginShelfView to update its own UI.
  void UpdateAfterSessionChange();

  // Sets the contents of the kiosk app menu.
  void SetKioskApps(const std::vector<KioskAppMenuEntry>& kiosk_apps);

  // Sets the callback used when a menu item is selected, as well as when the
  // kiosk menu is opened.
  void ConfigureKioskCallbacks(
      const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app,
      const base::RepeatingClosure& on_show_menu);

  // Sets the state of the login dialog.
  void SetLoginDialogState(OobeDialogState state);

  // Sets if the guest button on the login shelf can be shown. Even if set to
  // true the button may still not be visible.
  void SetAllowLoginAsGuest(bool allow_guest);

  // Sets whether parent access button can be shown on the login shelf.
  void ShowParentAccessButton(bool show);

  // Sets if the guest button and apps button on the login shelf can be
  // shown during gaia signin screen.
  void SetIsFirstSigninStep(bool is_first);

  // Sets whether users can be added from the login screen.
  void SetAddUserButtonEnabled(bool enable_add_user);

  // Sets whether shutdown button is enabled in the login screen.
  void SetShutdownButtonEnabled(bool enable_shutdown_button);

  // Disable shelf buttons and tray buttons temporarily and enable them back
  // later. It could be used for temporary disable due to opened modal dialog.
  void SetButtonEnabled(bool enabled);

  // Sets and animates the opacity of login shelf buttons.
  void SetButtonOpacity(float target_opacity);

  // Test API. Set device to have kiosk license.
  void SetKioskLicenseModeForTesting(bool is_kiosk_license_mode);

  // views::View:
  void AddedToWidget() override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // Test API. Returns true if request was successful (i.e. button was
  // clickable).
  bool LaunchAppForTesting(const std::string& app_id);
  bool LaunchAppForTesting(const AccountId& account_id);

  // Adds test delegate. Delegate will become owned by LoginShelfView.
  void InstallTestUiUpdateDelegate(
      std::unique_ptr<TestUiUpdateDelegate> delegate);

  TestUiUpdateDelegate* test_ui_update_delegate() {
    return test_ui_update_delegate_.get();
  }

  // Returns scoped object to temporarily block Browse as Guest login button.
  std::unique_ptr<ScopedGuestButtonBlocker> GetScopedGuestButtonBlocker();

  // Returns the button container.
  views::View* GetButtonContainerByID(ButtonId button_id);

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

  // LockScreenActionBackgroundObserver:
  void OnLockScreenActionBackgroundStateChanged(
      LockScreenActionBackgroundState state) override;

  // ShutdownControllerImpl::Observer:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(const std::vector<LoginUserInfo>& users) override;
  void OnOobeDialogStateChanged(OobeDialogState state) override;

  // ash::EnterpriseDomainObserver
  void OnDeviceEnterpriseInfoChanged() override;
  void OnEnterpriseAccountDomainChanged() override;

  // Called when a locale change is detected. Updates the login shelf button
  // strings.
  void HandleLocaleChange();

  // Returns the Kiosk instruction bubble.
  KioskAppInstructionBubble* GetKioskInstructionBubbleForTesting();

  // Returns the shutdown confirmation bubble.
  ShelfShutdownConfirmationBubble* GetShutdownConfirmationBubbleForTesting();

  LoginShelfButton* GetLoginShelfButtonByID(ButtonId button_id);

 private:
  class ScopedGuestButtonBlockerImpl;

  bool LockScreenActionBackgroundAnimating() const;

  // Updates the visibility of buttons based on state changes, e.g. shutdown
  // policy updates, session state changes etc.
  void UpdateUi();

  // Updates the colors of all buttons. Uses current theme colors and force
  // light colors during OOBE.
  void UpdateButtonsColors();

  // Updates the total bounds of all buttons.
  void UpdateButtonUnionBounds();

  // Callback functions of the buttons on the shutdown confirmation bubble.
  // If confirmed, the confirmation bubble would go hidden and the device would
  // shutdown. If cancelled, the bubble would go hidden.
  void OnRequestShutdownConfirmed();
  void OnRequestShutdownCancelled();

  // RequestShutdown is triggered by the shutdown button. If the feature flag
  // kShutdownConfirmationDialog is enabled, a shutdown confirmation bubble
  // would appear. If not, the device would shutdown immediately.
  void RequestShutdown();

  bool ShouldShowShutdownButton() const;

  bool ShouldShowGuestButton() const;

  bool ShouldShowEnterpriseEnrollmentButton() const;

  bool ShouldShowSchoolEnrollmentButton() const;

  bool ShouldShowSignInButton() const;

  bool ShouldShowAddUserButton() const;

  bool ShouldShowAppsButton() const;

  bool ShouldShowGuestAndAppsButtons() const;

  bool ShouldShowOsInstallButton() const;

  void SetButtonVisible(ButtonId id, bool visible);

  // Helper function which calls `closure` when device display is on. Or if the
  // number of dropped calls exceeds 'kMaxDroppedCallsWhenDisplaysOff'
  void CallIfDisplayIsOn(const base::RepeatingClosure& closure);

  // Helper function which calls on_kiosk_menu_shown when kiosk menu is shown.
  void OnKioskMenuShown(const base::RepeatingClosure& on_kiosk_menu_shown);
  void OnKioskMenuclosed();

  void OnAddUserButtonClicked();

  OobeDialogState dialog_state_ = OobeDialogState::HIDDEN;
  bool allow_guest_ = true;
  bool is_first_signin_step_ = false;
  bool show_parent_access_ = false;
  // TODO(crbug.com/1307303): Determine if this is a kiosk license device.
  bool kiosk_license_mode_ = false;
  // When the Gaia screen is active during Login, the guest-login button should
  // appear if there are no user views.
  bool login_screen_has_users_ = false;

  raw_ptr<LockScreenActionBackgroundController> lock_screen_action_background_;

  base::ScopedObservation<TrayAction, TrayActionObserver>
      tray_action_observation_{this};

  base::ScopedObservation<LockScreenActionBackgroundController,
                          LockScreenActionBackgroundObserver>
      lock_screen_action_background_observation_{this};

  base::ScopedObservation<ShutdownControllerImpl,
                          ShutdownControllerImpl::Observer>
      shutdown_controller_observation_{this};

  base::ScopedObservation<LoginDataDispatcher, LoginDataDispatcher::Observer>
      login_data_dispatcher_observation_{this};

  base::ScopedObservation<EnterpriseDomainModel, EnterpriseDomainObserver>
      enterprise_domain_model_observation_{this};

  // The kiosk app button will only be created for the primary display's login
  // shelf.
  raw_ptr<KioskAppsButton> kiosk_apps_button_ = nullptr;

  // The shutdown confirmation bubble button.
  raw_ptr<LoginShelfButton> shutdown_confirmation_button_ = nullptr;

  // The kiosk app instruction will be shown if the kiosk app button is visible.
  raw_ptr<KioskAppInstructionBubble> kiosk_instruction_bubble_ = nullptr;

  // This is used in tests to check if the confirmation bubble is visible and to
  // click its buttons.
  raw_ptr<ShelfShutdownConfirmationBubble> test_shutdown_confirmation_bubble_ =
      nullptr;

  // This is used in tests to wait until UI is updated.
  std::unique_ptr<TestUiUpdateDelegate> test_ui_update_delegate_;

  // Maintains a list of LoginShelfButton children of LoginShelfView.
  std::vector<raw_ptr<LoginShelfButton, VectorExperimental>>
      login_shelf_buttons_;

  // Number of active scoped Guest button blockers.
  int scoped_guest_button_blockers_ = 0;

  // Whether shelf buttons are temporarily disabled due to opened modal dialog.
  bool is_shelf_temp_disabled_ = false;

  // Counter for dropped shutdown and signout calls due to turned off displays.
  int dropped_calls_when_displays_off_ = 0;

  // Set of the tray buttons which are in disabled state. It is used to record
  // and recover the states of tray buttons after temporarily disable of the
  // buttons.
  std::set<raw_ptr<TrayBackgroundView, SetExperimental>> disabled_tray_buttons_;

  base::WeakPtrFactory<LoginShelfView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_LOGIN_SHELF_VIEW_H_
