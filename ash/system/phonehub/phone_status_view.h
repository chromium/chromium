// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "chromeos/components/phonehub/phone_model.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// The header row at the top of the Phone Hub panel, showing phone title and
// status (wifi, volime, etc.).
class ASH_EXPORT PhoneStatusView
    : public TriView,
      public views::ButtonListener,
      public chromeos::phonehub::PhoneModel::Observer {
 public:
  explicit PhoneStatusView(chromeos::phonehub::PhoneModel* phone_model);
  ~PhoneStatusView() override;
  PhoneStatusView(PhoneStatusView&) = delete;
  PhoneStatusView operator=(PhoneStatusView&) = delete;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // chromeos::phonehub::PhoneHubModel::Observer:
  void OnModelChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneStatusViewTest, MobileProviderVisibility);
  FRIEND_TEST_ALL_PREFIXES(PhoneStatusViewTest, PhoneStatusLabelsContent);

  // Update the labels and icons in the view to display current phone status.
  void Update();

  void UpdateMobileStatus();
  void UpdateBatteryStatus();
  PowerStatus::BatteryImageInfo CalculateBatteryInfo();

  // Clear the existing labels and icons for the phone status.
  void ClearExistingStatus();

  void ConfigureTriViewContainer(TriView::Container container);

  chromeos::phonehub::PhoneModel* phone_model_ = nullptr;

  // Owned by views hierarchy.
  views::Label* phone_name_label_ = nullptr;
  views::ImageView* signal_icon_ = nullptr;
  views::Label* mobile_provider_label_ = nullptr;
  views::ImageView* battery_icon_ = nullptr;
  views::Label* battery_label_ = nullptr;
  TopShortcutButton* settings_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_
