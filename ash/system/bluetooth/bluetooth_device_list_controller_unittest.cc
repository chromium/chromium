// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_controller_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/bluetooth/fake_bluetooth_detailed_view.h"
#include "ash/system/tray/tri_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const char kDeviceId1[] = "/device/id/1";
const char kDeviceId2[] = "/device/id/2";
const char kDeviceNickname[] = "mau5";

}  // namespace

class BluetoothDeviceListControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(features::kQsRevamp,
                                              IsQsRevampEnabled());
    AshTestBase::SetUp();

    fake_bluetooth_detailed_view_ =
        std::make_unique<FakeBluetoothDetailedView>(/*delegate=*/nullptr);
    bluetooth_device_list_controller_impl_ =
        std::make_unique<BluetoothDeviceListControllerImpl>(
            fake_bluetooth_detailed_view_.get());
  }

  void TearDown() override { AshTestBase::TearDown(); }

  const TriView* FindConnectedSubHeader() {
    return FindSubHeaderWithText(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_CURRENTLY_CONNECTED_DEVICES));
  }

  const TriView* FindPreviouslyConnectedSubHeader() {
    return FindSubHeaderWithText(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_PREVIOUSLY_CONNECTED_DEVICES));
  }

  const TriView* FindNoDeviceConnectedSubHeader() {
    return FindSubHeaderWithText(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_NO_DEVICE_CONNECTED));
  }

  const views::Separator* FindSeparator() {
    for (const auto* view : device_list()->children()) {
      if (!std::strcmp("Separator", view->GetClassName()))
        return static_cast<const views::Separator*>(view);
    }
    return nullptr;
  }

  PairedBluetoothDevicePropertiesPtr BuildDeviceProperties(
      const std::string& id) {
    PairedBluetoothDevicePropertiesPtr device_properties =
        PairedBluetoothDeviceProperties::New();
    device_properties->device_properties = BluetoothDeviceProperties::New();
    device_properties->device_properties->id = id;
    return device_properties;
  }

  const std::u16string& GetSubHeaderText(const TriView* sub_header) {
    EXPECT_TRUE(sub_header);
    EXPECT_EQ(1u, sub_header->children().at(1)->children().size());
    return static_cast<views::Label*>(
               sub_header->children().at(1)->children().at(0))
        ->GetText();
  }

  const char* GetDeviceId(const BluetoothDeviceListItemView* device_item_view) {
    return device_item_view->device_properties()->device_properties->id.c_str();
  }

  const BluetoothDeviceListItemView* GetFirstDeviceView() {
    EXPECT_LT(1u, device_list()->children().size());
    return static_cast<BluetoothDeviceListItemView*>(
        device_list()->children().at(1));
  }

  void CheckDeviceListOrdering(size_t connected_device_count,
                               size_t previously_connected_device_count) {
    if (connected_device_count && previously_connected_device_count) {
      const TriView* connected_sub_header = FindConnectedSubHeader();
      const TriView* previously_connected_sub_header =
          FindPreviouslyConnectedSubHeader();

      EXPECT_TRUE(connected_sub_header);
      EXPECT_TRUE(previously_connected_sub_header);

      const size_t connected_index =
          device_list()->GetIndexOf(connected_sub_header).value();
      EXPECT_EQ(0u, connected_index);

      // QsRevamp does not use a separator.
      if (!features::IsQsRevampEnabled()) {
        const views::Separator* device_list_separator = FindSeparator();
        EXPECT_TRUE(device_list_separator);

        const size_t previously_connected_index =
            device_list()->GetIndexOf(previously_connected_sub_header).value();
        const size_t separator_index =
            device_list()->GetIndexOf(device_list_separator).value();
        EXPECT_EQ(connected_device_count + 1, separator_index);
        EXPECT_EQ(separator_index + 1, previously_connected_index);
      }

      return;
    }

    if (connected_device_count) {
      const TriView* connected_sub_header = FindConnectedSubHeader();
      EXPECT_TRUE(connected_sub_header);
      EXPECT_EQ(0u, device_list()->GetIndexOf(connected_sub_header));
      EXPECT_EQ(connected_device_count + 1, device_list()->children().size());
      return;
    }

    if (previously_connected_device_count) {
      const TriView* previously_connected_sub_header =
          FindPreviouslyConnectedSubHeader();
      EXPECT_TRUE(previously_connected_sub_header);
      EXPECT_EQ(0u, device_list()->GetIndexOf(previously_connected_sub_header));
      EXPECT_EQ(previously_connected_device_count + 1,
                device_list()->children().size());
      return;
    }

    const TriView* no_device_connected_sub_header =
        FindNoDeviceConnectedSubHeader();
    EXPECT_TRUE(no_device_connected_sub_header);
    EXPECT_EQ(0u, device_list()->GetIndexOf(no_device_connected_sub_header));
    EXPECT_EQ(1u, device_list()->children().size());
  }

  void CheckNotifyDeviceListChangedCount(size_t call_count) {
    EXPECT_EQ(call_count, fake_bluetooth_detailed_view()
                              ->notify_device_list_changed_call_count());
  }

  // TODO(b/305075031) clean up after the flag is removed.
  bool IsQsRevampEnabled() { return true; }

  views::View* device_list() {
    return static_cast<BluetoothDetailedView*>(
               fake_bluetooth_detailed_view_.get())
        ->device_list();
  }

  BluetoothDeviceListController* bluetooth_device_list_controller() {
    return bluetooth_device_list_controller_impl_.get();
  }

  FakeBluetoothDetailedView* fake_bluetooth_detailed_view() {
    return fake_bluetooth_detailed_view_.get();
  }

 protected:
  const std::vector<PairedBluetoothDevicePropertiesPtr> empty_list_;

 private:
  const TriView* FindSubHeaderWithText(const std::u16string text) {
    for (const auto* view : device_list()->children()) {
      if (std::strcmp("TriView", view->GetClassName()))
        continue;
      const TriView* sub_header = static_cast<const TriView*>(view);
      if (GetSubHeaderText(sub_header) == text)
        return sub_header;
    }
    return nullptr;
  }

  std::unique_ptr<FakeBluetoothDetailedView> fake_bluetooth_detailed_view_;
  std::unique_ptr<BluetoothDeviceListControllerImpl>
      bluetooth_device_list_controller_impl_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         BluetoothDeviceListControllerTest,
                         testing::Bool());

