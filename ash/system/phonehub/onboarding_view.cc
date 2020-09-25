// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_view.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/unified/rounded_label_button.h"
#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Tag value used to uniquely identify the "Dismiss" and "Get started" buttons.
constexpr int kDismissButtonTag = 1;
constexpr int kGetStartedTag = 2;

}  // namespace

OnboardingView::OnboardingView() {
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
  auto dismiss = std::make_unique<views::LabelButton>(
      this, l10n_util::GetStringUTF16(
                IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_DISMISS_BUTTON));
  dismiss->SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  dismiss->set_tag(kDismissButtonTag);
  content_view_->AddButton(std::move(dismiss));

  auto get_started = std::make_unique<RoundedLabelButton>(
      this, l10n_util::GetStringUTF16(
                IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_GET_STARTED_BUTTON));
  get_started->set_tag(kGetStartedTag);
  content_view_->AddButton(std::move(get_started));
}

OnboardingView::~OnboardingView() = default;

void OnboardingView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  // TODO(meilinw): implement button pressed actions.
}

BEGIN_METADATA(OnboardingView, views::View)
END_METADATA

}  // namespace ash
