// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_CONTENT_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_CONTENT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// A base class for Phone Hub content views.
class ASH_EXPORT PhoneHubContentView : public views::View {
  METADATA_HEADER(PhoneHubContentView, views::View)

 public:
  ~PhoneHubContentView() override;

  // Called upon bubble closing, subclasses can install their own handlers here
  // if needed for when the the bubble is dismissed.
  virtual void OnBubbleClose();

  // Returns the screen to be logged for metrics.
  virtual phone_hub_metrics::Screen GetScreenForMetrics() const;

 protected:
  PhoneHubContentView();

  void LogInterstitialScreenEvent(
      phone_hub_metrics::InterstitialScreenEvent event);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_CONTENT_VIEW_H_
