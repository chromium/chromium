// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "ash/shutdown_controller_impl.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/tray_action/tray_action.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
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

class KioskAppsButton;

// LoginShelfView contains the shelf buttons visible outside of an active user
// session. ShelfView and LoginShelfView should never be shown together.
class ASH_EXPORT LoginShelfView : public views::View,
                                  public views::ButtonListener,
                                  public TrayActionObserver,
                                  public LockScreenActionBackgroundObserver,
                                  public ShutdownControllerImpl::Observer,
                                  public LoginDataDispatcher::Observer,
                                  public LocaleChangeObserver {
 public:
  enum ButtonId {
    kShutdown = 1,   // Shut down the device.
    kRestart,        // Restart the device.
    kSignOut,        // Sign out the active user session.
    kCloseNote,      // Close the lock screen note.
    kCancel,         // Cancel multiple user sign-in.
    kBrowseAsGuest,  // Use in guest mode.
    kAddUser,        // Add a new user.
    kApps,           // Show list of available kiosk apps.
    kParentAccess    // Unlock child device with Parent Access Code.
  };

  // Stores and notifies UiUpdate test callbacks.
  class TestUiUpdateDelegate {
   public:
    virtual ~TestUiUpdateDelegate();
    virtual void OnUiUpdate() = 0;
  };

 public:
  explicit LoginShelfView(
      LockScreenActionBackgroundController* lock_screen_action_background);
  ~LoginShelfView() override;

  // ShelfWidget observes SessionController for higher-level UI changes and
  // then notifies LoginShelfView to update its own UI.
  void UpdateAfterSessionChange();

  // Sets the contents of the kiosk app menu, as well as the callback used when
  // a menu item is selected.
  void SetKioskApps(
      const std::vector<KioskAppMenuEntry>& kiosk_apps,
      const base::RepeatingCallback<void(const KioskAppMenuEntry&)>&
          launch_app);

  // Sets the state of the login dialog.
  void SetLoginDialogState(OobeDialogState state);

  // Sets if the guest button on the login shelf can be shown. Even if set to
  // true the button may still not be visible.
  void SetAllowLoginAsGuest(bool allow_guest);

  // Sets whether parent access button can be shown on the login shelf.
  void ShowParentAccessButton(bool show);

  // Sets if the guest button on the login shelf can be shown during gaia
  // signin screen.
  void ShowGuestButtonInOobe(bool show);

  // Sets whether users can be added from the login screen.
  void SetAddUserButtonEnabled(bool enable_add_user);

  // Sets whether shutdown button is enabled in the login screen.
  void SetShutdownButtonEnabled(bool enable_shutdown_button);

  // views::View:
  const char* GetClassName() const override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  gfx::Rect get_button_union_bounds() const { return button_union_bounds_; }

  // Test API. Returns true if request was successful (i.e. button was
  // clickable).
  bool LaunchAppForTesting(const std::string& app_id);
  bool SimulateAddUserButtonForTesting();
  bool SimulateGuestButtonForTesting();

  // Adds test delegate. Delegate will become owned by LoginShelfView.
  void InstallTestUiUpdateDelegate(
      std::unique_ptr<TestUiUpdateDelegate> delegate);

  TestUiUpdateDelegate* test_ui_update_delegate() {
    return test_ui_update_delegate_.get();
  }

  // Returns scoped object to temporarily block Browse as Guest login button.
  std::unique_ptr<ScopedGuestButtonBlocker> GetScopedGuestButtonBlocker();

 protected:
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

  // LocaleChangeObserver:
  void OnLocaleChanged() override;

 private:
  class ScopedGuestButtonBlockerImpl;

  bool LockScreenActionBackgroundAnimating() const;

  // Updates the visibility of buttons based on state changes, e.g. shutdown
  // policy updates, session state changes etc.
  void UpdateUi();

  // Updates the color of all buttons. Uses dark colors if |use_dark_colors| is
  // true, light colors otherwise.
  void UpdateButtonColors(bool use_dark_colors);

  // Updates the total bounds of all buttons.
  void UpdateButtonUnionBounds();

  bool ShouldShowGuestButton() const;

  OobeDialogState dialog_state_ = OobeDialogState::HIDDEN;
  bool allow_guest_ = true;
  bool allow_guest_in_oobe_ = false;
  bool show_parent_access_ = false;
  // When the Gaia screen is active during Login, the guest-login button should
  // appear if there are no user views.
  bool login_screen_has_users_ = false;

  LockScreenActionBackgroundController* lock_screen_action_background_;

  ScopedObserver<TrayAction, TrayActionObserver> tray_action_observer_{this};

  ScopedObserver<LockScreenActionBackgroundController,
                 LockScreenActionBackgroundObserver>
      lock_screen_action_background_observer_{this};

  ScopedObserver<ShutdownControllerImpl, ShutdownControllerImpl::Observer>
      shutdown_controller_observer_{this};

  ScopedObserver<LocaleUpdateControllerImpl, LocaleChangeObserver>
      locale_change_observer_{this};

  ScopedObserver<LoginDataDispatcher, LoginDataDispatcher::Observer>
      login_data_dispatcher_observer_{this};

  // The kiosk app button will only be created for the primary display's login
  // shelf.
  KioskAppsButton* kiosk_apps_button_ = nullptr;

  // This is used in tests to wait until UI is updated.
  std::unique_ptr<TestUiUpdateDelegate> test_ui_update_delegate_;

  // The bounds of all the buttons that this view is showing. Useful for
  // letting events that target the "empty space" pass through. These
  // coordinates are local to the view.
  gfx::Rect button_union_bounds_;

  // Number of active scoped Guest button blockers.
  int scoped_guest_button_blockers_ = 0;

  base::WeakPtrFactory<LoginShelfView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_LOGIN_SHELF_VIEW_H_
