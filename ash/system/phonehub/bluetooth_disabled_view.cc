// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/bluetooth_disabled_view.h"

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/unified/rounded_label_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Tag value used to uniquely identify the "Learn More" and "Ok, got it"
// buttons.
constexpr int kLearnMoreButtonTag = 1;
constexpr int kOkButtonTag = 2;

}  // namespace

BluetoothDisabledView::BluetoothDisabledView() {
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
  auto learn_more = std::make_unique<views::LabelButton>(
      this, l10n_util::GetStringUTF16(
                IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_LEARN_MORE_BUTTON));
  learn_more->SetEnabledTextColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  learn_more->set_tag(kLearnMoreButtonTag);
  content_view_->AddButton(std::move(learn_more));

  auto refresh = std::make_unique<RoundedLabelButton>(
      this, l10n_util::GetStringUTF16(
                IDS_ASH_PHONE_HUB_BLUETOOTH_DISABLED_DIALOG_OK_BUTTON));
  refresh->set_tag(kOkButtonTag);
  content_view_->AddButton(std::move(refresh));
}

BluetoothDisabledView::~BluetoothDisabledView() = default;

void BluetoothDisabledView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  // TODO(crbug.com/1126208): implement button pressed actions.
}

BEGIN_METADATA(BluetoothDisabledView, views::View)
END_METADATA

}  // namespace ash
