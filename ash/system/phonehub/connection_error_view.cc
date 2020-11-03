// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/connection_error_view.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/interstitial_view_button.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "chromeos/components/phonehub/connection_scheduler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::Screen;

ConnectionErrorView::ConnectionErrorView(
    ErrorStatus error,
    chromeos::phonehub::ConnectionScheduler* connection_scheduler)
    : connection_scheduler_(connection_scheduler) {
  SetID(error == ErrorStatus::kDisconnected
            ? PhoneHubViewID::kDisconnectedView
            : PhoneHubViewID::kReconnectingView);
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

  if (error == ErrorStatus::kReconnecting) {
    phone_hub_metrics::LogInterstitialScreenEvent(
        Screen::kReconnecting, InterstitialScreenEvent::kShown);
    return;
  }

  // Add "Learn more" and "Refresh" buttons only for disconnected state.
  auto learn_more = std::make_unique<InterstitialViewButton>(
      base::BindRepeating(
          &ConnectionErrorView::ButtonPressed, base::Unretained(this),
          InterstitialScreenEvent::kLearnMore,
          base::BindRepeating(
              &NewWindowDelegate::NewTabWithUrl,
              base::Unretained(NewWindowDelegate::GetInstance()),
              GURL(kLearnMoreUrl), /*from_user_interaction=*/true)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_CONNECTION_ERROR_DIALOG_LEARN_MORE_BUTTON),
      /*paint_background=*/false);
  learn_more->SetEnabledTextColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  learn_more->SetID(PhoneHubViewID::kDisconnectedLearnMoreButton);
  content_view_->AddButton(std::move(learn_more));

  auto refresh = std::make_unique<InterstitialViewButton>(
      base::BindRepeating(
          &ConnectionErrorView::ButtonPressed, base::Unretained(this),
          InterstitialScreenEvent::kConfirm,
          base::BindRepeating(
              &chromeos::phonehub::ConnectionScheduler::ScheduleConnectionNow,
              base::Unretained(connection_scheduler_))),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_CONNECTION_ERROR_DIALOG_REFRESH_BUTTON),
      /*paint_background=*/true);
  refresh->SetID(PhoneHubViewID::kDisconnectedRefreshButton);
  content_view_->AddButton(std::move(refresh));

  phone_hub_metrics::LogInterstitialScreenEvent(
      Screen::kConnectionError, InterstitialScreenEvent::kShown);
}

ConnectionErrorView::~ConnectionErrorView() = default;

phone_hub_metrics::Screen ConnectionErrorView::GetScreenForMetrics() const {
  return GetID() == PhoneHubViewID::kReconnectingView
             ? Screen::kReconnecting
             : Screen::kConnectionError;
}

void ConnectionErrorView::ButtonPressed(InterstitialScreenEvent event,
                                        base::RepeatingClosure callback) {
  LogInterstitialScreenEvent(event);
  std::move(callback).Run();
}

BEGIN_METADATA(ConnectionErrorView, views::View)
END_METADATA

}  // namespace ash
