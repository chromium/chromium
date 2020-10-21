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
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/unified/rounded_label_button.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/phonehub/notification_access_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "url/gurl.h"

namespace ash {

using phone_hub_metrics::InterstitialScreen;
using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::LogInterstitialScreenEvent;

namespace {

// Appearance.
// TODO(crbug.com/1126208): update constants to spec.
constexpr int kButtonSpacingDip = 10;
constexpr int kBorderThicknessDip = 1;
constexpr int kBorderCornerRadiusDip = 10;
constexpr gfx::Insets kTextLabelBorderInsets = {10, 0, 0, 0};
constexpr gfx::Insets kButtonContainerBorderInsets = {10, 0, 5, 5};

// Tag value used to uniquely identify the "Dismiss" and "Get started" buttons.
constexpr int kDismissButtonTag = 1;
constexpr int kSetUpButtonTag = 2;

// URL of the multidevice settings page with the URL parameter that will
// start up the opt-in-flow.
constexpr char kMultideviceSettingsUrl[] =
    "chrome://os-settings/multidevice/"
    "features?showNotificationAccessSetupDialog";

}  // namespace

NotificationOptInView::NotificationOptInView(
    TrayBubbleView* bubble_view,
    chromeos::phonehub::NotificationAccessManager* notification_access_manager)
    : bubble_view_(bubble_view),
      notification_access_manager_(notification_access_manager) {
  SetID(PhoneHubViewID::kNotificationOptInView);
  InitLayout();
  LogInterstitialScreenEvent(InterstitialScreen::kNotificationOptIn,
                             InterstitialScreenEvent::kShown);
}

NotificationOptInView::~NotificationOptInView() = default;

void NotificationOptInView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  switch (sender->tag()) {
    case kDismissButtonTag:
      // Dismiss this view if user chose to opt out and update the bubble size.
      LogInterstitialScreenEvent(InterstitialScreen::kNotificationOptIn,
                                 InterstitialScreenEvent::kDismiss);
      SetVisible(false);
      bubble_view_->UpdateBubble();
      notification_access_manager_->DismissSetupRequiredUi();
      break;
    case kSetUpButtonTag:
      // Opens the notification set up dialog in settings to start the opt in
      // flow.
      LogInterstitialScreenEvent(InterstitialScreen::kNotificationOptIn,
                                 InterstitialScreenEvent::kConfirm);
      NewWindowDelegate::GetInstance()->NewTabWithUrl(
          GURL(kMultideviceSettingsUrl), /*from_user_interaction=*/true);
      break;
  }
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
  TrayPopupItemStyle body_style(
      TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  body_style.SetupLabel(text_label_);
  text_label_->SetBorder(views::CreateEmptyBorder(kTextLabelBorderInsets));
  text_label_->SetText(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DESCRIPTION));

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
          this,
          l10n_util::GetStringUTF16(
              IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DISMISS_BUTTON),
          /*paint_background=*/false));
  dismiss_button_->set_tag(kDismissButtonTag);
  dismiss_button_->SetEnabledTextColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  set_up_button_ =
      button_container->AddChildView(std::make_unique<InterstitialViewButton>(
          this,
          l10n_util::GetStringUTF16(
              IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_SET_UP_BUTTON),
          /*paint_background=*/true));
  set_up_button_->set_tag(kSetUpButtonTag);
}

BEGIN_METADATA(NotificationOptInView, views::View)
END_METADATA

}  // namespace ash
