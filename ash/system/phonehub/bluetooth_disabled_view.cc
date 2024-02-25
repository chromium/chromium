// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/bluetooth_disabled_view.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/phonehub/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::Screen;

BluetoothDisabledView::BluetoothDisabledView() {
  SetID(PhoneHubViewID::kBluetoothDisabledView);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* content_view = AddChildView(
      std::make_unique<PhoneHubInterstitialView>(/*show_progress=*/false));
  content_view->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_PHONE_HUB_ERROR_STATE_IMAGE));
  content_view->SetTitle(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_TITLE));
  content_view->SetDescription(l10n_util::GetStringFUTF16(
      IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_DESCRIPTION,
      ui::GetChromeOSDeviceName()));

  // Add "Learn more" button.
  auto learn_more = std::make_unique<PillButton>(
      base::BindRepeating(&BluetoothDisabledView::LearnMoreButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_LEARN_MORE_BUTTON),
      PillButton::Type::kSecondaryWithoutIcon, /*icon=*/nullptr);
  learn_more->SetID(PhoneHubViewID::kBluetoothDisabledLearnMoreButton);
  content_view->AddButton(std::move(learn_more));

  LogInterstitialScreenEvent(InterstitialScreenEvent::kShown);
}

BluetoothDisabledView::~BluetoothDisabledView() = default;

phone_hub_metrics::Screen BluetoothDisabledView::GetScreenForMetrics() const {
  return Screen::kBluetoothOrWifiDisabled;
}

void BluetoothDisabledView::LearnMoreButtonPressed() {
  LogInterstitialScreenEvent(InterstitialScreenEvent::kLearnMore);
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(phonehub::kPhoneHubLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

BEGIN_METADATA(BluetoothDisabledView)
END_METADATA

}  // namespace ash
