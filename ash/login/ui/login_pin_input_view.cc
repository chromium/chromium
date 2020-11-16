// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_input_view.h"

#include "ash/login/ui/access_code_input.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {
// Max width of the pin input field
constexpr const int kMaxWidthPinInputDp = 280;
constexpr const int kFieldWidth = 24;
constexpr const int kFieldSpace = 8;
// Total height of the view.
constexpr const int kPinInputTotalHeightDp = 37;
// Default length
constexpr const int kDefaultLength = 6;
constexpr const int kPinAutosubmitMinLength = 6;
constexpr const int kPinAutosubmitMaxLength = 12;
}  // namespace

// A FixedLengthCodeInput that is always obscured and
// has some special focus handling.
class LoginPinInput : public FixedLengthCodeInput {
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
  // views::view
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  int length_ = 0;
  LoginPinInputView::OnPinSubmit on_submit_;
  LoginPinInputView::OnPinChanged on_changed_;

  base::WeakPtrFactory<FixedLengthCodeInput> weak_ptr_factory_{this};
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
}

void LoginPinInput::OnModified(bool last_field_active, bool complete) {
  DCHECK(on_changed_);
  on_changed_.Run(IsEmpty());

  // Submit the input if its the last field, and complete.
  if (last_field_active && complete) {
    base::Optional<std::string> user_input = GetCode();
    DCHECK(on_submit_);
    SetReadOnly(true);
    on_submit_.Run(base::UTF8ToUTF16(user_input.value_or(std::string())));
  }
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
  if (gesture_event.details().type() != ui::EventType::ET_GESTURE_TAP)
    return false;

  FixedLengthCodeInput::RequestFocus();
  return true;
}

void LoginPinInput::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  FixedLengthCodeInput::GetAccessibleNodeData(node_data);
  const int inserted_digits = active_input_index();
  const int remaining_digits = length_ - inserted_digits;
  node_data->SetDescription(l10n_util::GetPluralStringFUTF16(
      IDS_ASH_LOGIN_PIN_INPUT_DIGITS_REMAINING, remaining_digits));
  node_data->SetName(l10n_util::GetStringUTF8(
          IDS_ASH_LOGIN_POD_PASSWORD_PIN_INPUT_ACCESSIBLE_NAME));
}

LoginPinInputView::TestApi::TestApi(LoginPinInputView* view) : view_(view) {
  DCHECK(view_);
}

LoginPinInputView::TestApi::~TestApi() = default;

views::View* LoginPinInputView::TestApi::code_input() {
  return view_->code_input_;
}

base::Optional<std::string> LoginPinInputView::TestApi::GetCode() {
  return view_->code_input_->GetCode();
}

LoginPinInputView::LoginPinInputView() : length_(kDefaultLength) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  code_input_ = AddChildView(std::make_unique<LoginPinInput>(
      length_,
      base::BindRepeating(&LoginPinInputView::SubmitPin,
                          base::Unretained(this)),
      base::BindRepeating(&LoginPinInputView::OnChanged,
                          base::Unretained(this))));
  Layout();
}

LoginPinInputView::~LoginPinInputView() = default;

bool LoginPinInputView::IsAutosubmitSupported(int length) {
  return chromeos::features::IsPinAutosubmitFeatureEnabled() &&
         length >= kPinAutosubmitMinLength && length <= kPinAutosubmitMaxLength;
}

void LoginPinInputView::Init(const OnPinSubmit& on_submit,
                             const OnPinChanged& on_changed) {
  DCHECK(on_submit);
  DCHECK(on_changed);
  on_submit_ = on_submit;
  on_changed_ = on_changed;
}

void LoginPinInputView::SubmitPin(const base::string16& pin) {
  DCHECK(on_submit_);
  on_submit_.Run(pin);
}

void LoginPinInputView::UpdateLength(const size_t pin_length) {
  // If the length is 0 (unknown) auto submit is disabled and not visible.
  // Only recreate the UI if the length is different than the current one.
  if (pin_length == 0 || pin_length == length_)
    return;

  // Hide the view before deleting.
  SetVisible(false);

  length_ = pin_length;
  RemoveChildView(code_input_);
  delete code_input_;
  code_input_ = AddChildView(std::make_unique<LoginPinInput>(
      pin_length,
      base::BindRepeating(&LoginPinInputView::SubmitPin,
                          base::Unretained(this)),
      base::BindRepeating(&LoginPinInputView::OnChanged,
                          base::Unretained(this))));
  is_read_only_ = false;
  Layout();
  SetVisible(true);
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
  if (!is_read_only_)
    code_input_->InsertDigit(digit);
}

void LoginPinInputView::SetReadOnly(bool read_only) {
  is_read_only_ = read_only;
  code_input_->SetReadOnly(read_only);
}

gfx::Size LoginPinInputView::CalculatePreferredSize() const {
  const int ideal_size = kFieldWidth * length_ +
                         kFieldSpace * (length_ - 1);
  return gfx::Size(std::min(kMaxWidthPinInputDp, ideal_size),
                   kPinInputTotalHeightDp);
}

void LoginPinInputView::RequestFocus() {
  DCHECK(code_input_);
  code_input_->RequestFocus();
}

void LoginPinInputView::OnChanged(bool is_empty) {
  if (on_changed_)
    on_changed_.Run(is_empty);
}

}  // namespace ash