TEST_P(BluetoothDeviceListControllerTest,
       HasCorrectSubHeaderWithNoPairedDevices) {
  CheckNotifyDeviceListChangedCount(/*call_count=*/0u);

  bluetooth_device_list_controller()->UpdateBluetoothEnabledState(true);
  bluetooth_device_list_controller()->UpdateDeviceList(
      /*connected=*/empty_list_,
      /*previously_connected=*/empty_list_);
  CheckNotifyDeviceListChangedCount(/*call_count=*/1u);

  EXPECT_EQ(1u, device_list()->children().size());

  const TriView* no_device_connected_sub_header =
      FindNoDeviceConnectedSubHeader();
  EXPECT_TRUE(no_device_connected_sub_header);
}

TEST_P(BluetoothDeviceListControllerTest,
       HasCorrectDeviceListOrderWithPairedAndPreviouslyPairedDevices) {
  CheckNotifyDeviceListChangedCount(/*call_count=*/0u);

  bluetooth_device_list_controller()->UpdateBluetoothEnabledState(true);

  std::vector<PairedBluetoothDevicePropertiesPtr> connected_list;
  connected_list.push_back(BuildDeviceProperties(kDeviceId1));

  bluetooth_device_list_controller()->UpdateDeviceList(
      /*connected=*/connected_list,
      /*previously_connected=*/empty_list_);
  CheckNotifyDeviceListChangedCount(/*call_count=*/1u);

  const TriView* connected_devices_sub_header = FindConnectedSubHeader();

  EXPECT_EQ(2u, device_list()->children().size());
  EXPECT_STREQ(kDeviceId1, GetDeviceId(GetFirstDeviceView()));
  EXPECT_TRUE(connected_devices_sub_header);

  CheckDeviceListOrdering(
      /*connected_device_count=*/connected_list.size(),
      /*previously_connected_device_count=*/empty_list_.size());

  std::vector<PairedBluetoothDevicePropertiesPtr> previously_connected_list;
  previously_connected_list.push_back(BuildDeviceProperties(kDeviceId2));

  bluetooth_device_list_controller()->UpdateDeviceList(
      /*connected=*/empty_list_,
      /*previously_connected=*/previously_connected_list);
  CheckNotifyDeviceListChangedCount(/*call_count=*/2u);

  const TriView* previously_connected_devices_sub_header =
      FindPreviouslyConnectedSubHeader();

  EXPECT_EQ(2u, device_list()->children().size());
  EXPECT_STREQ(kDeviceId2, GetDeviceId(GetFirstDeviceView()));
  EXPECT_TRUE(previously_connected_devices_sub_header);

  CheckDeviceListOrdering(
      /*connected_device_count=*/0,
      /*previously_connected_device_count=*/previously_connected_list.size());

  // "Update" the device list multiple times to be sure that no children are
  // duplicated and every child is re-ordered correctly.
  for (int i = 0; i < 2; i++) {
    bluetooth_device_list_controller()->UpdateDeviceList(
        /*connected=*/connected_list,
        /*previously_connected=*/previously_connected_list);
  }

  CheckNotifyDeviceListChangedCount(/*call_count=*/4u);

  // This is confusing but `device_list()` is actually the scroll contents of
  // the bluetooth detailed view, rather than a list of devices. So the count
  // here is a combination of the following views (all are optional):
  // *  Connected device header
  // *  Connected device list
  // *  Separator
  // *  Previously connected device header
  // *  Previously connected device list
  const int connected_header_count = FindConnectedSubHeader() ? 1 : 0;
  const int previously_connected_header_count =
      FindPreviouslyConnectedSubHeader() ? 1 : 0;
  const int separator_count = FindSeparator() ? 1 : 0;
  const size_t device_count =
      connected_list.size() + previously_connected_list.size();
  const auto expected_device_list_size = connected_header_count +
                                         previously_connected_header_count +
                                         separator_count + device_count;
  EXPECT_EQ(expected_device_list_size, device_list()->children().size());

  CheckDeviceListOrdering(
      /*connected_device_count=*/connected_list.size(),
      /*previously_connected_device_count=*/previously_connected_list.size());
}

