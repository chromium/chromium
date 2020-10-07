// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_view.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/phonehub/interstitial_view_button.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/unified/rounded_label_button.h"
#include "base/strings/string16.h"
#include "chromeos/components/phonehub/onboarding_ui_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

OnboardingView::OnboardingView(
    chromeos::phonehub::OnboardingUiTracker* onboarding_ui_tracker)
    : onboarding_ui_tracker_(onboarding_ui_tracker) {
  SetID(PhoneHubViewID::kOnboardingView);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view_ = AddChildView(
      std::make_unique<PhoneHubInterstitialView>(/*show_progress=*/false));

  // TODO(crbug.com/1127996): Replace PNG file with vector icon.
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PHONE_HUB_ONBOARDING_IMAGE);
  content_view_->SetImage(*image);
  content_view_->SetTitle(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_TITLE));
  content_view_->SetDescription(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_DESCRIPTION));

  // Add "Dismiss" and "Get started" buttons.
  auto dismiss = std::make_unique<InterstitialViewButton>(
      this,
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_DISMISS_BUTTON),
      /*paint_background=*/false);
  dismiss->SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  dismiss->SetID(PhoneHubViewID::kOnboardingDismissButton);
  content_view_->AddButton(std::move(dismiss));

  auto get_started = std::make_unique<InterstitialViewButton>(
      this,
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_GET_STARTED_BUTTON),
      /*paint_background=*/true);
  get_started->SetID(PhoneHubViewID::kOnboardingGetStartedButton);
  content_view_->AddButton(std::move(get_started));
}

OnboardingView::~OnboardingView() = default;

void OnboardingView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  switch (sender->GetID()) {
    case PhoneHubViewID::kOnboardingGetStartedButton:
      onboarding_ui_tracker_->HandleGetStarted();
      return;
    case PhoneHubViewID::kOnboardingDismissButton:
      onboarding_ui_tracker_->DismissSetupUi();
      return;
  }
}

BEGIN_METADATA(OnboardingView, views::View)
END_METADATA

}  // namespace ash
