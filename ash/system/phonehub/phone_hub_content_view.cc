// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_content_view.h"

namespace ash {

PhoneHubContentView::PhoneHubContentView() = default;
PhoneHubContentView::~PhoneHubContentView() = default;

void PhoneHubContentView::OnBubbleClose() {
  // Nothing to do.
}

phone_hub_metrics::Screen PhoneHubContentView::GetScreenForMetrics() const {
  return phone_hub_metrics::Screen::kInvalid;
}

void PhoneHubContentView::LogInterstitialScreenEvent(
    phone_hub_metrics::InterstitialScreenEvent event) {
  phone_hub_metrics::LogInterstitialScreenEvent(GetScreenForMetrics(), event);
}

}  // namespace ash