TEST_P(BluetoothDeviceListControllerTest, ExistingDeviceViewsAreUpdated) {
  CheckNotifyDeviceListChangedCount(/*call_count=*/0u);

  bluetooth_device_list_controller()->UpdateBluetoothEnabledState(true);

  std::vector<PairedBluetoothDevicePropertiesPtr> connected_list;
  connected_list.push_back(BuildDeviceProperties(kDeviceId1));

  bluetooth_device_list_controller()->UpdateDeviceList(
      /*connected=*/connected_list,
      /*previously_connected=*/empty_list_);
  CheckNotifyDeviceListChangedCount(/*call_count=*/1u);

  EXPECT_EQ(2u, device_list()->children().size());

  const BluetoothDeviceListItemView* first_item = GetFirstDeviceView();

  EXPECT_FALSE(first_item->device_properties()->nickname.has_value());

  connected_list.at(0)->nickname = kDeviceNickname;

  bluetooth_device_list_controller()->UpdateDeviceList(
      /*connected=*/connected_list,
      /*previously_connected=*/empty_list_);
  CheckNotifyDeviceListChangedCount(/*call_count=*/2u);

  EXPECT_EQ(2u, device_list()->children().size());
  EXPECT_EQ(1u, device_list()->GetIndexOf(first_item));
  EXPECT_TRUE(first_item->device_properties()->nickname.has_value());
  EXPECT_STREQ(kDeviceNickname,
               first_item->device_properties()->nickname.value().c_str());
}

TEST_P(BluetoothDeviceListControllerTest,
       DeviceListIsClearedWhenBluetoothBecomesDisabled) {
  CheckNotifyDeviceListChangedCount(/*call_count=*/0u);

  bluetooth_device_list_controller()->UpdateBluetoothEnabledState(true);

  std::vector<PairedBluetoothDevicePropertiesPtr> connected_list;
  connected_list.push_back(BuildDeviceProperties(kDeviceId1));

  bluetooth_device_list_controller()->UpdateDeviceList(
      /*connected=*/connected_list,
      /*previously_connected=*/empty_list_);
  CheckNotifyDeviceListChangedCount(/*call_count=*/1u);

  EXPECT_EQ(2u, device_list()->children().size());

  bluetooth_device_list_controller()->UpdateBluetoothEnabledState(false);

  EXPECT_EQ(0u, device_list()->children().size());
}

}  // namespace ash
