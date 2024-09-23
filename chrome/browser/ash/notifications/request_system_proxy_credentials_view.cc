// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/request_system_proxy_credentials_view.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/passphrase_textfield.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

class ErrorLabelView : public views::Label {
  METADATA_HEADER(ErrorLabelView, views::Label)

 public:
  explicit ErrorLabelView(bool show_error_label)
      : Label(l10n_util::GetStringUTF16(
            IDS_SYSTEM_PROXY_AUTH_DIALOG_ERROR_LABEL)) {
    SetEnabled(true);
    SetVisible(show_error_label);
  }
  ErrorLabelView(const ErrorLabelView&) = delete;
  ErrorLabelView& operator=(const ErrorLabelView&) = delete;
  ~ErrorLabelView() override = default;

  // views::View:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    SetEnabledColor(GetColorProvider()->GetColor(ui::kColorAlertHighSeverity));
  }
};

BEGIN_METADATA(ErrorLabelView)
END_METADATA

}  // namespace

namespace ash {

RequestSystemProxyCredentialsView::RequestSystemProxyCredentialsView(
    const std::string& proxy_server,
    bool show_error_label,
    base::OnceClosure view_destruction_callback)
    : window_title_(
          l10n_util::GetStringUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_TITLE)),
      proxy_server_(proxy_server),
      show_error_label_(show_error_label),
      view_destruction_callback_(std::move(view_destruction_callback)) {
  Init();
}

RequestSystemProxyCredentialsView::~RequestSystemProxyCredentialsView() {
  std::move(view_destruction_callback_).Run();
}

views::View* RequestSystemProxyCredentialsView::GetInitiallyFocusedView() {
  return username_textfield_;
}

std::u16string RequestSystemProxyCredentialsView::GetWindowTitle() const {
  return window_title_;
}

bool RequestSystemProxyCredentialsView::ShouldShowCloseButton() const {
  return false;
}

const std::string& RequestSystemProxyCredentialsView::GetProxyServer() const {
  return proxy_server_;
}

std::u16string RequestSystemProxyCredentialsView::GetUsername() const {
  return username_textfield_->GetText();
}

std::u16string RequestSystemProxyCredentialsView::GetPassword() const {
  return password_textfield_->GetText();
}

void RequestSystemProxyCredentialsView::Init() {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText)));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_OK_BUTTON));

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  auto* info_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_TEXT,
                                 base::ASCIIToUTF16(GetProxyServer()))));
  info_label->SetEnabled(true);
  info_label->SetTextStyle(views::style::STYLE_PRIMARY);
  info_label->SetProperty(views::kCrossAxisAlignmentKey,
                          views::LayoutAlignment::kStart);

  auto* info_label_privacy = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_PRIVACY_WARNING)));
  info_label_privacy->SetEnabled(true);
  info_label_privacy->SetTextStyle(views::style::STYLE_SECONDARY);
  info_label_privacy->SetProperty(views::kCrossAxisAlignmentKey,
                                  views::LayoutAlignment::kStart);

  auto* auth_container =
      AddChildView(std::make_unique<views::TableLayoutView>());
  auth_container->AddColumn(
      views::LayoutAlignment::kStart, views::LayoutAlignment::kStretch,
      views::TableLayout::kFixedSize,
      views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  const int label_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auth_container->AddPaddingColumn(views::TableLayout::kFixedSize,
                                   label_padding);
  auth_container->AddColumn(
      views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch, 1.0f,
      views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  const int unrelated_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  auth_container->AddPaddingRow(views::TableLayout::kFixedSize,
                                unrelated_vertical_spacing);
  auth_container->AddRows(1, views::TableLayout::kFixedSize);
  auto* username_label = auth_container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_SYSTEM_PROXY_AUTH_DIALOG_USERNAME_LABEL)));
  username_label->SetEnabled(true);

  username_textfield_ =
      auth_container->AddChildView(std::make_unique<views::Textfield>());
  username_textfield_->SetEnabled(true);
  username_textfield_->GetViewAccessibility().SetName(*username_label);

  const int related_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  auth_container->AddPaddingRow(views::TableLayout::kFixedSize,
                                related_vertical_spacing);
  auth_container->AddRows(1, views::TableLayout::kFixedSize);
  auto* password_label = auth_container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_SYSTEM_PROXY_AUTH_DIALOG_PASSWORD_LABEL)));
  password_label->SetEnabled(true);
  password_textfield_ = auth_container->AddChildView(
      std::make_unique<chromeos::PassphraseTextfield>());
  password_textfield_->SetEnabled(true);
  password_textfield_->GetViewAccessibility().SetName(*password_label);
  auth_container->AddPaddingRow(views::TableLayout::kFixedSize,
                                related_vertical_spacing);

  auto* error_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  error_container->SetBetweenChildSpacing(label_padding);
  auto* error_icon =
      error_container->AddChildView(std::make_unique<views::ImageView>());
  constexpr int kIconSize = 18;
  error_icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kInfoOutlineIcon, ui::kColorAlertHighSeverity, kIconSize));
  error_icon->SetImageSize(gfx::Size(kIconSize, kIconSize));
  error_icon->SetVisible(show_error_label_);

  error_label_ = error_container->AddChildView(
      std::make_unique<ErrorLabelView>(show_error_label_));
  error_container->SetFlexForView(error_label_, 1);
}

BEGIN_METADATA(RequestSystemProxyCredentialsView)
ADD_READONLY_PROPERTY_METADATA(std::string, ProxyServer)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Username)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Password)
END_METADATA

}  // namespace ash
