// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/connection_error_view.h"

#include <memory>

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

// Tag value used to uniquely identify the "Learn More" and "Refresh" buttons.
constexpr int kLearnMoreButtonTag = 1;
constexpr int kRefreshButtonTag = 2;

}  // namespace

ConnectionErrorView::ConnectionErrorView(ErrorStatus error) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view_ = AddChildView(std::make_unique<PhoneHubInterstitialView>(
      /*show_progress=*/error == ErrorStatus::kReconnecting));

  // TODO(crbug.com/1127996): Replace PNG file with vector icon.
  const int image_id = (error == ErrorStatus::kDisconnected)
                           ? IDR_PHONE_HUB_ERROR_STATE_IMAGE
                           : IDR_PHONE_HUB_CONNECTING_IMAGE;
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id);
  content_view_->SetImage(*image);

  const int title = (error == ErrorStatus::kDisconnected)
                        ? IDS_ASH_PHONE_HUB_CONNECTION_ERROR_DIALOG_TITLE
                        : IDS_ASH_PHONE_HUB_INITIAL_CONNECTING_DIALOG_TITLE;
  content_view_->SetTitle(l10n_util::GetStringUTF16(title));
  content_view_->SetDescription(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_CONNECTION_ERROR_DIALOG_DESCRIPTION));

  if (error == ErrorStatus::kReconnecting)
    return;

  // Add "Learn more" and "Refresh" buttons only for disconnected state.
  auto learn_more = std::make_unique<views::LabelButton>(
      this, l10n_util::GetStringUTF16(
                IDS_ASH_PHONE_HUB_CONNECTION_ERROR_DIALOG_LEARN_MORE_BUTTON));
  learn_more->SetEnabledTextColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  learn_more->set_tag(kLearnMoreButtonTag);
  content_view_->AddButton(std::move(learn_more));

  auto refresh = std::make_unique<RoundedLabelButton>(
      this, l10n_util::GetStringUTF16(
                IDS_ASH_PHONE_HUB_CONNECTION_ERROR_DIALOG_REFRESH_BUTTON));
  refresh->set_tag(kRefreshButtonTag);
  content_view_->AddButton(std::move(refresh));
}

ConnectionErrorView::~ConnectionErrorView() = default;

void ConnectionErrorView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  // TODO(meilinw): implement button pressed actions.
}

BEGIN_METADATA(ConnectionErrorView, views::View)
END_METADATA

}  // namespace ash
