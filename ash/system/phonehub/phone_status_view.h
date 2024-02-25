// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/tri_view.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/phone_model.h"

namespace views {
class ImageView;
class Label;
class Separator;
}  // namespace views

namespace ash {

class IconButton;

// The header row at the top of the Phone Hub panel, showing phone title and
// status (wifi, volime, etc.).
class ASH_EXPORT PhoneStatusView : public TriView,
                                   public phonehub::PhoneModel::Observer {
 public:
  class Delegate {
   public:
    virtual bool CanOpenConnectedDeviceSettings() = 0;
    virtual void OpenConnectedDevicesSettings() = 0;
  };

  PhoneStatusView(phonehub::PhoneModel* phone_model, Delegate* delegate);
  ~PhoneStatusView() override;
  PhoneStatusView(PhoneStatusView&) = delete;
  PhoneStatusView operator=(PhoneStatusView&) = delete;

  // TriView:
  void OnThemeChanged() override;

  // phonehub::PhoneHubModel::Observer:
  void OnModelChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneStatusViewTest, MobileProviderVisibility);
  FRIEND_TEST_ALL_PREFIXES(PhoneStatusViewTest, PhoneStatusLabelsContent);
  FRIEND_TEST_ALL_PREFIXES(PhoneStatusViewTest, ClickOnSettings);

  // Update the labels and icons in the view to display current phone status.
  void Update();

  void UpdateMobileStatus();
  void UpdateBatteryStatus();
  PowerStatus::BatteryImageInfo CalculateBatteryInfo(
      const SkColor icon_fg_color);
  void SetBatteryTooltipText();

  // Clear the existing labels and icons for the phone status.
  void ClearExistingStatus();

  void ConfigureTriViewContainer(TriView::Container container);

  raw_ptr<phonehub::PhoneModel> phone_model_ = nullptr;
  raw_ptr<Delegate> delegate_ = nullptr;

  // Owned by views hierarchy.
  raw_ptr<views::Label> phone_name_label_ = nullptr;
  raw_ptr<views::ImageView> signal_icon_ = nullptr;
  raw_ptr<views::ImageView> battery_icon_ = nullptr;
  raw_ptr<views::Label> battery_label_ = nullptr;
  raw_ptr<views::Separator> separator_ = nullptr;
  raw_ptr<IconButton> settings_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_STATUS_VIEW_H_
