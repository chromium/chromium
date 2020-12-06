// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/notification_opt_in_view.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/interstitial_view_button.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/phonehub/notification_access_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "url/gurl.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::LogInterstitialScreenEvent;

namespace {

// Appearance.
constexpr int kButtonSpacingDip = 8;
constexpr int kBorderThicknessDip = 1;
constexpr int kBorderCornerRadiusDip = 8;
constexpr gfx::Insets kTextLabelBorderInsets = {12, 16, 12, 16};
constexpr gfx::Insets kButtonContainerBorderInsets = {0, 0, 12, 16};
constexpr int kTextLabelLineHeightDip = 20;

// Typography.
constexpr int kLabelTextFontSizeDip = 14;

// URL of the multidevice settings page with the URL parameter that will
// start up the opt-in-flow.
constexpr char kMultideviceSettingsUrl[] =
    "chrome://os-settings/multidevice/"
    "features?showNotificationAccessSetupDialog";

}  // namespace

NotificationOptInView::NotificationOptInView(
    chromeos::phonehub::NotificationAccessManager* notification_access_manager)
    : notification_access_manager_(notification_access_manager) {
  SetID(PhoneHubViewID::kNotificationOptInView);
  InitLayout();

  DCHECK(notification_access_manager_);
  access_manager_observer_.Add(notification_access_manager_);

  // Checks and updates its visibility upon creation.
  UpdateVisibility();

  LogNotificationOptInEvent(InterstitialScreenEvent::kShown);
}

NotificationOptInView::~NotificationOptInView() = default;

void NotificationOptInView::SetUpButtonPressed() {
  // Opens the notification set up dialog in settings to start the opt in flow.
  LogNotificationOptInEvent(InterstitialScreenEvent::kConfirm);
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kMultideviceSettingsUrl), /*from_user_interaction=*/true);
}

void NotificationOptInView::DismissButtonPressed() {
  // Dismiss this view if user chose to opt out and update the bubble size.
  LogNotificationOptInEvent(InterstitialScreenEvent::kDismiss);
  SetVisible(false);
  notification_access_manager_->DismissSetupRequiredUi();
}

void NotificationOptInView::OnNotificationAccessChanged() {
  UpdateVisibility();
}

void NotificationOptInView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  const SkColor border_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThicknessDip, kBorderCornerRadiusDip, border_color));

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  // Set up layout row for the text label.
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  text_label_ =
      layout->AddView(std::make_unique<views::Label>(), 1, 1,
                      views::GridLayout::CENTER, views::GridLayout::CENTER);
  auto text_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  text_label_->SetEnabledColor(text_color);
  text_label_->SetAutoColorReadabilityEnabled(false);
  auto default_font = text_label_->font_list();
  text_label_->SetFontList(default_font
                               .DeriveWithSizeDelta(kLabelTextFontSizeDip -
                                                    default_font.GetFontSize())
                               .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  text_label_->SetLineHeight(kTextLabelLineHeightDip);
  text_label_->SetBorder(views::CreateEmptyBorder(kTextLabelBorderInsets));
  text_label_->SetMultiLine(/*multi_line=*/true);
  text_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  text_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DESCRIPTION,
      ui::GetChromeOSDeviceName()));

  // Set up layout row for the buttons.
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  auto* button_container =
      layout->AddView(std::make_unique<views::View>(), 1, 1,
                      views::GridLayout::TRAILING, views::GridLayout::CENTER);
  button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kButtonSpacingDip));
  button_container->SetBorder(
      views::CreateEmptyBorder(kButtonContainerBorderInsets));
  dismiss_button_ =
      button_container->AddChildView(std::make_unique<InterstitialViewButton>(
          base::BindRepeating(&NotificationOptInView::DismissButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DISMISS_BUTTON),
          /*paint_background=*/false));
  dismiss_button_->SetEnabledTextColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  dismiss_button_->SetID(kNotificationOptInDismissButton);
  set_up_button_ =
      button_container->AddChildView(std::make_unique<InterstitialViewButton>(
          base::BindRepeating(&NotificationOptInView::SetUpButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_SET_UP_BUTTON),
          /*paint_background=*/true));
  set_up_button_->SetID(kNotificationOptInSetUpButton);
}

void NotificationOptInView::UpdateVisibility() {
  DCHECK(notification_access_manager_);

  // Can only request access if it is available but has not yet been granted.
  bool can_request_access = notification_access_manager_->GetAccessStatus() ==
                            chromeos::phonehub::NotificationAccessManager::
                                AccessStatus::kAvailableButNotGranted;
  const bool should_show =
      can_request_access &&
      !notification_access_manager_->HasNotificationSetupUiBeenDismissed();
  SetVisible(should_show);
}

BEGIN_METADATA(NotificationOptInView, views::View)
END_METADATA

}  // namespace ash
