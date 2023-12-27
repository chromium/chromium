// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
//   |  <-   |  |   0   |  |  ->   |
//   |       |  |   +   |  |       |
//    -------    -------    -------
//
// The <- represents the delete button while -> represents the submit button.
// The submit button is optional.
//
class ASH_EXPORT LoginPinView : public NonAccessibleView {
  METADATA_HEADER(LoginPinView, NonAccessibleView)

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
    views::View* GetSubmitButton() const;

    // Sets the timers that are used for backspace auto-submit. |delay_timer| is
    // the initial delay before an auto-submit, and |repeat_timer| fires
    // whenever a new backspace event should run after the initial delay.
    void SetBackspaceTimers(std::unique_ptr<base::OneShotTimer> delay_timer,
                            std::unique_ptr<base::RepeatingTimer> repeat_timer);

    void ClickOnDigit(int number) const;

   private:
    const raw_ptr<LoginPinView, DanglingUntriaged> view_;
  };

  using OnPinKey = base::RepeatingCallback<void(int value)>;
  using OnPinBackspace = base::RepeatingClosure;
  using OnPinSubmit = base::RepeatingClosure;

  // Creates PIN view with the specified |keyboard_style|.
  // |on_key| is called whenever the user taps one of the pin buttons; must be
  // non-null.
  // |on_backspace| is called when the user wants to erase the most recently
  // tapped key; must be non-null.
  // If |on_submit| is valid, there will be a submit button on the pinpad that
  // calls it when the user wants to submit the PIN / password.
  LoginPinView(Style keyboard_style,
               const OnPinKey& on_key,
               const OnPinBackspace& on_backspace,
               const OnPinSubmit& on_submit = base::NullCallback());

  LoginPinView(const LoginPinView&) = delete;
  LoginPinView& operator=(const LoginPinView&) = delete;

  ~LoginPinView() override;

  // Notify accessibility that location of rows and LoginPinView changed.
  void NotifyAccessibilityLocationChanged();

  // Called when the password field text changed.
  void OnPasswordTextChanged(bool is_empty);

 private:
  class BackspacePinButton;
  class DigitPinButton;
  class SubmitPinButton;

  // Builds and returns a new view which contains a row of the PIN keyboard.
  NonAccessibleView* BuildAndAddRow();

  raw_ptr<BackspacePinButton> backspace_ = nullptr;
  // The submit button does not exist when no |on_submit| callback is passed.
  raw_ptr<SubmitPinButton> submit_button_ = nullptr;

  std::vector<raw_ptr<NonAccessibleView, VectorExperimental>> rows_;
  std::vector<raw_ptr<DigitPinButton, VectorExperimental>> digit_buttons_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PIN_VIEW_H_
