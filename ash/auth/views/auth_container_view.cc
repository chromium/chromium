// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_container_view.h"

#include <memory>
#include <string>

#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/auth_view_utils.h"
#include "ash/auth/views/fingerprint_view.h"
#include "ash/auth/views/pin_container_view.h"
#include "ash/auth/views/pin_status_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The Auth container width.
constexpr int kAuthContainerViewWidthDp = 268;

// Distance between the switch button and the above view.
constexpr int kSwitchButtonTopDistanceDp = 28;

// Adapter classes to observe password and pin UI events.
class PinObserverAdapter : public PinContainerView::Observer {
 public:
  explicit PinObserverAdapter(AuthContainerView* view)
      : auth_container_(view) {}
  ~PinObserverAdapter() override = default;
  PinObserverAdapter(const PinObserverAdapter&) = delete;
  PinObserverAdapter& operator=(const PinObserverAdapter&) = delete;

  // PinContainerView::Observer:
  void OnSubmit(const std::u16string& text) override {
    auth_container_->PinSubmit(text);
  }

  void OnEscape() override { auth_container_->Escape(); }

  void OnContentsChanged(const std::u16string& text) override {
    auth_container_->ContentsChanged();
  }

  void OnTextVisibleChanged(bool visible) override {
    auth_container_->ContentsChanged();
  }

 private:
  raw_ptr<AuthContainerView> auth_container_;
};

class PasswordObserverAdapter : public AuthInputRowView::Observer {
 public:
  explicit PasswordObserverAdapter(AuthContainerView* view)
      : auth_container_(view) {}
  ~PasswordObserverAdapter() override = default;
  PasswordObserverAdapter(const PinObserverAdapter&) = delete;
  PasswordObserverAdapter& operator=(const PasswordObserverAdapter&) = delete;

  // AuthInputRowView::Observer:
  void OnSubmit(const std::u16string& text) override {
    auth_container_->PasswordSubmit(text);
  }

  void OnEscape() override { auth_container_->Escape(); }

  void OnContentsChanged(const std::u16string& text) override {
    auth_container_->ContentsChanged();
  }

  void OnTextVisibleChanged(bool visible) override {
    auth_container_->ContentsChanged();
  }

 private:
  raw_ptr<AuthContainerView> auth_container_;
};

}  // namespace

AuthContainerView::TestApi::TestApi(AuthContainerView* view) : view_(view) {}

AuthContainerView::TestApi::~TestApi() = default;

raw_ptr<PinContainerView> AuthContainerView::TestApi::GetPinContainerView() {
  return view_->pin_container_;
}

raw_ptr<AuthInputRowView> AuthContainerView::TestApi::GetPasswordView() {
  return view_->password_view_;
}

raw_ptr<views::Button> AuthContainerView::TestApi::GetSwitchButton() {
  return view_->switch_button_;
}

raw_ptr<PinStatusView> AuthContainerView::TestApi::GetPinStatusView() {
  return view_->pin_status_;
}

raw_ptr<FingerprintView> AuthContainerView::TestApi::GetFingerprintView() {
  return view_->fingerprint_view_;
}

AuthInputType AuthContainerView::TestApi::GetCurrentInputType() {
  return view_->current_input_type_;
}

raw_ptr<AuthContainerView> AuthContainerView::TestApi::GetView() {
  return view_;
}

AuthContainerView::AuthContainerView(AuthFactorSet auth_factors)
    : available_auth_factors_(auth_factors) {
  CHECK(!auth_factors.empty());

  CHECK(auth_factors.Has(AuthInputType::kPassword) ||
        auth_factors.Has(AuthInputType::kPin));
  if (!auth_factors.Has(AuthInputType::kPassword)) {
    current_input_type_ = AuthInputType::kPin;
  }

  // Initialize layout.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout_ = SetLayoutManager(std::move(layout));

  // Add child views and adjust their visibility.
  AddPasswordView();

  AddPinView();

  AddSwitchButton();

  AddPinStatusView();

  AddFingerprintView();
}

AuthContainerView::~AuthContainerView() {
  pin_container_->RemoveObserver(pin_observer_.get());
  pin_container_ = nullptr;
  pin_observer_.reset();

  password_view_->RemoveObserver(password_observer_.get());
  password_view_ = nullptr;
  password_observer_.reset();

  pin_status_ = nullptr;

  fingerprint_view_ = nullptr;
}

