// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/bluetooth_disabled_view.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/interstitial_view_button.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/status_area_widget.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::LogInterstitialScreenEvent;
using phone_hub_metrics::Screen;

BluetoothDisabledView::BluetoothDisabledView() {
  SetID(PhoneHubViewID::kBluetoothDisabledView);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view_ = AddChildView(
      std::make_unique<PhoneHubInterstitialView>(/*show_progress=*/false));

  // TODO(crbug.com/1127996): Replace PNG file with vector icon.
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PHONE_HUB_ERROR_STATE_IMAGE);
  content_view_->SetImage(*image);
  content_view_->SetTitle(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_TITLE));
  content_view_->SetDescription(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_DESCRIPTION));

  // Add "Learn more" and "Ok, got it" buttons.
  auto learn_more = std::make_unique<InterstitialViewButton>(
      base::BindRepeating(&BluetoothDisabledView::LearnMoreButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_LEARN_MORE_BUTTON),
      /*paint_background=*/false);
  learn_more->SetEnabledTextColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  learn_more->SetID(PhoneHubViewID::kBluetoothDisabledLearnMoreButton);
  content_view_->AddButton(std::move(learn_more));

  auto confirm = std::make_unique<InterstitialViewButton>(
      base::BindRepeating(&BluetoothDisabledView::ConfirmButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_OK_BUTTON),
      /*paint_background=*/true);
  confirm->SetID(PhoneHubViewID::kBluetoothDisabledConfirmButton);
  content_view_->AddButton(std::move(confirm));

  LogInterstitialScreenEvent(GetScreenForMetrics(),
                             InterstitialScreenEvent::kShown);
}

BluetoothDisabledView::~BluetoothDisabledView() = default;

phone_hub_metrics::Screen BluetoothDisabledView::GetScreenForMetrics() const {
  return Screen::kBluetoothOrWifiDisabled;
}

void BluetoothDisabledView::LearnMoreButtonPressed() {
  LogInterstitialScreenEvent(GetScreenForMetrics(),
                             InterstitialScreenEvent::kLearnMore);
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kLearnMoreUrl), /*from_user_interaction=*/true);
}

void BluetoothDisabledView::ConfirmButtonPressed() {
  LogInterstitialScreenEvent(GetScreenForMetrics(),
                             InterstitialScreenEvent::kConfirm);
  Shell::GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->phone_hub_tray()
      ->CloseBubble();
}

BEGIN_METADATA(BluetoothDisabledView, views::View)
END_METADATA

}  // namespace ash
