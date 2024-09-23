// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PIN_REQUEST_VIEW_H_
#define ASH_LOGIN_UI_PIN_REQUEST_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/login/ui/access_code_input.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/display/display_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace views {
class Label;
class LabelButton;
class Textfield;
}  // namespace views

namespace ash {
class LoginButton;
class LoginPinView;

// State of the PinRequestView.
enum class PinRequestViewState {
  kNormal,
  kError,
};

struct ASH_EXPORT PinRequest {
  PinRequest();
  PinRequest(PinRequest&&);
  PinRequest& operator=(PinRequest&&);
  ~PinRequest();

  // Callback for PIN validations. It is called when the validation has finished
  // and the view is closing.
  // |success| indicates whether the validation was successful.
  using OnPinRequestDone = base::OnceCallback<void(bool success)>;
  OnPinRequestDone on_pin_request_done = base::NullCallback();

  // Whether the help button is displayed.
  bool help_button_enabled = false;

  std::optional<int> pin_length;

  // When |pin_keyboard_always_enabled| is set, the PIN keyboard is displayed at
  // all times. Otherwise, it is only displayed when the device is in tablet
  // mode.
  bool pin_keyboard_always_enabled = false;

  // The pin widget is a modal and already contains a dimmer, however
  // when another modal is the parent of the widget, the dimmer will be placed
  // behind the two windows. |extra_dimmer| will create an extra dimmer between
  // the two.
  bool extra_dimmer = false;

  // Whether the entered PIN should be displayed clearly or only as bullets.
  bool obscure_pin = true;

  // Strings for UI.
  std::u16string title;
  std::u16string description;
  std::u16string accessible_title;
};

// The view that allows for input of pins to authorize certain actions.
class ASH_EXPORT PinRequestView : public views::DialogDelegateView,
                                  public display::DisplayObserver {
 public:
  enum class SubmissionResult {
    // Closes the UI and calls |on_pin_request_done_|.
    kPinAccepted,
    // PIN rejected - keeps the UI in its current state.
    kPinError,
    // Async waiting for result - keeps the UI in its current state.
    kSubmitPending,
  };

  class Delegate {
   public:
    virtual SubmissionResult OnPinSubmitted(const std::string& pin) = 0;
    virtual void OnBack() = 0;
    virtual void OnHelp() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(PinRequestView* view);
    ~TestApi();

    LoginButton* back_button();
    views::Label* title_label();
    views::Label* description_label();
    views::View* access_code_view();
    views::LabelButton* help_button();
    views::Button* submit_button();
    LoginPinView* pin_keyboard_view();

    views::Textfield* GetInputTextField(int index);
    PinRequestViewState state() const;

   private:
    const raw_ptr<PinRequestView, DanglingUntriaged> view_;
  };

  // Creates pin request view that will enable the user to enter a pin.
  // |request| is used to configure callbacks and UI details.
  PinRequestView(PinRequest request, Delegate* delegate);

  PinRequestView(const PinRequestView&) = delete;
  PinRequestView& operator=(const PinRequestView&) = delete;

  ~PinRequestView() override;

  // views::View:
  void RequestFocus() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::DialogDelegateView:
  views::View* GetInitiallyFocusedView() override;
  std::u16string GetAccessibleWindowTitle() const override;

  // display::Observer:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // Sets whether the user can enter a PIN. Other buttons (back, submit etc.)
  // are unaffected.
  void SetInputEnabled(bool input_enabled);

  // Clears previously entered PIN from the PIN input field(s).
  void ClearInput();

  // Updates state of the view.
  void UpdateState(PinRequestViewState state,
                   const std::u16string& title,
                   const std::u16string& description);

 private:
  class FocusableLabelButton;

  // Submits access code for validation.
  void SubmitCode();

  // Closes the view.
  void OnBack();

  // Updates view's preferred size.
  void UpdatePreferredSize();

  // Moves focus to |submit_button_|.
  void FocusSubmitButton();

  // Called when access code input changes. |complete| brings information
  // whether current input code is complete. |last_field_active| contains
  // information whether last input field is currently active.
  void OnInputChange(bool last_field_active, bool complete);

  // Returns if the pin keyboard should be visible.
  bool PinKeyboardVisible() const;

  // Size that depends on the pin keyboards visibility.
  gfx::Size GetPinRequestViewSize() const;

  PinRequestViewState state_ = PinRequestViewState::kNormal;

  // Unowned pointer to the delegate. The delegate should outlive this instance.
  raw_ptr<Delegate> delegate_;

  // Callback to close the UI.
  PinRequest::OnPinRequestDone on_pin_request_done_;

  // Auto submit code when the last input has been inserted.
  bool auto_submit_enabled_ = true;

  // If false, |pin_keyboard_view| is only displayed in tablet mode.
  bool pin_keyboard_always_enabled_ = true;

  // Strings as on view construction to enable restoring the original state.
  std::u16string default_title_;
  std::u16string default_description_;
  const std::u16string default_accessible_title_;

  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> description_label_ = nullptr;
  raw_ptr<AccessCodeInput> access_code_view_ = nullptr;
  raw_ptr<LoginPinView> pin_keyboard_view_ = nullptr;
  raw_ptr<LoginButton> back_button_ = nullptr;
  raw_ptr<FocusableLabelButton> help_button_ = nullptr;
  raw_ptr<views::Button> submit_button_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;

  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<PinRequestView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PIN_REQUEST_VIEW_H_
