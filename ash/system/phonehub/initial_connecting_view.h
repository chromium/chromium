// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_INITIAL_CONNECTING_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_INITIAL_CONNECTING_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PhoneHubInterstitialView;

// An interstitial view representing this device is trying to connect to your
// phone after the user has opted in the Phone Hub feature through the
// onboarding UI.
class ASH_EXPORT InitialConnectingView : public PhoneHubContentView {
 public:
  METADATA_HEADER(InitialConnectingView);

  InitialConnectingView();
  InitialConnectingView(const InitialConnectingView&) = delete;
  InitialConnectingView& operator=(const InitialConnectingView&) = delete;
  ~InitialConnectingView() override;

  // PhoneHubContentView:
  phone_hub_metrics::Screen GetScreenForMetrics() const override;

 private:
  // Responsible for displaying the connecting UI contents.
  // Owned by view hierarchy.
  PhoneHubInterstitialView* content_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_INITIAL_CONNECTING_VIEW_H_
