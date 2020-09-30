// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace chromeos {
namespace phonehub {
class PhoneHubManager;
}  // namespace phonehub
}  // namespace chromeos

namespace ash {

class TrayBubbleView;

// A view of the Phone Hub panel, displaying phone status and utility actions
// such as phone status, task continuation, etc.
class PhoneConnectedView : public views::View {
 public:
  PhoneConnectedView(TrayBubbleView* bubble_view,
                     chromeos::phonehub::PhoneHubManager* phone_hub_manager);
  ~PhoneConnectedView() override;

  // views::View:
  void ChildPreferredSizeChanged(View* child) override;
  void ChildVisibilityChanged(View* child) override;
  const char* GetClassName() const override;

 private:
  void AddSeparator();
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_
