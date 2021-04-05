// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/request_system_proxy_credentials_view.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/notifications/passphrase_textfield.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

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
  SetBorder(views::CreateEmptyBorder(
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT)));
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_OK_BUTTON));

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);

  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        100, views::GridLayout::ColumnSize::kUsePreferred, 0,
                        0);

  layout->StartRow(0, column_view_set_id);
  auto info_label = std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
      IDS_SYSTEM_PROXY_AUTH_DIALOG_TEXT, base::ASCIIToUTF16(GetProxyServer())));
  info_label->SetEnabled(true);
  info_label->SetTextStyle(views::style::STYLE_PRIMARY);
  layout->AddView(std::move(info_label));

  layout->StartRow(0, column_view_set_id);
  auto info_label_privacy = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_PRIVACY_WARNING));
  info_label_privacy->SetEnabled(true);
  info_label_privacy->SetTextStyle(views::style::STYLE_SECONDARY);
  layout->AddView(std::move(info_label_privacy));

  column_view_set_id++;
  column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  const int label_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  column_set->AddPaddingColumn(0, label_padding);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  const int unrelated_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  layout->StartRowWithPadding(1.0, column_view_set_id, 0,
                              unrelated_vertical_spacing);
  auto username_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_USERNAME_LABEL));
  username_label->SetEnabled(true);

  auto username_textfield = std::make_unique<views::Textfield>();
  username_textfield->SetEnabled(true);
  username_textfield->SetAssociatedLabel(
      layout->AddView(std::move(username_label)));
  username_textfield_ = layout->AddView(std::move(username_textfield));

  const int related_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  layout->StartRowWithPadding(1.0, column_view_set_id, 0,
                              related_vertical_spacing);
  auto password_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_PASSWORD_LABEL));
  password_label->SetEnabled(true);
  auto password_textfield = std::make_unique<PassphraseTextfield>();
  password_textfield->SetEnabled(true);
  password_textfield->SetAssociatedLabel(
      layout->AddView(std::move(password_label)));
  password_textfield_ = layout->AddView(std::move(password_textfield));

  column_view_set_id++;
  column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(0, label_padding);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        1.0, views::GridLayout::ColumnSize::kUsePreferred, 0,
                        0);
  layout->StartRowWithPadding(1.0, column_view_set_id, 0,
                              related_vertical_spacing);
  auto error_icon = std::make_unique<views::ImageView>();
  const int kIconSize = 18;
  error_icon->SetImage(
      gfx::CreateVectorIcon(vector_icons::kInfoOutlineIcon, kIconSize,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_AlertSeverityHigh)));
  error_icon->SetImageSize(gfx::Size(kIconSize, kIconSize));
  error_icon->SetVisible(show_error_label_);
  layout->AddView(std::move(error_icon));
  auto error_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SYSTEM_PROXY_AUTH_DIALOG_ERROR_LABEL));
  error_label->SetEnabled(true);
  error_label->SetVisible(show_error_label_);
  error_label->SetEnabledColor(error_label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_AlertSeverityHigh));
  error_label_ = layout->AddView(std::move(error_label));
}

BEGIN_METADATA(RequestSystemProxyCredentialsView, views::DialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(std::string, ProxyServer)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Username)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Password)
END_METADATA

}  // namespace ash
