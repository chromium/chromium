// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_ONBOARDING_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_ONBOARDING_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PhoneHubInterstitialView;

// An additional entry point UI to ask the existing multidevice users to opt in
// and set up the Phone feature on this device.
class ASH_EXPORT OnboardingView : public views::View,
                                  public views::ButtonListener {
 public:
  METADATA_HEADER(OnboardingView);

  OnboardingView();
  OnboardingView(const OnboardingView&) = delete;
  OnboardingView& operator=(const OnboardingView&) = delete;
  ~OnboardingView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // The view responsible for displaying the onboarding UI contents.
  // Owned by view hierarchy.
  PhoneHubInterstitialView* content_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_ONBOARDING_VIEW_H_
