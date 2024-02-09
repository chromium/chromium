// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_ONBOARDING_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_ONBOARDING_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class PhoneHubInterstitialView;

namespace phonehub {
class OnboardingUiTracker;
}

// An additional entry point UI to ask the existing multidevice users to opt in
// and set up the Phone feature on this device. Note that this class handles
// both the main onboarding screen and the dismiss prompt together.
class ASH_EXPORT OnboardingView : public PhoneHubContentView {
  METADATA_HEADER(OnboardingView, PhoneHubContentView)

 public:
  class Delegate {
   public:
    virtual void HideStatusHeaderView() = 0;
    virtual bool IsPhoneHubIconClickedWhenNudgeVisible() = 0;
  };

  // The different onboarding flows that are supported.
  enum OnboardingFlow { kExistingMultideviceUser = 0, kNewMultideviceUser };

  OnboardingView(phonehub::OnboardingUiTracker* onboarding_ui_tracker,
                 Delegate* delegate,
                 OnboardingFlow onboarding_flow);
  OnboardingView(const OnboardingView&) = delete;
  OnboardingView& operator=(const OnboardingView&) = delete;
  ~OnboardingView() override;

  // Update |content_view_| to display the dismiss prompt contents.
  // Invoked when user clicks the "Dismiss" button on the main onboarding view.
  void ShowDismissPrompt();

  // Determine if OnboardingView is created when nudge is visible.
  bool IsOnboardingViewStartedFromNudge();

  // PhoneHubContentView:
  void OnBubbleClose() override;
  phone_hub_metrics::Screen GetScreenForMetrics() const override;

 private:
  // The view responsible for displaying the onboarding UI contents.
  // Owned by view hierarchy.
  raw_ptr<PhoneHubInterstitialView, DanglingUntriaged> main_view_ = nullptr;

  raw_ptr<phonehub::OnboardingUiTracker> onboarding_ui_tracker_ = nullptr;
  raw_ptr<Delegate, DanglingUntriaged> delegate_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_ONBOARDING_VIEW_H_
