// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_input_view.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/access_code_input.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {
// Max width of the pin input field
constexpr const int kMaxWidthPinInputDp = 280;
constexpr const int kFieldWidth = 24;
constexpr const int kFieldSpace = 8;
// Default length
constexpr const int kPinAutosubmitMinLength = 6;
constexpr const int kPinAutosubmitMaxLength = 12;

}  // namespace

// A FixedLengthCodeInput that is always obscured and
// has some special focus handling.
class LoginPinInput : public FixedLengthCodeInput {
  METADATA_HEADER(LoginPinInput, FixedLengthCodeInput)

 public:
  LoginPinInput(int length,
                LoginPinInputView::OnPinSubmit on_submit,
                LoginPinInputView::OnPinChanged on_changed);

  void OnModified(bool last_field_active, bool complete);

  // views::TextfieldController:
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;
  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  void UpdateAccessibleDescription();

  int length_ = 0;
  LoginPinInputView::OnPinSubmit on_submit_;
  LoginPinInputView::OnPinChanged on_changed_;

  base::WeakPtrFactory<FixedLengthCodeInput> weak_ptr_factory_{this};
  base::CallbackListSubscription active_index_changed_callback_;
};

LoginPinInput::LoginPinInput(int length,
                             LoginPinInputView::OnPinSubmit on_submit,
                             LoginPinInputView::OnPinChanged on_changed)
    : FixedLengthCodeInput(length,
                           /*on_input_change*/
                           base::BindRepeating(&LoginPinInput::OnModified,
                                               base::Unretained(this)),
                           /*on_enter*/ base::DoNothing(),
                           /*on_escape*/ base::DoNothing(),
                           /*obscure_pin*/ true),
      length_(length),
      on_submit_(on_submit),
      on_changed_(on_changed) {
  // Do not allow the user to navigate to other fields. Only insertion
  // and deletion will move the caret.
  SetAllowArrowNavigation(false);
  DCHECK(on_submit_);
  DCHECK(on_changed_);

  active_index_changed_callback_ =
      AddActiveInputIndexChanged(base::BindRepeating(
          &LoginPinInput::UpdateAccessibleDescription, base::Unretained(this)));

  GetViewAccessibility().SetName(l10n_util::GetStringUTF8(
      IDS_ASH_LOGIN_POD_PASSWORD_PIN_INPUT_ACCESSIBLE_NAME));
  UpdateAccessibleDescription();
}

void LoginPinInput::OnModified(bool last_field_active, bool complete) {
  DCHECK(on_changed_);
  on_changed_.Run(IsEmpty());

  // Submit the input if its the last field, and complete.
  if (last_field_active && complete) {
    std::optional<std::string> user_input = GetCode();
    DCHECK(on_submit_);
    LOG(WARNING) << "crbug.com/1339004 : Submitting PIN " << IsReadOnly();
    AuthEventsRecorder::Get()->OnPinSubmit();
    SetReadOnly(true);
    on_submit_.Run(base::UTF8ToUTF16(user_input.value_or(std::string())));
  }

  UpdateAccessibleDescription();
}

// Focus on the entire field and not on a single element.
bool LoginPinInput::HandleMouseEvent(views::Textfield* sender,
                                     const ui::MouseEvent& mouse_event) {
  if (!(mouse_event.IsOnlyLeftMouseButton() ||
        mouse_event.IsOnlyRightMouseButton())) {
    return false;
  }

  FixedLengthCodeInput::RequestFocus();
  return true;
}

bool LoginPinInput::HandleGestureEvent(views::Textfield* sender,
                                       const ui::GestureEvent& gesture_event) {
  if (gesture_event.details().type() != ui::EventType::kGestureTap) {
    return false;
  }

  FixedLengthCodeInput::RequestFocus();
  return true;
}

bool LoginPinInput::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  // Let the parent view handle the 'Return' key. Triggers SmartLock login.
  if (key_event.type() == ui::EventType::kKeyPressed &&
      key_event.key_code() == ui::VKEY_RETURN) {
    return false;
  }

  // Delegate all other key events to the base class.
  return FixedLengthCodeInput::HandleKeyEvent(sender, key_event);
}

void LoginPinInput::UpdateAccessibleDescription() {
  const int inserted_digits = active_input_index();
  const int remaining_digits = length_ - inserted_digits;
  GetViewAccessibility().SetDescription(l10n_util::GetPluralStringFUTF16(
      IDS_ASH_LOGIN_PIN_INPUT_DIGITS_REMAINING, remaining_digits));
}

BEGIN_METADATA(LoginPinInput)
END_METADATA

const int LoginPinInputView::kDefaultLength = 6;

LoginPinInputView::TestApi::TestApi(LoginPinInputView* view) : view_(view) {
  DCHECK(view_);
}

LoginPinInputView::TestApi::~TestApi() = default;

