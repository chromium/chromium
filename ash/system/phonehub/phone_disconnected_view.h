// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_DISCONNECTED_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_DISCONNECTED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class PhoneHubInterstitialView;

namespace phone_hub_metrics {
enum class InterstitialScreenEvent;
}

namespace phonehub {
class ConnectionScheduler;
}

// An interstitial view represeting that connection to the phone has been
// interrupted.
class ASH_EXPORT PhoneDisconnectedView : public PhoneHubContentView {
  METADATA_HEADER(PhoneDisconnectedView, PhoneHubContentView)

 public:
  explicit PhoneDisconnectedView(
      phonehub::ConnectionScheduler* connection_scheduler);
  PhoneDisconnectedView(const PhoneDisconnectedView&) = delete;
  PhoneDisconnectedView& operator=(const PhoneDisconnectedView&) = delete;
  ~PhoneDisconnectedView() override;

  // PhoneHubContentView:
  phone_hub_metrics::Screen GetScreenForMetrics() const override;

 private:
  void ButtonPressed(phone_hub_metrics::InterstitialScreenEvent event,
                     base::RepeatingClosure callback);

  raw_ptr<phonehub::ConnectionScheduler> connection_scheduler_ = nullptr;

  raw_ptr<PhoneHubInterstitialView> content_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_DISCONNECTED_VIEW_H_
