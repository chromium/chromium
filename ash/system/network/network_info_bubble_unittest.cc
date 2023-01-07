// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_info_bubble.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using chromeos::network_config::CrosNetworkConfigTestHelper;

const char kIPv4ConfigPath[] = "/ipconfig/stub_ipv4_config";
const char kIPv6ConfigPath[] = "/ipconfig/stub_ipv6_config";

const char kWiFiServiceGuid[] = "wifi_guid";
const char kWiFiServiceName[] = "stub_wifi_service";
const char kWiFiServicePath[] = "/service/stub_wifi_service";

const char kStubEthernetDeviceName[] = "stub_ethernet_device";
const char kStubEthernetDevicePath[] = "/device/stub_ethernet_device";
const char kStubWiFiDeviceName[] = "stub_wifi_device";
const char kStubWiFiDevicePath[] = "/device/stub_wifi_device";
const char kStubCellularDeviceName[] = "stub_cellular_device";
const char kStubCellularDevicePath[] = "/device/stub_cellular_device";

const char* kEthernetMacAddress = "33:33:33:33:33:33";
const char* kWiFiMacAddress = "44:44:44:44:44:44";
const char* kCellularMacAddress = "55:55:55:55:55:55";
const char* kIpv4Address = "1.1.1.1";
const char* kIpv6Address = "2222:2222:2222:2222:2222:2222:2222:2222";

class FakeNetworkInfoBubbleDelegate : public NetworkInfoBubble::Delegate {
 public:
  FakeNetworkInfoBubbleDelegate() = default;
  ~FakeNetworkInfoBubbleDelegate() override = default;

  void SetShouldIncludeDeviceAddresses(bool should_include_device_addresses) {
    should_include_device_addresses_ = should_include_device_addresses;
  }

  bool should_include_device_addresses() const {
    return should_include_device_addresses_;
  }

  size_t on_info_bubble_destroyed_call_count() const {
    return on_info_bubble_destroyed_call_count_;
  }

 private:
  // NetworkInfoBubble::Delegate:
  bool ShouldIncludeDeviceAddresses() override {
    return should_include_device_addresses_;
  }

  void OnInfoBubbleDestroyed() override {
    ++on_info_bubble_destroyed_call_count_;
  }

  bool should_include_device_addresses_ = true;
  size_t on_info_bubble_destroyed_call_count_ = 0;
};

}  // namespace

class NetworkInfoBubbleTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);

    network_state_helper()->ClearDevices();

    network_state_helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                                          /*enabled=*/true);
    network_state_helper()->AddDevice(
        kStubCellularDevicePath, shill::kTypeCellular, kStubCellularDeviceName);
    network_state_helper()->AddDevice(
        kStubEthernetDevicePath, shill::kTypeEthernet, kStubEthernetDeviceName);
    network_state_helper()->AddDevice(kStubWiFiDevicePath, shill::kTypeWifi,
                                      kStubWiFiDeviceName);

    widget_ = CreateFramelessTestWidget();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  void OpenBubble() {
    ASSERT_FALSE(network_info_bubble_);

    std::unique_ptr<NetworkInfoBubble> network_info_bubble =
        std::make_unique<NetworkInfoBubble>(weak_factory_.GetWeakPtr(),
                                            widget_->GetRootView());
    network_info_bubble_ = network_info_bubble.get();

    views::BubbleDialogDelegateView::CreateBubble(network_info_bubble.release())
        ->Show();
  }

  void CloseBubble() {
    ASSERT_TRUE(network_info_bubble_);
    network_info_bubble_->GetWidget()->Close();
    network_info_bubble_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void AddDefaultNetworkWithIPAddresses() {
    base::DictionaryValue ipv4;
    ipv4.SetKey(shill::kAddressProperty, base::Value(kIpv4Address));
    ipv4.SetKey(shill::kMethodProperty, base::Value(shill::kTypeIPv4));

    base::DictionaryValue ipv6;
    ipv6.SetKey(shill::kAddressProperty, base::Value(kIpv6Address));
    ipv6.SetKey(shill::kMethodProperty, base::Value(shill::kTypeIPv6));

    network_config_helper_.network_state_helper().ip_config_test()->AddIPConfig(
        kIPv4ConfigPath, ipv4);
    network_config_helper_.network_state_helper().ip_config_test()->AddIPConfig(
        kIPv6ConfigPath, ipv6);

    base::ListValue ip_configs;
    ip_configs.Append(kIPv4ConfigPath);
    ip_configs.Append(kIPv6ConfigPath);

    network_config_helper_.network_state_helper()
        .device_test()
        ->SetDeviceProperty(kStubWiFiDevicePath, shill::kIPConfigsProperty,
                            ip_configs, /*notify_changed=*/true);

    network_config_helper_.network_state_helper().service_test()->AddService(
        kWiFiServicePath, kWiFiServiceGuid, kWiFiServiceName, shill::kTypeWifi,
        shill::kStateOnline, true);

    base::RunLoop().RunUntilIdle();
  }

  void SetDeviceMacAddress(const std::string& device_path,
                           const std::string& mac_address) {
    network_config_helper_.network_state_helper()
        .device_test()
        ->SetDeviceProperty(device_path, shill::kAddressProperty,
                            base::Value(mac_address), true);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateMouseExit() {
    ASSERT_TRUE(network_info_bubble_);
    static_cast<views::View*>(network_info_bubble_)
        ->OnMouseExited(ui::MouseEvent(ui::ET_MOUSE_EXITED, gfx::PointF(),
                                       gfx::PointF(), base::TimeTicks(), 0, 0));
    network_info_bubble_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void InvalidateDelegate() { weak_factory_.InvalidateWeakPtrs(); }

  views::Label* FindLabelView() {
    return static_cast<views::Label*>(network_info_bubble_->GetViewByID(
        NetworkInfoBubble::kNetworkInfoBubbleLabelViewId));
  }

  NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  FakeNetworkInfoBubbleDelegate* fake_delegate() { return &fake_delegate_; }

 private:
  CrosNetworkConfigTestHelper network_config_helper_;
  FakeNetworkInfoBubbleDelegate fake_delegate_;
  NetworkInfoBubble* network_info_bubble_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
  std::string wifi_path_;
  std::unique_ptr<views::Widget> widget_;

  base::WeakPtrFactory<FakeNetworkInfoBubbleDelegate> weak_factory_{
      &fake_delegate_};
};

TEST_F(NetworkInfoBubbleTest, NotifiesDelegateOnDestruction) {
  OpenBubble();
  EXPECT_EQ(0u, fake_delegate()->on_info_bubble_destroyed_call_count());
  CloseBubble();
  EXPECT_EQ(1u, fake_delegate()->on_info_bubble_destroyed_call_count());
}

TEST_F(NetworkInfoBubbleTest, BubbleClosesOnMouseExit) {
  OpenBubble();
  EXPECT_EQ(0u, fake_delegate()->on_info_bubble_destroyed_call_count());
  SimulateMouseExit();
  EXPECT_EQ(1u, fake_delegate()->on_info_bubble_destroyed_call_count());
}

TEST_F(NetworkInfoBubbleTest, DoesNotNotifyDelegateIfDelegateInvalid) {
  OpenBubble();
  EXPECT_EQ(0u, fake_delegate()->on_info_bubble_destroyed_call_count());
  InvalidateDelegate();
  CloseBubble();
  EXPECT_EQ(0u, fake_delegate()->on_info_bubble_destroyed_call_count());
}

TEST_F(NetworkInfoBubbleTest, HasCorrectText) {
  OpenBubble();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_NETWORKS),
            FindLabelView()->GetText());
  CloseBubble();

  std::u16string expected_text =
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_ETHERNET_ADDRESS,
                                 base::UTF8ToUTF16(kEthernetMacAddress));
  SetDeviceMacAddress(kStubEthernetDevicePath, kEthernetMacAddress);

  OpenBubble();
  EXPECT_EQ(expected_text, FindLabelView()->GetText());
  CloseBubble();

  base::StrAppend(
      &expected_text,
      {u"\n", l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_WIFI_ADDRESS,
                                         base::UTF8ToUTF16(kWiFiMacAddress))});
  SetDeviceMacAddress(kStubWiFiDevicePath, kWiFiMacAddress);

  OpenBubble();
  EXPECT_EQ(expected_text, FindLabelView()->GetText());
  CloseBubble();

  base::StrAppend(&expected_text,
                  {u"\n", l10n_util::GetStringFUTF16(
                              IDS_ASH_STATUS_TRAY_CELLULAR_ADDRESS,
                              base::UTF8ToUTF16(kCellularMacAddress))});
  SetDeviceMacAddress(kStubCellularDevicePath, kCellularMacAddress);

  OpenBubble();
  EXPECT_EQ(expected_text, FindLabelView()->GetText());
  CloseBubble();

  fake_delegate()->SetShouldIncludeDeviceAddresses(false);

  OpenBubble();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_NETWORKS),
            FindLabelView()->GetText());
  CloseBubble();

  const std::u16string expected_text_ip_addresses = base::StrCat(
      {l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_IP_ADDRESS,
                                  base::UTF8ToUTF16(kIpv4Address)),
       u"\n",
       l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_IPV6_ADDRESS,
                                  base::UTF8ToUTF16(kIpv6Address))});

  AddDefaultNetworkWithIPAddresses();

  OpenBubble();
  EXPECT_EQ(expected_text_ip_addresses, FindLabelView()->GetText());
  CloseBubble();

  fake_delegate()->SetShouldIncludeDeviceAddresses(true);

  expected_text = expected_text_ip_addresses + u"\n" + expected_text;

  OpenBubble();
  EXPECT_EQ(expected_text, FindLabelView()->GetText());
  CloseBubble();
}

}  // namespace ash
