// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_
#define ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/login/ui/auth_error_bubble.h"
#include "ash/login/ui/bottom_status_indicator.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_error_bubble.h"
#include "ash/login/ui/management_bubble.h"
#include "ash/login/ui/management_disclosure_dialog.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/user_state.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/management_disclosure_client.h"
#include "ash/public/cpp/smartlock_state.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/tray/system_tray_observer.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace keyboard {
class KeyboardUIController;
}  // namespace keyboard

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {

class KioskAppDefaultMessage;
class LockScreenMediaView;
class LoginAuthUserView;
class LoginBigUserView;
class LoginCameraTimeoutView;
class LoginDetachableBaseModel;
class LoginExpandedPublicAccountView;
class LoginUserView;
class NoteActionLaunchButton;
class ScrollableUsersListView;

namespace mojom {
enum class TrayActionState;
}

enum class BottomIndicatorState {
  kNone,
  kManagedDevice,
  kAdbSideLoadingEnabled,
};

// LockContentsView hosts the root view for the lock screen. All other lock
// screen views are embedded within this one. LockContentsView is per-display,
// but it is always shown on the primary display. There is only one instance
// at a time.
class ASH_EXPORT LockContentsView
    : public NonAccessibleView,
      public LoginDataDispatcher::Observer,
      public SystemTrayObserver,
      public display::DisplayObserver,
      public KeyboardControllerObserver,
      public chromeos::PowerManagerClient::Observer,
      public EnterpriseDomainObserver,
      public views::FocusChangeListener {
  METADATA_HEADER(LockContentsView, NonAccessibleView)

 public:
  friend class LockContentsViewTestApi;

  enum class DisplayStyle {
    // Display all the user views, top header view in LockContentsView.
    kAll,
    // Display only the public account expanded view, other views in
    // LockContentsView are hidden.
    kExclusivePublicAccountExpandedView,
  };

  // Number of login attempts before a login dialog is shown. For example, if
  // this value is 4 then the user can submit their password 4 times, and on the
  // 4th bad attempt the login dialog is shown. This only applies to the login
  // screen.
  static const int kLoginAttemptsBeforeGaiaDialog;

  LockContentsView(
      mojom::TrayActionState initial_note_action_state,
      LockScreen::ScreenType screen_type,
      LoginDataDispatcher* data_dispatcher,
      std::unique_ptr<LoginDetachableBaseModel> detachable_base_model);

  LockContentsView(const LockContentsView&) = delete;
  LockContentsView& operator=(const LockContentsView&) = delete;

  ~LockContentsView() override;

  void FocusNextUser();
  void FocusPreviousUser();
  void ShowEnterpriseDomainManager(
      const std::string& entreprise_domain_manager);
  void ShowAdbEnabled();
  void ToggleSystemInfo();
  void ShowParentAccessDialog();
  // Shows the current device privacy disclosures.
  void ShowManagementDisclosureDialog();
  void SetHasKioskApp(bool has_kiosk_apps);

  // views::View:
  void Layout(PassKey) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(const std::vector<LoginUserInfo>& users) override;
  void OnUserAvatarChanged(const AccountId& account_id,
                           const UserAvatar& avatar) override;
  void OnUserAuthFactorsChanged(
      const AccountId& user,
      cryptohome::AuthFactorsSet auth_factors,
      cryptohome::PinLockAvailability pin_available_at) override;
  void OnPinEnabledForUserChanged(
      const AccountId& user,
      bool enabled,
      cryptohome::PinLockAvailability available_at) override;
  void OnChallengeResponseAuthEnabledForUserChanged(const AccountId& user,
                                                    bool enabled) override;
  void OnFingerprintStateChanged(const AccountId& account_id,
                                 FingerprintState state) override;
  void OnResetFingerprintUIState(const AccountId& account_id) override;
  void OnFingerprintAuthResult(const AccountId& account_id,
                               bool success) override;
  void OnSmartLockStateChanged(const AccountId& account_id,
                               SmartLockState state) override;
  void OnSmartLockAuthResult(const AccountId& account_id,
                             bool success) override;
  void OnAuthEnabledForUser(const AccountId& user) override;
  void OnAuthDisabledForUser(
      const AccountId& user,
      const AuthDisabledData& auth_disabled_data) override;
  void OnAuthenticationStageChanged(
      const AuthenticationStage auth_stage) override;
  void OnSetTpmLockedState(const AccountId& user,
                           bool is_locked,
                           base::TimeDelta time_left) override;
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;
  void OnForceOnlineSignInForUser(const AccountId& user) override;
  void OnWarningMessageUpdated(const std::u16string& message) override;
  void OnSystemInfoChanged(bool show,
                           bool enforced,
                           const std::string& os_version_label_text,
                           const std::string& enterprise_info_text,
                           const std::string& bluetooth_name,
                           bool adb_sideloading_enabled) override;
  void OnPublicSessionDisplayNameChanged(
      const AccountId& account_id,
      const std::string& display_name) override;
  void OnPublicSessionLocalesChanged(const AccountId& account_id,
                                     const std::vector<LocaleItem>& locales,
                                     const std::string& default_locale,
                                     bool show_advanced_view) override;
  void OnPublicSessionKeyboardLayoutsChanged(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<InputMethodItem>& keyboard_layouts) override;
  void OnPublicSessionShowFullManagementDisclosureChanged(
      bool show_full_management_disclosure) override;
  void OnDetachableBasePairingStatusChanged(
      DetachableBasePairingStatus pairing_status) override;
  void OnFocusLeavingLockScreenApps(bool reverse) override;
  void OnOobeDialogStateChanged(OobeDialogState state) override;

  void MaybeUpdateExpandedView(const AccountId& account_id,
                               const LoginUserInfo& user_info);

  // SystemTrayObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool is_visible) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;

  // ash::EnterpriseDomainObserver:
  void OnDeviceEnterpriseInfoChanged() override;
  void OnEnterpriseAccountDomainChanged() override;

  // Called by LockScreenMediaView:
  void CreateMediaView();
  void HideMediaView();
  bool AreMediaControlsEnabled() const;

  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

 private:
  using DisplayLayoutAction = base::RepeatingCallback<void(bool landscape)>;

  // Focus the next/previous widget.
  void FocusNextWidget(bool reverse);

  // Set |spacing_middle| to the correct size for low density layouts. If there
  // is less spacing available than desired, use up to the available.
  void SetLowDensitySpacing(views::View* spacing_middle,
                            views::View* secondary_view,
                            int landscape_dist,
                            int portrait_dist,
                            bool landscape);

  // Set |spacing_middle| for |media_view_|.
  void SetMediaViewSpacing(bool landscape);

  // 1-2 users.
  void CreateLowDensityLayout(
      const std::vector<LoginUserInfo>& users,
      std::unique_ptr<LoginBigUserView> primary_big_view);
  // 3-6 users.
  void CreateMediumDensityLayout(
      const std::vector<LoginUserInfo>& users,
      std::unique_ptr<LoginBigUserView> primary_big_view);
  // 7+ users.
  void CreateHighDensityLayout(
      const std::vector<LoginUserInfo>& users,
      views::BoxLayout* main_layout,
      std::unique_ptr<LoginBigUserView> primary_big_view);

  // Lay out the entire view. This is called when the view is attached to a
  // widget and when the screen is rotated.
  void DoLayout();

  // Lay out the top header. This is called when the children of the top header
  // change contents or visibility.
  void LayoutTopHeader();

  // Lay out the bottom status indicator. This is called when system information
  // is shown if ADB is enabled and at the initialization of lock screen if the
  // device is enrolled.
  void LayoutBottomStatusIndicator();

  // Lay out the user adding screen indicator. This is called when a secondary
  // user is being added.
  void LayoutUserAddingScreenIndicator();

  // Lay out the expanded public session view.
  void LayoutPublicSessionView();

  // Adds |layout_action| to |layout_actions_| and immediately executes it with
  // the current rotation.
  void AddDisplayLayoutAction(const DisplayLayoutAction& layout_action);

  // Change the active |auth_user_|. If |is_primary| is true, the active auth
  // switches to |opt_secondary_big_view_|. If |is_primary| is false, the active
  // auth switches to |primary_big_view_|.
  void SwapActiveAuthBetweenPrimaryAndSecondary(bool is_primary);

  // Called when an authentication check is complete.
  void OnAuthenticate(bool auth_success,
                      bool display_error_messages,
                      bool authenticated_by_pin);

  // Tries to lookup the stored state for |user|. Returns an unowned pointer
  // that is invalidated whenver |users_| changes.
  UserState* FindStateForUser(const AccountId& user);

  // Updates the auth methods for |to_update| and |to_hide|, if passed.
  // For auth users:
  //   |to_hide| will be set to LoginAuthUserView::AUTH_NONE. At minimum,
  //   |to_update| will show a password prompt.
  // For pubic account users:
  //   |to_hide| will set to disable auth.
  //   |to_update| will show an arrow button.
  void LayoutAuth(LoginBigUserView* to_update,
                  LoginBigUserView* opt_to_hide,
                  bool animate);

  // Make the user at |user_index| the big user with auth enabled.
  // We pass in the index because the actual user may change.
  void SwapToBigUser(int user_index);

  // Warning to remove a user is shown.
  void OnRemoveUserWarningShown(bool is_primary);
  // Remove one of the auth users.
  void RemoveUser(bool is_primary);

  // Called after the big user change has taken place.
  void OnBigUserChanged();

  // Get the current active big user view.
  LoginBigUserView* CurrentBigUserView();

  // Opens an error bubble to indicate authentication failure.
  void ShowAuthErrorMessage(bool authenticated_by_pin);

  // Hides the error bubble indicating authentication failure if open.
  void HideAuthErrorMessage();

  // Called when LoginAuthFactorsView enters/exits a state where an auth
  // factor wants to hide the password and pin fields.
  void OnAuthFactorIsHidingPasswordChanged(const AccountId& account_id,
                                           bool auth_factor_is_hiding_password);

  // Called when parent access validation finished for the user with
  // |account_id|.
  void OnParentAccessValidationFinished(const AccountId& account_id,
                                        bool access_granted);

  // Returns keyboard controller for the view. Returns nullptr if keyboard is
  // not activated, view has not been added to the widget yet or keyboard is not
  // displayed in this window.
  keyboard::KeyboardUIController* GetKeyboardControllerForView() const;

  // Called when the public account is tapped.
  void OnPublicAccountTapped(bool is_primary);

  // Called when the user presses buttons in the authentication error bubble.
  void LearnMoreButtonPressed();
  void RecoverUserButtonPressed();

  // Helper method to allocate a LoginBigUserView instance.
  std::unique_ptr<LoginBigUserView> AllocateLoginBigUserView(
      const LoginUserInfo& user,
      bool is_primary);

  // Returns the big view for |user| if |user| is one of the active
  // big views. If |require_auth_active| is true then the view must
  // have auth enabled.
  LoginBigUserView* TryToFindBigUser(const AccountId& user,
                                     bool require_auth_active);

  // Returns the user view for |user|.
  LoginUserView* TryToFindUserView(const AccountId& user);

  // Change the visibility of child views based on the |style|.
  void SetDisplayStyle(DisplayStyle style);

  // Register accelerators used in login screen.
  void RegisterAccelerators();

  // Performs the specified accelerator action.
  void PerformAction(LoginAcceleratorAction action);

  // Check whether the view should display the system information based on all
  // factors including policy settings, channel and Alt-V accelerator.
  bool GetSystemInfoVisibility() const;

  // Updates the colors of the system info view.
  void UpdateSystemInfoColors();

  // Updates the colors of the bottom status indicator.
  void UpdateBottomStatusIndicatorColors();

  // Toggles the visibility of the |bottom_status_indicator_| based on its
  // content type and whether the extension UI window is opened.
  void UpdateBottomStatusIndicatorVisibility();

  // Shows a pop-up including more details about device management. It is
  // triggered when the bottom status indicator is clicked while displaying a
  // "device is managed" type message.
  void OnBottomStatusIndicatorTapped();

  // Shows GAIA sign-in page.
  void OnBackToSigninButtonTapped();

  // Update visibility of Kiosk default message. Called only if
  // kiosk_license_mode_ is true.
  void UpdateKioskDefaultMessageVisibility();

  // Record the number of password attempts for the given account id to UMA.
  // Afterwards reset the number of attempts.
  void RecordAndResetPasswordAttempts(
      AuthEventsRecorder::AuthenticationOutcome outcome,
      AccountId account_id);

  // Updates the layout with the new users list.
  void ApplyUserChanges(const std::vector<LoginUserInfo>& users);

  // Shows the pin auth and hides the pin delay message on the user pod when pin
  // becomes available after being soft-locked.
  void OnPinUnlock(bool is_primary);

  const LockScreen::ScreenType screen_type_;

  std::vector<UserState> users_;

  const raw_ptr<LoginDataDispatcher, LeakedDanglingUntriaged>
      data_dispatcher_;  // Unowned.
  std::unique_ptr<LoginDetachableBaseModel> detachable_base_model_;

  raw_ptr<LoginBigUserView> primary_big_view_ = nullptr;
  raw_ptr<LoginBigUserView> opt_secondary_big_view_ = nullptr;
  raw_ptr<ScrollableUsersListView> users_list_ = nullptr;

  // View for media controls that appear on the lock screen if it is enabled.
  raw_ptr<LockScreenMediaView> media_view_ = nullptr;
  raw_ptr<views::View> middle_spacing_view_ = nullptr;

  // View that contains the note action button and the system info labels,
  // placed on the top right corner of the screen without affecting layout of
  // other views.
  raw_ptr<views::View> top_header_ = nullptr;

  // View for launching a note taking action handler from the lock screen.
  raw_ptr<NoteActionLaunchButton> note_action_ = nullptr;

  // View for showing the version, enterprise and bluetooth info.
  raw_ptr<views::View> system_info_ = nullptr;

  // Contains authentication user and the additional user views.
  raw_ptr<NonAccessibleView> main_view_ = nullptr;

  // If the kiosk app button is not visible, the kiosk app default message would
  // be shown.
  raw_ptr<KioskAppDefaultMessage, AcrossTasksDanglingUntriaged>
      kiosk_default_message_ = nullptr;

  // Actions that should be executed before a new layout happens caused by a
  // display change (eg. screen rotation). A full layout pass is performed after
  // all actions are executed.
  std::vector<DisplayLayoutAction> layout_actions_;

  display::ScopedDisplayObserver display_observer_{this};

  base::ScopedObservation<EnterpriseDomainModel, EnterpriseDomainObserver>
      enterprise_domain_model_observation_{this};

  // All error bubbles and the tooltip view are child views of LockContentsView,
  // and will be torn down when LockContentsView is torn down.
  // Bubble for displaying authentication error.
  raw_ptr<AuthErrorBubble> auth_error_bubble_;
  // Bubble for displaying detachable base errors.
  raw_ptr<LoginErrorBubble> detachable_base_error_bubble_;
  // Bubble for displaying management details.
  raw_ptr<ManagementBubble> management_bubble_;
  // Indicator at top of screen for displaying a warning message when a
  // secondary user is being added.
  raw_ptr<views::View> user_adding_screen_indicator_ = nullptr;
  // Bubble for displaying warning banner message.
  raw_ptr<LoginErrorBubble> warning_banner_bubble_;

  // The current ManagementDisclosureDialog, if one exists.
  // Shows the list of device privacy disclosures.
  base::WeakPtr<ManagementDisclosureDialog> management_disclosure_dialog_;

  // View that is shown on login timeout with camera usage.
  raw_ptr<LoginCameraTimeoutView, AcrossTasksDanglingUntriaged>
      login_camera_timeout_view_ = nullptr;

  // Bottom status indicator displaying entreprise domain or ADB enabled alert
  raw_ptr<BottomStatusIndicator> bottom_status_indicator_;

  // Tracks the visibility of the extension Ui window.
  bool extension_ui_visible_ = false;

  // Tracks the unlock attempt of each user before a successful sign-in.
  base::flat_map<AccountId, int> unlock_attempt_by_user_;
  base::flat_map<AccountId, int> pin_unlock_attempt_by_user_;

  // Whether a lock screen app is currently active (i.e. lock screen note action
  // state is reported as kActive by the data dispatcher).
  bool lock_screen_apps_active_ = false;

  // Tracks the visibility of the OOBE dialog.
  bool oobe_dialog_visible_ = false;

  // Whether the lock screen note is disabled. Used to override the actual lock
  // screen note state.
  bool disable_lock_screen_note_ = false;

  // Whether the device is enrolled with Kiosk SKU.
  bool kiosk_license_mode_ = false;
  // Whether any kiosk app is added.
  bool has_kiosk_apps_ = false;

  // Whether the system information should be displayed or not be displayed
  // forcedly according to policy settings.
  std::optional<bool> enable_system_info_enforced_ = std::nullopt;

  // Whether the system information is intended to be displayed if possible.
  // (e.g., Alt-V is pressed, particular OS channels)
  bool enable_system_info_if_possible_ = false;

  // Expanded view for public account user to select language and keyboard.
  raw_ptr<LoginExpandedPublicAccountView> expanded_view_ = nullptr;

  // Whether the virtual keyboard is currently shown. Used to determine whether
  // to show the PIN keyboard or not.
  bool keyboard_shown_ = false;

  // Whether there is an ongoing auth layout. The keyboard visibility might
  // change during an auth layout, if triggered by a focus request on the
  // password view. Since a keyboard visibility change triggers an auth layout
  // and since we do not want an auth layout during an auth layout, we cancel
  // the new layout request (the new keyboard visibility info is useless).
  bool ongoing_auth_layout_ = false;

  // Accelerators handled by login screen.
  std::map<ui::Accelerator, LoginAcceleratorAction> accel_map_;

  BottomIndicatorState bottom_status_indicator_state_ =
      BottomIndicatorState::kNone;

  // When OnUsersChanged called during authentication this object stores
  // the users info till the authentication finished.
  std::optional<std::vector<LoginUserInfo>> pending_users_change_;

  // Client to communicate with chrome for displaying the management disclosure.
  raw_ptr<ManagementDisclosureClient> management_disclosure_client_ = nullptr;

  // The widget this view is attached to. This field is here so that we can
  // remove `this` as FocusChangeListener in `RemovedFromWidget`.
  base::WeakPtr<views::Widget> widget_;

  base::WeakPtrFactory<LockContentsView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_
