// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/tray_action/tray_action.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/views/widget/widget.h"

namespace views {
class View;
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
    void AddOnShownCallback(base::OnceClosure on_shown);

   private:
    const raw_ptr<LockScreen, DanglingUntriaged> lock_screen_;
  };

  // The UI that this instance is displaying.
  enum class ScreenType { kLogin, kLock };

  LockScreen(const LockScreen&) = delete;
  LockScreen& operator=(const LockScreen&) = delete;

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
  // Shows the current device privacy disclosures.
  void ShowManagementDisclosureDialog();
  void SetHasKioskApp(bool has_kiosk_apps);

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLockStateChanged(bool locked) override;
  void OnChromeTerminating() override;

 private:
  explicit LockScreen(ScreenType type);
  ~LockScreen() override;

  std::unique_ptr<views::View> MakeContentsView();

  // Shows the lock screen widget, unless the global instance was already
  // destroyed. Called after the first wallpaper becomes ready.
  static void ShowWidgetUponWallpaperReady();

  // The type of screen shown. Controls how the screen is dismissed.
  const ScreenType type_;

  // The lock screen widget.
  std::unique_ptr<views::Widget> widget_;

  // Unowned pointer to the LockContentsView hosted in lock window.
  raw_ptr<LockContentsView> contents_view_ = nullptr;

  bool is_shown_ = false;

  // Clipboard used to restore user session's clipboard, after having made a
  // new one especially for the lock screen. We want two separate clipboards
  // for security purposes: if a user leaves their session locked, with their
  // password copied, it leaves the lock screen vulnerable. However, this is
  // a desirable behavior for secondary login screen.
  std::unique_ptr<ui::Clipboard> saved_clipboard_;

  std::unique_ptr<views::Widget::PaintAsActiveLock> paint_as_active_lock_;

  base::ScopedObservation<TrayAction, TrayActionObserver>
      tray_action_observation_{this};
  ScopedSessionObserver session_observer_{this};

  std::vector<base::OnceClosure> on_shown_callbacks_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_H_
