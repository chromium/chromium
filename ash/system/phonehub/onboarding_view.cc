// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/phonehub/onboarding_ui_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::Screen;

// OnboardingMainView ---------------------------------------------------------
// Main onboarding screen with Phone Hub feature description and two buttons
// (Get Started and Dismiss), where user can either choose to grant permission
// to enable this feature or dismiss the screen.
class OnboardingMainView : public PhoneHubInterstitialView {
  METADATA_HEADER(OnboardingMainView, PhoneHubInterstitialView)

 public:
  OnboardingMainView(phonehub::OnboardingUiTracker* onboarding_ui_tracker,
                     OnboardingView* parent_view,
                     OnboardingView::OnboardingFlow onboarding_flow)
      : PhoneHubInterstitialView(/*show_progress=*/false),
        onboarding_ui_tracker_(onboarding_ui_tracker),
        parent_view_(parent_view),
        onboarding_flow_(onboarding_flow) {
    SetID(PhoneHubViewID::kOnboardingMainView);
    InitLayout();
  }

  // PhoneHubInterstitialView:
  Screen GetScreenForMetrics() const override {
    switch (onboarding_flow_) {
      case OnboardingView::kExistingMultideviceUser:
        return Screen::kOnboardingExistingMultideviceUser;
      case OnboardingView::kNewMultideviceUser:
        return Screen::kOnboardingNewMultideviceUser;
    }
  }

 private:
  void InitLayout() {
    SetImage(ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
        IDR_PHONE_HUB_ONBOARDING_IMAGE));
    SetTitle(
        l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_TITLE));
    SetDescription(l10n_util::GetStringUTF16(
        IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_DESCRIPTION));

    auto get_started = std::make_unique<PillButton>(
        base::BindRepeating(&OnboardingMainView::GetStartedButtonPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(
            IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_GET_STARTED_BUTTON),
        PillButton::Type::kPrimaryWithoutIcon,
        /*icon=*/nullptr);
    get_started->SetID(PhoneHubViewID::kOnboardingGetStartedButton);
    AddButton(std::move(get_started));
  }

  void GetStartedButtonPressed() {
    LogInterstitialScreenEvent(InterstitialScreenEvent::kConfirm);
    onboarding_ui_tracker_->HandleGetStarted(
        parent_view_->IsOnboardingViewStartedFromNudge());
  }

  raw_ptr<phonehub::OnboardingUiTracker> onboarding_ui_tracker_ = nullptr;
  raw_ptr<OnboardingView> parent_view_ = nullptr;
  const OnboardingView::OnboardingFlow onboarding_flow_;
};

BEGIN_METADATA(OnboardingMainView)
END_METADATA

// OnboardingView -------------------------------------------------------------
OnboardingView::OnboardingView(
    phonehub::OnboardingUiTracker* onboarding_ui_tracker,
    Delegate* delegate,
    OnboardingFlow onboarding_flow)
    : onboarding_ui_tracker_(onboarding_ui_tracker), delegate_(delegate) {
  SetID(PhoneHubViewID::kOnboardingView);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  main_view_ = AddChildView(std::make_unique<OnboardingMainView>(
      onboarding_ui_tracker_, this, onboarding_flow));

  LogInterstitialScreenEvent(InterstitialScreenEvent::kShown);
}

OnboardingView::~OnboardingView() = default;

void OnboardingView::OnBubbleClose() {
  main_view_->OnBubbleClose();
}

Screen OnboardingView::GetScreenForMetrics() const {
  return main_view_->GetScreenForMetrics();
}

bool OnboardingView::IsOnboardingViewStartedFromNudge() {
  if (!delegate_) {
    return false;
  }

  return delegate_->IsPhoneHubIconClickedWhenNudgeVisible();
}

BEGIN_METADATA(OnboardingView)
END_METADATA

}  // namespace ash
