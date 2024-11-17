// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/active_session_auth_view.h"

#include <memory>
#include <string>

#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/auth_view_utils.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/account_id/account_id.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The in session view width.
constexpr int kActiveSessionAuthViewWidthDp = 322;

// The in session view corner radius.
constexpr int kActiveSessionAuthViewCornerRadiusDp = 20;

// Distance between the top of the view and the close button.
constexpr int kCloseButtonTopDistanceDp = 16;

// Distance between the right hand side of the view and the close button.
constexpr int kCloseButtonRightDistanceDp = 16;

// Distance between the top of the view and the header.
constexpr int kHeaderTopDistanceDp = 32;

// Distance between the left hand side of the view and the header.
constexpr int kHeaderHorizontalDistanceDp = 32;

// Distance between the header and the auth container.
constexpr int kHeaderAuthContainerDistanceDp = 28;

// Distance between the bottom of the view and the auth container.
constexpr int kAuthContainerBottomDistanceDp = 32;

}  // namespace

ActiveSessionAuthView::TestApi::TestApi(ActiveSessionAuthView* view)
    : view_(view) {}

ActiveSessionAuthView::TestApi::~TestApi() {}

raw_ptr<AuthHeaderView> ActiveSessionAuthView::TestApi::GetAuthHeaderView() {
  return view_->auth_header_;
}

raw_ptr<AuthContainerView>
ActiveSessionAuthView::TestApi::GetAuthContainerView() {
  return view_->auth_container_;
}

raw_ptr<views::Button> ActiveSessionAuthView::TestApi::GetCloseButton() {
  return view_->close_button_;
}

raw_ptr<ActiveSessionAuthView> ActiveSessionAuthView::TestApi::GetView() {
  return view_;
}

ActiveSessionAuthView::ActiveSessionAuthView(const AccountId& account_id,
                                             const std::u16string& title,
                                             const std::u16string& description,
                                             AuthFactorSet auth_factors)
    : account_id_(account_id) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // Initialize layout.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout_ = SetLayoutManager(std::move(layout));

  // Add the header and a close icon on the right top corner.
  AddHeaderAndCloseButton(title, description);

  // Add vertical space between the header and the auth container.
  AddVerticalSpace(this, kHeaderAuthContainerDistanceDp);

  // Add auth container view and register to observe the events.
  AddAuthContainer(auth_factors);

  // Add vertical space to the bottom of the view.
  AddVerticalSpace(this, kAuthContainerBottomDistanceDp);

  // Set the background.
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysBaseElevated, kActiveSessionAuthViewCornerRadiusDp));

  // Set the view as a dialog for a11y purposes.
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_IN_SESSION_AUTH_DIALOG_ACCESSIBLE_NAME));
  GetViewAccessibility().SetDescription(l10n_util::GetStringUTF16(
      IDS_ASH_IN_SESSION_AUTH_DIALOG_ACCESSIBLE_DESCRIPTION));
}

ActiveSessionAuthView::~ActiveSessionAuthView() {
  auth_container_->RemoveObserver(this);
  auth_container_ = nullptr;
  auth_header_ = nullptr;
  close_button_ = nullptr;
}

void ActiveSessionAuthView::AddHeaderAndCloseButton(
    const std::u16string& title,
    const std::u16string& description) {
  CHECK_EQ(auth_header_, nullptr);
  CHECK_EQ(close_button_, nullptr);

  auto header_layout = std::make_unique<views::FillLayout>();
  views::View* header = AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::move(header_layout));

  header->SetPaintToLayer();
  header->layer()->SetFillsBoundsOpaquely(false);

  // Auth header position and add.
  views::View* auth_header_container =
      AddChildView(std::make_unique<views::View>());

  auto auth_header_container_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(kHeaderTopDistanceDp, kHeaderHorizontalDistanceDp, 0,
                        kHeaderHorizontalDistanceDp));
  auth_header_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  auth_header_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  auth_header_container->SetLayoutManager(
      std::move(auth_header_container_layout));
  auth_header_ = auth_header_container->AddChildView(
      std::make_unique<AuthHeaderView>(account_id_, title, description));
  header_observation_.Observe(auth_header_);

  header->AddChildView(auth_header_container);

  // Close button position and creation.
  views::View* close_button_view =
      AddChildView(std::make_unique<views::View>());

  auto close_button_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kCloseButtonTopDistanceDp, kCloseButtonRightDistanceDp));
  close_button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  close_button_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);
  close_button_view->SetLayoutManager(std::move(close_button_layout));
  header->AddChildView(close_button_view);

  IconButton::Builder builder;
  builder.SetType(IconButton::Type::kXSmall)
      .SetAccessibleNameId(
          IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_CLOSE_DIALOG_BUTTON)
      .SetTogglable(false)
      .SetEnabled(true)
      .SetCallback(base::BindRepeating(&ActiveSessionAuthView::Close,
                                       base::Unretained(this)))
      .SetVectorIcon(&views::kIcCloseIcon);

  close_button_ = close_button_view->AddChildView(builder.Build());
}

