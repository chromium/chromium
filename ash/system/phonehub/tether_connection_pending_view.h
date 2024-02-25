// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_TETHER_CONNECTION_PENDING_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_TETHER_CONNECTION_PENDING_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PhoneHubInterstitialView;

// A connecting to tether hotspot view shown when resuming an interrupted
// connection to PhoneHub as a result of an in progress tether connection.
class ASH_EXPORT TetherConnectionPendingView : public PhoneHubContentView {
  METADATA_HEADER(TetherConnectionPendingView, PhoneHubContentView)

 public:
  TetherConnectionPendingView();
  TetherConnectionPendingView(const TetherConnectionPendingView&) = delete;
  TetherConnectionPendingView& operator=(const TetherConnectionPendingView&) =
      delete;
  ~TetherConnectionPendingView() override;

  // PhoneHubContentView:
  phone_hub_metrics::Screen GetScreenForMetrics() const override;

 private:
  // Responsible for displaying the connecting UI contents.
  // Owned by view hierarchy.
  raw_ptr<PhoneHubInterstitialView> content_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_TETHER_CONNECTION_PENDING_VIEW_H_
