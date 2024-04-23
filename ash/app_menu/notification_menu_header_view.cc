// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_header_view.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"

namespace ash {

namespace {

// Color of text in NotificationMenuHeaderView.
constexpr SkColor kNotificationHeaderTextColor =
    SkColorSetRGB(0X1A, 0x73, 0xE8);

// Line height of all text in the NotificationMenuHeaderView in dips.
constexpr int kNotificationHeaderLineHeight = 20;

}  // namespace

NotificationMenuHeaderView::NotificationMenuHeaderView() {
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      kNotificationVerticalPadding, kNotificationHorizontalPadding)));

  notification_title_ = new views::Label(
      std::u16string(l10n_util::GetStringUTF16(
          IDS_MESSAGE_CENTER_NOTIFICATION_ACCESSIBLE_NAME_PLURAL)),
      {views::Label::GetDefaultFontList().DeriveWithSizeDelta(1)});
  notification_title_->SetEnabledColor(kNotificationHeaderTextColor);
  notification_title_->SetLineHeight(kNotificationHeaderLineHeight);
  AddChildView(notification_title_.get());

  counter_ = new views::Label(
      std::u16string(),
      {views::Label::GetDefaultFontList().DeriveWithSizeDelta(1)});
  counter_->SetEnabledColor(kNotificationHeaderTextColor);
  counter_->SetLineHeight(kNotificationHeaderLineHeight);
  AddChildView(counter_.get());
}

NotificationMenuHeaderView::~NotificationMenuHeaderView() = default;

void NotificationMenuHeaderView::UpdateCounter(int number_of_notifications) {
  if (number_of_notifications_ == number_of_notifications)
    return;

  number_of_notifications_ = number_of_notifications;

  counter_->SetText(base::NumberToString16(number_of_notifications_));
}

gfx::Size NotificationMenuHeaderView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(views::MenuConfig::instance().touchable_menu_min_width,
                   GetInsets().height() +
                       notification_title_->GetPreferredSize({}).height());
}

void NotificationMenuHeaderView::Layout(PassKey) {
  const gfx::Insets insets = GetInsets();

  const gfx::Size notification_title_preferred_size =
      notification_title_->GetPreferredSize({});
  notification_title_->SetBounds(insets.left(), insets.top(),
                                 notification_title_preferred_size.width(),
                                 notification_title_preferred_size.height());

  const gfx::Size counter_preferred_size = counter_->GetPreferredSize({});
  counter_->SetBounds(width() - counter_preferred_size.width() - insets.right(),
                      insets.top(), counter_preferred_size.width(),
                      counter_preferred_size.height());
}

BEGIN_METADATA(NotificationMenuHeaderView)
END_METADATA

}  // namespace ash
