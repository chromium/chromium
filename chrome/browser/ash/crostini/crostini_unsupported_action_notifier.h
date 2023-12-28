// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/system/toast_data.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/display/display_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace crostini {

// Notifies the user when they try to do something Crostini doesn't yet support
// e.g. use the virtual keyboard in a crostini app.
// TODO(davidmunro): Emit metrics around how often we're hitting these issues so
// we can prioritise appropriately.
class CrostiniUnsupportedActionNotifier
    : public display::DisplayObserver,
      public aura::client::FocusChangeObserver,
      public ash::KeyboardControllerObserver {
 public:
  // Adapter around external integrations which we can mock out for testing,
  // stateless.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    virtual bool IsInTabletMode();

    // True if the window which currently has focus is a crostini window,
    // doesn't count the terminal.
    virtual bool IsFocusedWindowCrostini();

    // Is the current virtual keyboard visible.
    virtual bool IsVirtualKeyboardVisible();

    // Shows a toast to the user.
    virtual void ShowToast(ash::ToastData toast_data);

    // How long toasts should be displayed for. Timing varies depending on
    // e.g. whether screen magnification is enabled.
    virtual base::TimeDelta ToastTimeout();

    virtual void AddFocusObserver(aura::client::FocusChangeObserver* observer);
    virtual void RemoveFocusObserver(
        aura::client::FocusChangeObserver* observer);
    virtual void AddDisplayObserver(display::DisplayObserver* observer);
    virtual void RemoveDisplayObserver(display::DisplayObserver* observer);
    virtual void AddKeyboardControllerObserver(
        ash::KeyboardControllerObserver* observer);
    virtual void RemoveKeyboardControllerObserver(
        ash::KeyboardControllerObserver* observer);
  };

  CrostiniUnsupportedActionNotifier();
  explicit CrostiniUnsupportedActionNotifier(
      std::unique_ptr<Delegate> delegate);

  CrostiniUnsupportedActionNotifier(const CrostiniUnsupportedActionNotifier&) =
      delete;
  CrostiniUnsupportedActionNotifier& operator=(
      const CrostiniUnsupportedActionNotifier&) = delete;

  ~CrostiniUnsupportedActionNotifier() override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // ash::KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool visible) override;

  Delegate* get_delegate_for_testing() { return delegate_.get(); }

 private:
  // Checks if the user is trying to use a virtual keyboard with a crostini
  // app and, if so and if they haven't already been notified that it's not
  // supported, notify them.
  void ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();

  std::unique_ptr<Delegate> delegate_;
  bool virtual_keyboard_unsupported_message_shown_ = false;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_