void AuthContainerView::AddPasswordView() {
  CHECK_EQ(password_view_, nullptr);
  password_view_ = AddChildView(std::make_unique<AuthInputRowView>(
      AuthInputRowView::AuthType::kPassword));
  password_view_->SetVisible(current_input_type_ == AuthInputType::kPassword);

  password_observer_ = std::make_unique<PasswordObserverAdapter>(this);
  password_view_->AddObserver(password_observer_.get());
}

void AuthContainerView::AddPinView() {
  CHECK_EQ(pin_container_, nullptr);
  pin_container_ = AddChildView(std::make_unique<PinContainerView>());
  pin_container_->SetVisible(current_input_type_ == AuthInputType::kPin);

  pin_observer_ = std::make_unique<PinObserverAdapter>(this);
  pin_container_->AddObserver(pin_observer_.get());
}

void AuthContainerView::AddSwitchButton() {
  // Add separator between the switch and the above view.
  switch_button_spacer_ = AddVerticalSpace(this, kSwitchButtonTopDistanceDp);

  switch_button_ = AddChildView(
      views::Builder<ash::PillButton>()
          .SetText(l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SWITCH_TO_PIN))
          .SetPillButtonType(ash::PillButton::Type::kDefaultElevatedWithoutIcon)
          .SetCallback(
              base::BindRepeating(&AuthContainerView::ToggleCurrentAuthType,
                                  weak_ptr_factory_.GetWeakPtr()))
          .Build());

  switch_button_->SetVisible(HasPassword() && HasPin());
  switch_button_spacer_->SetVisible(switch_button_->GetVisible());
}

void AuthContainerView::AddPinStatusView() {
  CHECK_EQ(pin_status_, nullptr);
  pin_status_ = AddChildView(std::make_unique<PinStatusView>());
  pin_status_->SetVisible(false);
}

void AuthContainerView::AddFingerprintView() {
  CHECK_EQ(fingerprint_view_, nullptr);
  fingerprint_view_ = AddChildView(std::make_unique<FingerprintView>());
  if (available_auth_factors_.Has(AuthInputType::kFingerprint)) {
    SetFingerprintState(FingerprintState::AVAILABLE_DEFAULT);
  }
}

gfx::Size AuthContainerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int preferred_height = 0;

  if (pin_container_->GetVisible()) {
    preferred_height +=
        pin_container_->GetPreferredSize(available_size).height();
  }
  if (password_view_->GetVisible()) {
    preferred_height +=
        password_view_->GetPreferredSize(available_size).height();
  }

  if (pin_status_->GetVisible()) {
    preferred_height += pin_status_->GetPreferredSize(available_size).height();
  }

  if (fingerprint_view_->GetVisible()) {
    preferred_height +=
        fingerprint_view_->GetPreferredSize(available_size).height();
  }

  if (switch_button_->GetVisible()) {
    preferred_height +=
        switch_button_->GetPreferredSize(available_size).height();
    preferred_height +=
        switch_button_spacer_->GetPreferredSize(available_size).height();
  }

  return gfx::Size(kAuthContainerViewWidthDp, preferred_height);
}

void AuthContainerView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->AddState(ax::mojom::State::kInvisible);
}

std::string AuthContainerView::GetObjectName() const {
  return "AuthContainerView";
}

void AuthContainerView::RequestFocus() {
  if (current_input_type_ == AuthInputType::kPassword) {
    password_view_->RequestFocus();
  } else if (current_input_type_ == AuthInputType::kPin) {
    pin_container_->RequestFocus();
  }
}

void AuthContainerView::SetHasPassword(bool has_password) {
  if (has_password == HasPassword()) {
    return;
  }
  available_auth_factors_.PutOrRemove(AuthInputType::kPassword, has_password);

  UpdateAuthInput();
  UpdateSwitchButtonState();
  PreferredSizeChanged();
}

bool AuthContainerView::HasPassword() const {
  return available_auth_factors_.Has(AuthInputType::kPassword);
}

void AuthContainerView::SetHasPin(bool has_pin) {
  if (has_pin == HasPin()) {
    return;
  }
  available_auth_factors_.PutOrRemove(AuthInputType::kPin, has_pin);

  CHECK(fingerprint_view_);
  fingerprint_view_->SetHasPin(has_pin);

  UpdateAuthInput();
  UpdateSwitchButtonState();
  PreferredSizeChanged();
}

