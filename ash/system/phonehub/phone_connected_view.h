// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/app_stream_connection_error_dialog.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace ash {

namespace phonehub {
class PhoneHubManager;
}

class QuickActionsView;

// A view of the Phone Hub panel, displaying phone status and utility actions
// such as phone status, task continuation, etc.
class PhoneConnectedView : public PhoneHubContentView {
  METADATA_HEADER(PhoneConnectedView, PhoneHubContentView)

 public:
  explicit PhoneConnectedView(phonehub::PhoneHubManager* phone_hub_manager);
  ~PhoneConnectedView() override;

  // views::View:
  void ChildPreferredSizeChanged(View* child) override;
  void ChildVisibilityChanged(View* child) override;

  // PhoneHubContentView:
  phone_hub_metrics::Screen GetScreenForMetrics() const override;

  void ShowAppStreamErrorDialog(bool is_different_network,
                                bool is_phone_on_cellular);

 private:
  void OnAppStreamErrorDialogClosed();

  void OnAppStreamErrorDialogButtonClicked(const ui::Event& event);

  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_;
  raw_ptr<QuickActionsView> quick_actions_view_;
  std::unique_ptr<AppStreamConnectionErrorDialog> app_stream_error_dialog_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_CONNECTED_VIEW_H_