void ActiveSessionAuthView::AddAuthContainer(AuthFactorSet auth_factors) {
  CHECK_EQ(auth_container_, nullptr);
  auth_container_ =
      AddChildView(std::make_unique<AuthContainerView>(auth_factors));
  auth_container_->AddObserver(this);
}

gfx::Size ActiveSessionAuthView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // The header part of the view.
  int preferred_height = kHeaderTopDistanceDp;
  preferred_height += auth_header_->GetPreferredSize(available_size).height();
  preferred_height += kHeaderAuthContainerDistanceDp;

  // The auth container part of the view.
  preferred_height +=
      auth_container_->GetPreferredSize(available_size).height();
  preferred_height += kAuthContainerBottomDistanceDp;

  return gfx::Size(kActiveSessionAuthViewWidthDp, preferred_height);
}

void ActiveSessionAuthView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

std::string ActiveSessionAuthView::GetObjectName() const {
  return "ActiveSessionAuthView";
}

void ActiveSessionAuthView::RequestFocus() {
  auth_container_->RequestFocus();
}

void ActiveSessionAuthView::SetHasPassword(bool has_password) {
  auth_container_->SetHasPassword(has_password);
}

bool ActiveSessionAuthView::HasPassword() const {
  return auth_container_->HasPassword();
}

void ActiveSessionAuthView::SetHasPin(bool has_pin) {
  auth_container_->SetHasPin(has_pin);
}

bool ActiveSessionAuthView::HasPin() const {
  return auth_container_->HasPin();
}

void ActiveSessionAuthView::SetPinStatus(
    std::unique_ptr<cryptohome::PinStatus> pin_status) {
  auth_container_->SetPinStatus(std::move(pin_status));
}

const std::u16string& ActiveSessionAuthView::GetPinStatusMessage() const {
  return auth_container_->GetPinStatusMessage();
}

void ActiveSessionAuthView::SetInputEnabled(bool enabled) {
  auth_container_->SetInputEnabled(enabled);
  if (enabled) {
    RequestFocus();
  }
}

void ActiveSessionAuthView::OnPinSubmit(const std::u16string& pin) {
  for (auto& observer : observers_) {
    observer.OnPinSubmit(pin);
  }
}

void ActiveSessionAuthView::OnPasswordSubmit(const std::u16string& password) {
  for (auto& observer : observers_) {
    observer.OnPasswordSubmit(password);
  }
}

void ActiveSessionAuthView::SetErrorTitle(const std::u16string& error_str) {
  auth_header_->SetErrorTitle(error_str);
}

void ActiveSessionAuthView::OnEscape() {
  Close();
}

void ActiveSessionAuthView::Close() {
  for (auto& observer : observers_) {
    observer.OnClose();
  }
}

void ActiveSessionAuthView::SetFingerprintState(FingerprintState state) {
  auth_container_->SetFingerprintState(state);
}

void ActiveSessionAuthView::NotifyFingerprintAuthSuccess(
    base::OnceClosure on_success_animation_finished) {
  auth_container_->NotifyFingerprintAuthSuccess(
      std::move(on_success_animation_finished));
}

void ActiveSessionAuthView::NotifyFingerprintAuthFailure() {
  auth_container_->NotifyFingerprintAuthFailure();
}

void ActiveSessionAuthView::OnContentsChanged() {
  // If something changes on the UI e.g:
  // - user change the text of the input text
  // - the user switched PIN/password
  // - the input text visibility changed
  // then we would like to restore the original header and not show the error
  // anymore.
  auth_header_->RestoreTitle();
}

void ActiveSessionAuthView::ResetInputfields() {
  auth_container_->ResetInputfields();
}

void ActiveSessionAuthView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ActiveSessionAuthView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ActiveSessionAuthView::OnTitleChanged(const std::u16string& error_str) {
  GetViewAccessibility().SetName(error_str);
}

BEGIN_METADATA(ActiveSessionAuthView)
END_METADATA

}  // namespace ash
