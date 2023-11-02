// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ui/views/view.h"

namespace ash {

namespace phonehub {
class PhoneHubManager;
}

// A view of the Phone Hub panel, displaying phone status and utility actions
// such as phone status, task continuation, etc.
class PhoneConnectedView : public PhoneHubContentView {
 public:
  explicit PhoneConnectedView(phonehub::PhoneHubManager* phone_hub_manager);
  ~PhoneConnectedView() override;

  // views::View:
  void ChildPreferredSizeChanged(View* child) override;
  void ChildVisibilityChanged(View* child) override;
  const char* GetClassName() const override;

  // PhoneHubContentView:
  phone_hub_metrics::Screen GetScreenForMetrics() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_