views::View* LoginPinInputView::TestApi::code_input() {
  return view_->code_input_;
}

std::optional<std::string> LoginPinInputView::TestApi::GetCode() {
  return view_->code_input_->GetCode();
}

bool LoginPinInputView::TestApi::IsEmpty() {
  return view_->code_input_->IsEmpty();
}

LoginPinInputView::LoginPinInputView() : length_(kDefaultLength) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  code_input_ = AddChildView(std::make_unique<LoginPinInput>(
      length_,
      base::BindRepeating(&LoginPinInputView::SubmitPin,
                          base::Unretained(this)),
      base::BindRepeating(&LoginPinInputView::OnChanged,
                          base::Unretained(this))));
  DeprecatedLayoutImmediately();
}

LoginPinInputView::~LoginPinInputView() = default;

void LoginPinInputView::OnImplicitAnimationsCompleted() {
  Reset();
  SetVisible(false);
  StopObservingImplicitAnimations();
}

bool LoginPinInputView::IsAutosubmitSupported(int length) {
  return length >= kPinAutosubmitMinLength && length <= kPinAutosubmitMaxLength;
}

void LoginPinInputView::Init(const OnPinSubmit& on_submit,
                             const OnPinChanged& on_changed) {
  DCHECK(on_submit);
  DCHECK(on_changed);
  on_submit_ = on_submit;
  on_changed_ = on_changed;
}

void LoginPinInputView::SubmitPin(const std::u16string& pin) {
  DCHECK(on_submit_);
  on_submit_.Run(pin);
}

void LoginPinInputView::UpdateLength(const size_t pin_length) {
  // If the length is 0 (unknown) auto submit is disabled and not visible.
  // Only recreate the UI if the length is different than the current one.
  if (pin_length == 0 || pin_length == length_) {
    return;
  }

  length_ = pin_length;

  const bool was_readonly = IsReadOnly();
  const bool was_visible = GetVisible();

  // Hide the view before deleting.
  SetVisible(false);

  RemoveChildView(code_input_);
  delete code_input_;
  code_input_ = AddChildView(std::make_unique<LoginPinInput>(
      length_,
      base::BindRepeating(&LoginPinInputView::SubmitPin,
                          base::Unretained(this)),
      base::BindRepeating(&LoginPinInputView::OnChanged,
                          base::Unretained(this))));

  SetReadOnly(was_readonly);
  DeprecatedLayoutImmediately();
  SetVisible(was_visible);
}

void LoginPinInputView::SetAuthenticateWithEmptyPinOnReturnKey(bool enabled) {
  authenticate_with_empty_pin_on_return_key_ = enabled;
}

void LoginPinInputView::Reset() {
  DCHECK(code_input_);
  code_input_->ClearInput();
}

void LoginPinInputView::Backspace() {
  DCHECK(code_input_);
  code_input_->Backspace();
}

void LoginPinInputView::InsertDigit(int digit) {
  DCHECK(code_input_);
  if (!IsReadOnly()) {
    code_input_->InsertDigit(digit);
  }
}

void LoginPinInputView::SetReadOnly(bool read_only) {
  if (!read_only &&
      Shell::Get()->login_screen_controller()->IsAuthenticating()) {
    // TODO(b/276246832): We shouldn't enable the LoginPinInputView during
    // Authentication.
    LOG(WARNING) << "LoginPinInputView::SetReadOnly called with false during "
                    "Authentication.";
  }
  is_read_only_ = read_only;
  code_input_->SetReadOnly(read_only);
}

bool LoginPinInputView::IsReadOnly() const {
  return is_read_only_;
}

gfx::Size LoginPinInputView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int ideal_size = kFieldWidth * length_ + kFieldSpace * (length_ - 1);
  const int available_width = std::min(kMaxWidthPinInputDp, ideal_size);
  return gfx::Size(
      available_width,
      GetLayoutManager()->GetPreferredHeightForWidth(this, available_width));
}

void LoginPinInputView::RequestFocus() {
  DCHECK(code_input_);
  code_input_->RequestFocus();
}

bool LoginPinInputView::OnKeyPressed(const ui::KeyEvent& event) {
  // The 'Return' key has no relationship to the digits inserted in this view.
  // It just performs an unlock attempt with an empty PIN, which triggers a
  // SmartLock attempt in LoginAuthUserView. The user's PIN is only submitted
  // when the last digit is inserted.
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN && !IsReadOnly() &&
      authenticate_with_empty_pin_on_return_key_) {
    SubmitPin(u"");
    return true;
  }

  return false;
}

void LoginPinInputView::OnChanged(bool is_empty) {
  if (on_changed_) {
    on_changed_.Run(is_empty);
  }
}

BEGIN_METADATA(LoginPinInputView)
END_METADATA

}  // namespace ash