bool AuthContainerView::HasPin() const {
  return available_auth_factors_.Has(AuthInputType::kPin);
}

void AuthContainerView::SetPinStatus(
    std::unique_ptr<cryptohome::PinStatus> pin_status) {
  if (pin_status != nullptr) {
    SetHasPin(!pin_status->IsLockedFactor());
  }
  pin_status_->SetPinStatus(std::move(pin_status));
  PreferredSizeChanged();
}

const std::u16string& AuthContainerView::GetPinStatusMessage() const {
  return pin_status_->GetCurrentText();
}

void AuthContainerView::SetFingerprintState(FingerprintState state) {
  CHECK(fingerprint_view_);
  fingerprint_view_->SetState(state);
}

void AuthContainerView::NotifyFingerprintAuthSuccess(
    base::OnceCallback<void()> on_success_animation_finished) {
  CHECK(fingerprint_view_);
  fingerprint_view_->NotifyAuthSuccess(
      std::move(on_success_animation_finished));
}

void AuthContainerView::NotifyFingerprintAuthFailure() {
  CHECK(fingerprint_view_);
  fingerprint_view_->NotifyAuthFailure();
}

void AuthContainerView::SetInputEnabled(bool enabled) {
  SetEnabled(enabled);
  pin_container_->SetInputEnabled(enabled);
  password_view_->SetInputEnabled(enabled);
  switch_button_->SetEnabled(enabled);
}

void AuthContainerView::UpdateAuthInput() {
  // If necessary change to the available factor
  if (current_input_type_ == AuthInputType::kPassword && !HasPassword()) {
    current_input_type_ = AuthInputType::kPin;
  }
  if (current_input_type_ == AuthInputType::kPin && !HasPin()) {
    current_input_type_ = AuthInputType::kPassword;
  }
  // Show the current_input_type_'s view.
  if (current_input_type_ == AuthInputType::kPassword &&
      !password_view_->GetVisible()) {
    pin_container_->SetVisible(false);
    password_view_->SetVisible(true);
    password_view_->RequestFocus();
  } else if (current_input_type_ == AuthInputType::kPin &&
             !pin_container_->GetVisible()) {
    password_view_->SetVisible(false);
    pin_container_->SetVisible(true);
    pin_container_->RequestFocus();
  }
  PreferredSizeChanged();
}

void AuthContainerView::UpdateSwitchButtonState() {
  CHECK(HasPassword() || HasPin());
  switch_button_->SetVisible(HasPassword() && HasPin());
  switch_button_spacer_->SetVisible(switch_button_->GetVisible());
  if (HasPassword() && HasPin()) {
    switch_button_->SetText(l10n_util::GetStringUTF16(
        current_input_type_ == AuthInputType::kPassword
            ? (IDS_ASH_LOGIN_SWITCH_TO_PIN)
            : IDS_ASH_LOGIN_SWITCH_TO_PASSWORD));
  }
}

void AuthContainerView::PinSubmit(const std::u16string& pin) const {
  for (auto& observer : observers_) {
    observer.OnPinSubmit(pin);
  }
}

void AuthContainerView::PasswordSubmit(const std::u16string& password) const {
  for (auto& observer : observers_) {
    observer.OnPasswordSubmit(password);
  }
}

void AuthContainerView::Escape() const {
  for (auto& observer : observers_) {
    observer.OnEscape();
  }
}

void AuthContainerView::ContentsChanged() const {
  for (auto& observer : observers_) {
    observer.OnContentsChanged();
  }
}

void AuthContainerView::ToggleCurrentAuthType() {
  CHECK(HasPassword() && HasPin());
  if (current_input_type_ == AuthInputType::kPassword) {
    current_input_type_ = AuthInputType::kPin;
  } else {
    current_input_type_ = AuthInputType::kPassword;
  }
  // Clear the input fields.
  ResetInputfields();

  UpdateSwitchButtonState();
  UpdateAuthInput();
  for (auto& observer : observers_) {
    observer.OnContentsChanged();
  }
}

void AuthContainerView::ResetInputfields() {
  password_view_->ResetState();
  pin_container_->ResetState();
}

void AuthContainerView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthContainerView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(AuthContainerView)
END_METADATA

}  // namespace ash
