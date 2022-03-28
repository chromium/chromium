// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/authentication_dialog.h"
#include <memory>
#include "ash/public/cpp/shelf_config.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

void AddMargins(views::View* view) {
  const auto* layout_provider = views::LayoutProvider::Get();
  const int horizontal_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  view->SetProperty(views::kMarginsKey,
                    gfx::Insets::VH(vertical_spacing, horizontal_spacing));
}

void ConfigurePasswordField(views::Textfield* password_field) {
  const auto password_field_name =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER);
  password_field->SetAccessibleName(password_field_name);
  password_field->SetReadOnly(false);
  password_field->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD);
  password_field->SetPlaceholderText(password_field_name);
  AddMargins(password_field);
}

void ConfigureInvalidPasswordLabel(views::Label* invalid_password_label) {
  invalid_password_label->SetProperty(views::kCrossAxisAlignmentKey,
                                      views::LayoutAlignment::kStart);
  invalid_password_label->SetEnabledColor(SK_ColorRED);
  AddMargins(invalid_password_label);
}

void CenterWidgetOnPrimaryDisplay(views::Widget* widget) {
  auto bounds = display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  bounds.ClampToCenteredSize(widget->GetContentsView()->GetPreferredSize());
  widget->SetBounds(bounds);
}

}  // namespace

// static
AuthenticationDialog* AuthenticationDialog::Show(
    OnSubmitCallback submit_callback) {
  auto* authentication_dialog =
      new AuthenticationDialog(std::move(submit_callback));
  auto* widget = DialogDelegateView::CreateDialogWidget(authentication_dialog,
                                                        /*context=*/nullptr,
                                                        /*parent=*/nullptr);
  CenterWidgetOnPrimaryDisplay(widget);
  widget->Show();
  authentication_dialog->Init();
  return authentication_dialog;
}

AuthenticationDialog::~AuthenticationDialog() = default;

void AuthenticationDialog::Init() {
  ConfigureOkButton();
  password_field_->RequestFocus();
}

void AuthenticationDialog::NotifyResult(Result result,
                                        const std::u16string& token,
                                        base::TimeDelta timeout) {
  std::move(on_submit_).Run(result, token, timeout);
}

void AuthenticationDialog::CancelAuthAttempt() {
  NotifyResult(Result::kAborted, u"", base::Seconds(0));
}

void AuthenticationDialog::OnSubmit() {
  // TODO(crbug.com/1271551): Call appropriate backends to get token
  // and notify interested parties with |AuthenticationDialog::NotifyResult|
  // For now, we always assume the given password is invalid
  password_field_->SetInvalid(true);
  password_field_->SelectAll(false);
  invalid_password_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ERROR_AUTHENTICATING));
}

void AuthenticationDialog::ConfigureChildViews() {
  ConfigurePasswordField(password_field_);
  ConfigureInvalidPasswordLabel(invalid_password_label_);
}

void AuthenticationDialog::ConfigureOkButton() {
  views::LabelButton* ok_button = GetOkButton();
  ok_button->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  ok_button->SetCallback(base::BindRepeating(&AuthenticationDialog::OnSubmit,
                                             base::Unretained(this)));
}

AuthenticationDialog::AuthenticationDialog(OnSubmitCallback submit_callback)
    : password_field_(AddChildView(std::make_unique<views::Textfield>())),
      invalid_password_label_(AddChildView(std::make_unique<views::Label>())),
      on_submit_(std::move(submit_callback)) {
  // Dialog setup
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetTitle(l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_TITLE));
  SetModalType(ui::MODAL_TYPE_SYSTEM);

  // Callback setup
  SetCancelCallback(base::BindOnce(&AuthenticationDialog::CancelAuthAttempt,
                                   base::Unretained(this)));
  SetCloseCallback(base::BindOnce(&AuthenticationDialog::CancelAuthAttempt,
                                  base::Unretained(this)));

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true);

  ConfigureChildViews();
}

}  // namespace ash
