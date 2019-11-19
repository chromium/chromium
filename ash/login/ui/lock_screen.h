// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/login_types.h"
#include "ash/session/session_observer.h"
#include "ash/tray_action/tray_action.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/base/clipboard/clipboard.h"

namespace views {
class Widget;
}

namespace ash {

class LockContentsView;

class ASH_EXPORT LockScreen : public TrayActionObserver,
                              public SessionObserver {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LockScreen* lock_screen);
    ~TestApi();

    LockContentsView* contents_view() const;

   private:
    LockScreen* const lock_screen_;
  };

  // The UI that this instance is displaying.
  enum class ScreenType { kLogin, kLock };

  // Fetch the global lock screen instance. |Show()| must have been called
  // before this.
  static LockScreen* Get();

  // Creates and displays the lock screen. The lock screen communicates with the
  // backend C++ via a mojo API.
  static void Show(ScreenType type);

  // Returns true if the instance has been instantiated.
  static bool HasInstance();

  views::Widget* widget() { return widget_.get(); }

  // Destroys an existing lock screen instance.
  void Destroy();

  ScreenType screen_type() const { return type_; }

  // Returns if the screen has been shown (i.e. |LockWindow::Show| was called).
  bool is_shown() const { return is_shown_; }

  void FocusNextUser();
  void FocusPreviousUser();
  void ShowParentAccessDialog();
  void RequestSecurityTokenPin(SecurityTokenPinRequest request);
  void ClearSecurityTokenPinRequest();

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLockStateChanged(bool locked) override;

 private:
  explicit LockScreen(ScreenType type);
  ~LockScreen() override;

  // The type of screen shown. Controls how the screen is dismissed.
  const ScreenType type_;

  // The lock screen widget.
  std::unique_ptr<views::Widget> widget_;

  // Unowned pointer to the LockContentsView hosted in lock window.
  LockContentsView* contents_view_ = nullptr;

  bool is_shown_ = false;

  std::unique_ptr<ui::Clipboard> saved_clipboard_;

  ScopedObserver<TrayAction, TrayActionObserver> tray_action_observer_{this};
  ScopedSessionObserver session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(LockScreen);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_H_
