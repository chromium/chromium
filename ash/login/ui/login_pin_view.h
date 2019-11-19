// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "base/callback.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace base {
class OneShotTimer;
class RepeatingTimer;
}  // namespace base

namespace ash {

// Implements a PIN keyboard. The class emits high-level events that can be used
// by the embedder. The PIN keyboard, while displaying letters, only emits
// numbers.
//
// The view is always rendered via layers.
//
// The UI looks a little like this:
//    _______    _______    _______
//   |   1   |  |   2   |  |   3   |
//   |       |  | A B C |  | D E F |
//    -------    -------    -------
//    _______    _______    _______
//   |   4   |  |   5   |  |   6   |
//   | G H I |  | J K L |  | M N O |
//    -------    -------    -------
//    _______    _______    _______
//   |   7   |  |   8   |  |   9   |
//   |P Q R S|  | T U V |  |W X Y Z|
//    -------    -------    -------
//    _______    _______    _______
//   |  BACK |  |   0   |  |  <-   |
//   |       |  |   +   |  |       |
//    -------    -------    -------
//
// The "BACK" button is hidden by default.
//
class ASH_EXPORT LoginPinView : public NonAccessibleView {
 public:
  // Visual style of PIN keyboard.
  enum class Style {
    // Has a layout and look of a telephone keypad. Keys display a combination
    // of a digit and letters. The letters can be used for mnemonic techniques.
    kAlphanumeric,
    // Has a layout of a telephone keypad, but keys display only digits, no
    // letters.
    kNumeric,
  };

  class ASH_EXPORT TestApi {
   public:
    // Returns expected button size for the given PIN keyboard |style|.
    static gfx::Size GetButtonSize(Style style);

    explicit TestApi(LoginPinView* view);
    ~TestApi();

    views::View* GetButton(int number) const;
    views::View* GetBackspaceButton() const;
    views::View* GetBackButton() const;

    // Sets the timers that are used for backspace auto-submit. |delay_timer| is
    // the initial delay before an auto-submit, and |repeat_timer| fires
    // whenever a new backspace event should run after the initial delay.
    void SetBackspaceTimers(std::unique_ptr<base::OneShotTimer> delay_timer,
                            std::unique_ptr<base::RepeatingTimer> repeat_timer);

   private:
    LoginPinView* const view_;
  };

  using OnPinKey = base::RepeatingCallback<void(int value)>;
  using OnPinBackspace = base::RepeatingClosure;
  using OnPinBack = base::RepeatingClosure;

  // Creates PIN view with the specified |keyboard_style|.
  // |on_key| is called whenever the user taps one of the pin buttons; must be
  // non-null.
  // |on_backspace| is called when the user wants to erase the most recently
  // tapped key; must be non-null.
  // |on_back| is called when the user taps the back button; must be non-null
  // if the back button is shown.
  LoginPinView(Style keyboard_style,
               const OnPinKey& on_key,
               const OnPinBackspace& on_backspace,
               const OnPinBack& on_back);
  ~LoginPinView() override;

  void SetBackButtonVisible(bool visible);

  // Called when the password field text changed.
  void OnPasswordTextChanged(bool is_empty);

 private:
  class BackButton;
  class BackspacePinButton;

  BackButton* back_button_;
  BackspacePinButton* backspace_;
  OnPinKey on_key_;
  OnPinBackspace on_backspace_;
  OnPinBack on_back_;

  DISALLOW_COPY_AND_ASSIGN(LoginPinView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_
