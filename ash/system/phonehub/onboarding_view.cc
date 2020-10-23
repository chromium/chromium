// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_view.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/phonehub/interstitial_view_button.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/unified/rounded_label_button.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "chromeos/components/phonehub/onboarding_ui_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

using phone_hub_metrics::InterstitialScreen;
using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::LogInterstitialScreenEvent;

// OnboardingMainView ---------------------------------------------------------
// Main onboarding screen with Phone Hub feature description and two buttons
// (Get Started and Dismiss), where user can either choose to grant permission
// to enable this feature or dismiss the screen.
class OnboardingMainView : public PhoneHubInterstitialView,
                           public views::ButtonListener {
 public:
  OnboardingMainView(
      chromeos::phonehub::OnboardingUiTracker* onboarding_ui_tracker,
      OnboardingView* parent_view)
      : PhoneHubInterstitialView(/*show_progress=*/false),
        onboarding_ui_tracker_(onboarding_ui_tracker),
        parent_view_(parent_view) {
    SetID(PhoneHubViewID::kOnboardingMainView);
    InitLayout();
  }

  // views::ButtonListener:
  // TODO(crbug.com/1141629): deprecated, replace with |PressedCallback|.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    switch (sender->GetID()) {
      case PhoneHubViewID::kOnboardingGetStartedButton:
        // TODO(tengs): Distinguish between the two different onboarding flows.
        LogInterstitialScreenEvent(
            InterstitialScreen::kOnboardingNewMultideviceUser,
            InterstitialScreenEvent::kConfirm);
        onboarding_ui_tracker_->HandleGetStarted();
        return;
      case PhoneHubViewID::kOnboardingDismissButton:
        // TODO(tengs): Distinguish between the two different onboarding flows.
        LogInterstitialScreenEvent(
            InterstitialScreen::kOnboardingNewMultideviceUser,
            InterstitialScreenEvent::kDismiss);
        parent_view_->ShowDismissPrompt();
        return;
    }
  }

 private:
  void InitLayout() {
    // TODO(crbug.com/1127996): Replace PNG file with vector icon.
    gfx::ImageSkia* image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_PHONE_HUB_ONBOARDING_IMAGE);
    SetImage(*image);
    SetTitle(
        l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_TITLE));
    SetDescription(l10n_util::GetStringUTF16(
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
    AddButton(std::move(dismiss));

    auto get_started = std::make_unique<InterstitialViewButton>(
        this,
        l10n_util::GetStringUTF16(
            IDS_ASH_PHONE_HUB_ONBOARDING_DIALOG_GET_STARTED_BUTTON),
        /*paint_background=*/true);
    get_started->SetID(PhoneHubViewID::kOnboardingGetStartedButton);
    AddButton(std::move(get_started));
  }

  chromeos::phonehub::OnboardingUiTracker* onboarding_ui_tracker_ = nullptr;
  OnboardingView* parent_view_ = nullptr;
};

// OnboardingDismissPromptView ------------------------------------------------
// A follow-up prompt screen that pops up when the user has chosen to dismiss
// the main onboarding screen. It should not be shown again after being
// dismissed manually by either clicking the ack button or outside the bubble.
class OnboardingDismissPromptView : public PhoneHubInterstitialView,
                                    public views::ButtonListener {
 public:
  explicit OnboardingDismissPromptView(
      chromeos::phonehub::OnboardingUiTracker* onboarding_ui_tracker)
      : PhoneHubInterstitialView(/*show_progress=*/false),
        onboarding_ui_tracker_(onboarding_ui_tracker) {
    SetID(PhoneHubViewID::kOnboardingDismissPromptView);
    InitLayout();
  }

 private:
  void InitLayout() {
    // Adds title and description.
    SetTitle(l10n_util::GetStringUTF16(
        IDS_ASH_PHONE_HUB_ONBOARDING_DISMISS_DIALOG_TITLE));
    base::string16 part1 = l10n_util::GetStringUTF16(
        IDS_ASH_PHONE_HUB_ONBOARDING_DISMISS_DIALOG_DESCRIPTION_PART_1);
    base::string16 part2 = l10n_util::GetStringUTF16(
        IDS_ASH_PHONE_HUB_ONBOARDING_DISMISS_DIALOG_DESCRIPTION_PART_2);
    // Uses "\n" to create a newline separator between two text paragraphs.
    SetDescription(base::StrCat({part1, base::ASCIIToUTF16("\n\n"), part2}));

    // Adds "Ok, got it" button.
    auto ack_button = std::make_unique<InterstitialViewButton>(
        this,
        l10n_util::GetStringUTF16(
            IDS_ASH_PHONE_HUB_ONBOARDING_DISMISS_DIALOG_OK_BUTTON),
        /*paint_background=*/true);
    ack_button->SetID(PhoneHubViewID::kOnboardingDismissAckButton);
    AddButton(std::move(ack_button));
  }

  // PhoneHubContentView:
  void OnBubbleClose() override { onboarding_ui_tracker_->DismissSetupUi(); }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(sender->GetID(), PhoneHubViewID::kOnboardingDismissAckButton);
    Shell::GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->phone_hub_tray()
        ->CloseBubble();
  }

  chromeos::phonehub::OnboardingUiTracker* onboarding_ui_tracker_ = nullptr;
};

// OnboardingView -------------------------------------------------------------
OnboardingView::OnboardingView(
    chromeos::phonehub::OnboardingUiTracker* onboarding_ui_tracker,
    TrayBubbleView* bubble_view)
    : onboarding_ui_tracker_(onboarding_ui_tracker), bubble_view_(bubble_view) {
  SetID(PhoneHubViewID::kOnboardingView);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  main_view_ = AddChildView(
      std::make_unique<OnboardingMainView>(onboarding_ui_tracker_, this));

  // TODO(tengs): Distinguish between the two different onboarding flows.
  LogInterstitialScreenEvent(InterstitialScreen::kOnboardingNewMultideviceUser,
                             InterstitialScreenEvent::kShown);
}

OnboardingView::~OnboardingView() = default;

void OnboardingView::OnBubbleClose() {
  main_view_->OnBubbleClose();
}

void OnboardingView::ShowDismissPrompt() {
  DCHECK(main_view_);

  RemoveChildView(main_view_);
  main_view_ = AddChildView(
      std::make_unique<OnboardingDismissPromptView>(onboarding_ui_tracker_));

  // Updates bubble to handle size change with a different child view.
  bubble_view_->UpdateBubble();
}

BEGIN_METADATA(OnboardingView, views::View)
END_METADATA

}  // namespace ash
