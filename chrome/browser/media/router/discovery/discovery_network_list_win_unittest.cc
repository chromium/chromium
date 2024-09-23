// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list_win.h"

#include <wrl/client.h>

#include <optional>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_hstring.h"
#include "chrome/browser/media/router/discovery/discovery_network_list.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_ip_helper.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_winrt_network_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WinrtConnectivity = ABI::Windows::Networking::Connectivity;
namespace WinrtCollections = ABI::Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;

namespace media_router {

namespace {

constexpr const char kWiredAdapterName[] = "fake_wired_adapter";
constexpr GUID kWiredNetworkAdapterId = {
    0xf8eb6b9e,
    0xa77e,
    0x44c8,
    {0xa2, 0xf0, 0x4, 0x44, 0x84, 0xc6, 0xb6, 0xcd}};
constexpr uint8_t kWiredMacAddress[] = {0x00, 0x21, 0x02, 0x43, 0x14, 0x05};

constexpr const char kWifiAdapterName[] = "fake_wifi_adapter";
constexpr GUID kWifiNetworkAdapterId = {
    0xf494958d,
    0x661a,
    0x45f1,
    {0x9e, 0x91, 0xb6, 0x58, 0x38, 0x77, 0x7f, 0xe1}};
constexpr uint8_t kWifiMacAddress[] = {0x08, 0xf2, 0x06, 0xc4, 0xa1, 0x07};
constexpr const char kWifiSsid[] = "fake_wifi_ssid";

}  // namespace

class DiscoveryNetworkListWinTest : public testing::Test {
 public:
  DiscoveryNetworkListWinTest() = default;
  ~DiscoveryNetworkListWinTest() override = default;

  void SetUp() override;
  void TearDown() override;

  // Add network adapters and connection profiles to the fake Windows OS API
  // implementations.  AddWiredConnectionProfile() can be called once to setup a
  // network adapter using the kWired* constants defined above.
  // AddWifiConnectionProfile() can be called once to setup a network adapter
  // using the kWifi* constants defined above.  If a test requires more
  // adapters, it can call AddConnectionProfile() with test defined values.
  ComPtr<WinrtConnectivity::IConnectionProfile> AddWiredConnectionProfile(
      WinrtConnectivity::NetworkConnectivityLevel connectivity);
  ComPtr<WinrtConnectivity::IConnectionProfile> AddWifiConnectionProfile(
      WinrtConnectivity::NetworkConnectivityLevel connectivity);
  ComPtr<WinrtConnectivity::IConnectionProfile> AddConnectionProfile(
      const std::string& adapter_name,
      const GUID& network_adapter_id,
      const std::vector<uint8_t>& mac_address,
      std::optional<std::string> ssid,
      WinrtConnectivity::NetworkConnectivityLevel connectivity);

  // Returns a random 6-byte MAC address.
  std::vector<uint8_t> GenerateRandomMacAddress() const;

  DiscoveryNetworkListWinTest(const DiscoveryNetworkListWinTest&) = delete;
  DiscoveryNetworkListWinTest& operator=(const DiscoveryNetworkListWinTest&) =
      delete;

 protected:
  const std::vector<uint8_t> kWiredMacAddressVector{
      kWiredMacAddress, kWiredMacAddress + std::size(kWiredMacAddress)};

  const std::vector<uint8_t> kWifiMacAddressVector{
      kWifiMacAddress, kWifiMacAddress + std::size(kWifiMacAddress)};

  // Contains the fake Windows OS API implementations along with the network
  // adapters and connection profiles setup through Add*ConnectionProfile().
  FakeIpHelper fake_ip_helper_;
  FakeWinrtNetworkEnvironment fake_winrt_network_environment_;
};

void DiscoveryNetworkListWinTest::SetUp() {
  // Use the fake Windows OS APIs to simulate different networking environments.
  WindowsOsApi os_api_override;

  // Override the WinRT networking connectivity APIs.
  os_api_override.winrt_api.ro_get_activation_factory_callback =
      base::BindRepeating(
          &FakeWinrtNetworkEnvironment::FakeRoGetActivationFactory,
          base::Unretained(&fake_winrt_network_environment_));

  // Override the IP Helper Win32 APIs.
  os_api_override.ip_helper_api.get_adapters_addresses_callback =
      base::BindRepeating(&FakeIpHelper::GetAdaptersAddresses,
                          base::Unretained(&fake_ip_helper_));
  os_api_override.ip_helper_api.get_if_table2_callback = base::BindRepeating(
      &FakeIpHelper::GetIfTable2, base::Unretained(&fake_ip_helper_));
  os_api_override.ip_helper_api.free_mib_table_callback = base::BindRepeating(
      &FakeIpHelper::FreeMibTable, base::Unretained(&fake_ip_helper_));

  OverrideWindowsOsApiForTesting(os_api_override);

  // Force GetDiscoveryNetworkInfoList() to use the WinRT code path that avoids
  // prompting for network location permission on Windows 24H2.
  OverrideRequiresNetworkLocationPermissionForTesting(true);
}

void DiscoveryNetworkListWinTest::TearDown() {
  // Remove the overrides to start using the actual OS APIs again.
  OverrideWindowsOsApiForTesting({});
  OverrideRequiresNetworkLocationPermissionForTesting(false);
}

ComPtr<WinrtConnectivity::IConnectionProfile>
DiscoveryNetworkListWinTest::AddWiredConnectionProfile(
    WinrtConnectivity::NetworkConnectivityLevel connectivity) {
  return AddConnectionProfile(kWiredAdapterName, kWiredNetworkAdapterId,
                              kWiredMacAddressVector,
                              /*ssid=*/std::nullopt, connectivity);
}

ComPtr<WinrtConnectivity::IConnectionProfile>
DiscoveryNetworkListWinTest::AddWifiConnectionProfile(
    WinrtConnectivity::NetworkConnectivityLevel connectivity) {
  return AddConnectionProfile(kWifiAdapterName, kWifiNetworkAdapterId,
                              kWifiMacAddressVector, kWifiSsid, connectivity);
}

ComPtr<WinrtConnectivity::IConnectionProfile>
DiscoveryNetworkListWinTest::AddConnectionProfile(
    const std::string& adapter_name,
    const GUID& network_adapter_id,
    const std::vector<uint8_t>& mac_address,
    std::optional<std::string> ssid,
    WinrtConnectivity::NetworkConnectivityLevel connectivity) {
  IFTYPE adapter_type;
  if (ssid) {
    adapter_type = IF_TYPE_IEEE80211;
  } else {
    adapter_type = IF_TYPE_ETHERNET_CSMACD;
  }
  fake_ip_helper_.AddNetworkInterface(adapter_name, network_adapter_id,
                                      mac_address, adapter_type,
                                      IfOperStatusUp);

  return fake_winrt_network_environment_.AddConnectionProfile(
      network_adapter_id, connectivity, ssid);
}

std::vector<uint8_t> DiscoveryNetworkListWinTest::GenerateRandomMacAddress()
    const {
  std::vector<uint8_t> mac_address(6);
  base::RandBytes(mac_address);
  return mac_address;
}

TEST_F(DiscoveryNetworkListWinTest, GetInterfaceGuidMacMapEmpty) {
  auto interface_guid_mac_map = GetInterfaceGuidMacMap();
  EXPECT_EQ(interface_guid_mac_map.size(), 0u);
}

TEST_F(DiscoveryNetworkListWinTest, GetInterfaceGuidMacMap) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                NetworkConnectivityLevel_InternetAccess);

  auto interface_guid_mac_map = GetInterfaceGuidMacMap();
  ASSERT_EQ(interface_guid_mac_map.size(), 2u);

  auto it = interface_guid_mac_map.find(kWiredNetworkAdapterId);
  ASSERT_NE(it, interface_guid_mac_map.end());

  const std::string kExpectedWiredMacAddress(kWiredMacAddressVector.begin(),
                                             kWiredMacAddressVector.end());
  EXPECT_EQ(it->second, kExpectedWiredMacAddress);

  it = interface_guid_mac_map.find(kWifiNetworkAdapterId);
  ASSERT_NE(it, interface_guid_mac_map.end());

  const std::string kExpectedWifiMacAddress(kWifiMacAddressVector.begin(),
                                            kWifiMacAddressVector.end());
  EXPECT_EQ(it->second, kExpectedWifiMacAddress);
}

TEST_F(DiscoveryNetworkListWinTest, GetInterfaceGuidMacMapError) {
  AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                NetworkConnectivityLevel_InternetAccess);

  fake_ip_helper_.SimulateError(FakeIpHelperStatus::kErrorGetIfTable2Failed);

  auto interface_guid_mac_map = GetInterfaceGuidMacMap();
  EXPECT_EQ(interface_guid_mac_map.size(), 0u);
}

TEST_F(DiscoveryNetworkListWinTest, IsProfileConnectedToNetworkWhenOnline) {
  ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile =
      AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                    NetworkConnectivityLevel_InternetAccess);

  EXPECT_TRUE(IsProfileConnectedToNetwork(connection_profile.Get()));
}

TEST_F(DiscoveryNetworkListWinTest, IsProfileConnectedToNetworkWhenOffline) {
  ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile =
      AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                    NetworkConnectivityLevel_None);

  EXPECT_FALSE(IsProfileConnectedToNetwork(connection_profile.Get()));
}

TEST_F(DiscoveryNetworkListWinTest, IsProfileConnectedNetworkError) {
  ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile =
      AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                    NetworkConnectivityLevel_InternetAccess);

  fake_winrt_network_environment_.SimulateError(
      FakeWinrtNetworkStatus::
          kErrorConnectionProfileGetNetworkConnectivityLevelFailed);

  // `IsProfileConnectedToNetwork()` must return false when an error occurs.
  EXPECT_FALSE(IsProfileConnectedToNetwork(connection_profile.Get()));
}

TEST_F(DiscoveryNetworkListWinTest, GetProfileNetworkAdapterId) {
  ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile =
      AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                    NetworkConnectivityLevel_InternetAccess);

  GUID network_adapter_id;
  HRESULT hr =
      GetProfileNetworkAdapterId(connection_profile.Get(), &network_adapter_id);

  EXPECT_EQ(hr, S_OK);
  EXPECT_EQ(network_adapter_id, kWiredNetworkAdapterId);
}

TEST_F(DiscoveryNetworkListWinTest, GetProfileNetworkAdapterIdErrors) {
  ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile =
      AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                    NetworkConnectivityLevel_InternetAccess);

  constexpr FakeWinrtNetworkStatus kErrorStatusList[] = {
      FakeWinrtNetworkStatus::kErrorConnectionProfileGetNetworkAdapterFailed,
      FakeWinrtNetworkStatus::kErrorGetNetworkAdapterIdFailed,
  };
  for (const FakeWinrtNetworkStatus error_status : kErrorStatusList) {
    fake_winrt_network_environment_.SimulateError(error_status);

    GUID network_adapter_id = {0};
    HRESULT hr = GetProfileNetworkAdapterId(connection_profile.Get(),
                                            &network_adapter_id);

    EXPECT_EQ(hr, fake_winrt_network_environment_.MakeHresult(error_status))
        << " for error_status: " << static_cast<int>(error_status);
    EXPECT_EQ(network_adapter_id, GUID_NULL)
        << " for error_status: " << static_cast<int>(error_status);
  }
}

TEST_F(DiscoveryNetworkListWinTest, GetProfileWifiSSID) {
  ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile =
      AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                   NetworkConnectivityLevel_InternetAccess);

  HSTRING ssid_hstring;
  HRESULT hr = GetProfileWifiSSID(connection_profile.Get(), &ssid_hstring);
  ASSERT_EQ(hr, S_OK);

  base::win::ScopedHString ssid(ssid_hstring);
  EXPECT_EQ(ssid.GetAsUTF8(), kWifiSsid);
}

TEST_F(DiscoveryNetworkListWinTest, GetProfileWifiSSIDWhenNotWifi) {
  ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile =
      AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                    NetworkConnectivityLevel_InternetAccess);

  HSTRING ssid_hstring = nullptr;
  HRESULT hr = GetProfileWifiSSID(connection_profile.Get(), &ssid_hstring);
  EXPECT_EQ(hr, kWifiNotSupported);
  EXPECT_EQ(ssid_hstring, nullptr);
}

TEST_F(DiscoveryNetworkListWinTest, GetProfileWifiSsidErrors) {
  ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile =
      AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                   NetworkConnectivityLevel_InternetAccess);

  constexpr FakeWinrtNetworkStatus kErrorStatusList[] = {
      FakeWinrtNetworkStatus::kErrorConnectionProfileQueryInterfaceFailed,
      FakeWinrtNetworkStatus::
          kErrorConnectionProfileGetWlanConnectionProfileDetailsFailed,
      FakeWinrtNetworkStatus::
          kErrorWlanConnectionProfileDetailsGetConnectedSsidFailed,
  };
  for (const FakeWinrtNetworkStatus error_status : kErrorStatusList) {
    fake_winrt_network_environment_.SimulateError(error_status);

    HSTRING ssid_hstring = nullptr;
    HRESULT hr = GetProfileWifiSSID(connection_profile.Get(), &ssid_hstring);

    EXPECT_EQ(hr, fake_winrt_network_environment_.MakeHresult(error_status))
        << " for error_status: " << static_cast<int>(error_status);
    EXPECT_EQ(ssid_hstring, nullptr)
        << " for error_status: " << static_cast<int>(error_status);
  }
}

TEST_F(DiscoveryNetworkListWinTest, GetAllConnectionProfilesEmpty) {
  ComPtr<WinrtCollections::IVectorView<WinrtConnectivity::ConnectionProfile*>>
      connection_profiles;
  uint32_t connection_profiles_size = 0u;

  HRESULT hr =
      GetAllConnectionProfiles(&connection_profiles, &connection_profiles_size);

  EXPECT_EQ(hr, S_OK);
  EXPECT_NE(connection_profiles, nullptr);
  EXPECT_EQ(connection_profiles_size, 0u);
}

TEST_F(DiscoveryNetworkListWinTest, GetAllConnectionProfiles) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                NetworkConnectivityLevel_LocalAccess);

  ComPtr<WinrtCollections::IVectorView<WinrtConnectivity::ConnectionProfile*>>
      connection_profiles;
  uint32_t connection_profiles_size = 0u;

  HRESULT hr =
      GetAllConnectionProfiles(&connection_profiles, &connection_profiles_size);

  EXPECT_EQ(hr, S_OK);
  EXPECT_NE(connection_profiles, nullptr);
  EXPECT_EQ(connection_profiles_size, 2u);
}

TEST_F(DiscoveryNetworkListWinTest, GetAllConnectionProfilesError) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  constexpr FakeWinrtNetworkStatus kErrorStatusList[] = {
      FakeWinrtNetworkStatus::kErrorRoGetActivationFactoryFailed,
      FakeWinrtNetworkStatus::
          kErrorNetworkInformationStaticsGetConnectionProfilesFailed,
      FakeWinrtNetworkStatus::kErrorVectorViewGetSizeFailed,
  };

  for (const FakeWinrtNetworkStatus error_status : kErrorStatusList) {
    fake_winrt_network_environment_.SimulateError(error_status);

    ComPtr<WinrtCollections::IVectorView<WinrtConnectivity::ConnectionProfile*>>
        connection_profiles;
    uint32_t connection_profiles_size = 0u;

    HRESULT hr = GetAllConnectionProfiles(&connection_profiles,
                                          &connection_profiles_size);

    EXPECT_EQ(hr, fake_winrt_network_environment_.MakeHresult(error_status))
        << " for error_status: " << static_cast<int>(error_status);
    EXPECT_EQ(connection_profiles, nullptr)
        << " for error_status: " << static_cast<int>(error_status);
    EXPECT_EQ(connection_profiles_size, 0u)
        << " for error_status: " << static_cast<int>(error_status);
  }
}

TEST_F(DiscoveryNetworkListWinTest,
       DiscoveryNetworkListWinTest_GetMacSsidMapUsingWinrtWhenEmpty) {
  auto mac_ssid_map = GetMacSsidMapUsingWinrt();
  EXPECT_EQ(mac_ssid_map.size(), 0u);
}

TEST_F(DiscoveryNetworkListWinTest, GetMacSsidMapUsingWinrt) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                NetworkConnectivityLevel_InternetAccess);

  auto mac_ssid_map = GetMacSsidMapUsingWinrt();
  EXPECT_EQ(mac_ssid_map.size(), 1u);

  const std::string kExpectedWifiMacAddress(kWifiMacAddressVector.begin(),
                                            kWifiMacAddressVector.end());

  auto it = mac_ssid_map.find(kExpectedWifiMacAddress);
  ASSERT_NE(it, mac_ssid_map.end());
  EXPECT_EQ(it->second, kWifiSsid);
}

TEST_F(DiscoveryNetworkListWinTest, GetMacSsidMapUsingWinrtWhenDisconnected) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_None);

  auto mac_ssid_map = GetMacSsidMapUsingWinrt();
  EXPECT_EQ(mac_ssid_map.size(), 0u);
}

TEST_F(DiscoveryNetworkListWinTest, GetMacSsidMapUsingWinrtErrors) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  constexpr FakeWinrtNetworkStatus kErrorStatusList[] = {
      FakeWinrtNetworkStatus::
          kErrorNetworkInformationStaticsGetConnectionProfilesFailed,
      FakeWinrtNetworkStatus::kErrorVectorViewGetAtFailed,
      FakeWinrtNetworkStatus::
          kErrorConnectionProfileGetNetworkConnectivityLevelFailed,
      FakeWinrtNetworkStatus::
          kErrorConnectionProfileGetWlanConnectionProfileDetailsFailed,
      FakeWinrtNetworkStatus::kErrorGetNetworkAdapterIdFailed,
  };
  for (const FakeWinrtNetworkStatus error_status : kErrorStatusList) {
    fake_winrt_network_environment_.SimulateError(error_status);

    auto mac_ssid_map = GetMacSsidMapUsingWinrt();
    EXPECT_EQ(mac_ssid_map.size(), 0u)
        << " for error_status: " << static_cast<int>(error_status);
  }
}

TEST_F(DiscoveryNetworkListWinTest, GetMacSsidMapUsingWinrtWithIpHelperErrors) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);
  fake_ip_helper_.SimulateError(FakeIpHelperStatus::kErrorGetIfTable2Failed);

  auto mac_ssid_map = GetMacSsidMapUsingWinrt();
  EXPECT_EQ(mac_ssid_map.size(), 0u);
}

TEST_F(DiscoveryNetworkListWinTest,
       GetMacSsidMapUsingWinrtWithMissingMacAddress) {
  fake_winrt_network_environment_.AddConnectionProfile(
      kWifiNetworkAdapterId,
      WinrtConnectivity::NetworkConnectivityLevel::
          NetworkConnectivityLevel_InternetAccess,
      kWifiSsid);

  auto mac_ssid_map = GetMacSsidMapUsingWinrt();
  EXPECT_EQ(mac_ssid_map.size(), 0u);
}

TEST_F(DiscoveryNetworkListWinTest, GetDiscoveryNetworkInfoListEmpty) {
  std::vector<DiscoveryNetworkInfo> network_info_list =
      GetDiscoveryNetworkInfoList();
  EXPECT_EQ(network_info_list.size(), 0u);
}

TEST_F(DiscoveryNetworkListWinTest, GetDiscoveryNetworkInfoList) {
  AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                NetworkConnectivityLevel_InternetAccess);
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  std::vector<DiscoveryNetworkInfo> network_info_list =
      GetDiscoveryNetworkInfoList();
  ASSERT_EQ(network_info_list.size(), 2u);

  EXPECT_EQ(network_info_list[0].name, kWiredAdapterName);
  EXPECT_EQ(network_info_list[0].network_id,
            base::HexEncode(kWiredMacAddress, std::size(kWiredMacAddress)));

  EXPECT_EQ(network_info_list[1].name, kWifiAdapterName);
  EXPECT_EQ(network_info_list[1].network_id, kWifiSsid);
}

TEST_F(DiscoveryNetworkListWinTest,
       GetDiscoveryNetworkInfoListWithBufferOverflow) {
  // Calculate the number of network adapters that we need to add to the fake
  // environment to cause the first call to GetAdaptersAddresses() to fail with
  // ERROR_BUFFER_OVERFLOW..
  const size_t kMaxAdapterCount =
      (kGetAdaptersAddressesInitialBufferSize / sizeof(IP_ADAPTER_ADDRESSES));

  const size_t kOverflowAdapterCount = (kMaxAdapterCount + 1);
  std::vector<DiscoveryNetworkInfo> expected_network_info_list;

  // Setup the fake environment with the required number of network adapters.
  for (size_t i = 0u; i < kOverflowAdapterCount; ++i) {
    const std::string kAdapterName =
        ("fake_adapter_name_" + base::NumberToString(i));

    GUID network_adapter_id;
    ASSERT_HRESULT_SUCCEEDED(CoCreateGuid(&network_adapter_id));

    const std::vector<uint8_t> kMacAddress = GenerateRandomMacAddress();

    const std::string kSsid = ("fake_ssid_" + base::NumberToString(i));

    AddConnectionProfile(kAdapterName, network_adapter_id, kMacAddress, kSsid,
                         WinrtConnectivity::NetworkConnectivityLevel::
                             NetworkConnectivityLevel_InternetAccess);

    expected_network_info_list.emplace_back(kAdapterName, kSsid);
  }
  StableSortDiscoveryNetworkInfo(expected_network_info_list.begin(),
                                 expected_network_info_list.end());

  std::vector<DiscoveryNetworkInfo> actual_network_info_list =
      GetDiscoveryNetworkInfoList();
  ASSERT_EQ(actual_network_info_list, expected_network_info_list);
}

TEST_F(DiscoveryNetworkListWinTest,
       GetDiscoveryNetworkInfoListWithBufferOverflowRepeated) {
  AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                NetworkConnectivityLevel_InternetAccess);

  fake_ip_helper_.SimulateError(
      FakeIpHelperStatus::kErrorGetAdaptersAddressesBufferOverflow);

  std::vector<DiscoveryNetworkInfo> network_info_list =
      GetDiscoveryNetworkInfoList();
  EXPECT_EQ(network_info_list.size(), 0u);
}

TEST_F(DiscoveryNetworkListWinTest,
       GetDiscoveryNetworkInfoListWithMissingSsid) {
  AddWiredConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                                NetworkConnectivityLevel_InternetAccess);
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  // Force GetAdaptersAddresses() to always fail with ERROR_BUFFER_OVERFLOW.
  fake_winrt_network_environment_.SimulateError(
      FakeWinrtNetworkStatus::
          kErrorNetworkInformationStaticsGetConnectionProfilesFailed);

  std::vector<DiscoveryNetworkInfo> network_info_list =
      GetDiscoveryNetworkInfoList();
  ASSERT_EQ(network_info_list.size(), 2u);

  EXPECT_EQ(network_info_list[0].name, kWiredAdapterName);
  EXPECT_EQ(network_info_list[0].network_id,
            base::HexEncode(kWiredMacAddress, std::size(kWiredMacAddress)));

  EXPECT_EQ(network_info_list[1].name, kWifiAdapterName);
  EXPECT_EQ(network_info_list[1].network_id,
            base::HexEncode(kWifiMacAddress, std::size(kWifiMacAddress)));
}

TEST_F(DiscoveryNetworkListWinTest,
       GetDiscoveryNetworkInfoListWithAdapterStatusDown) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  // Adapters with the IfOperStatusDown status must be ignored.
  fake_ip_helper_.AddNetworkInterface(
      kWiredAdapterName, kWiredNetworkAdapterId, kWiredMacAddressVector,
      IF_TYPE_ETHERNET_CSMACD, IfOperStatusDown);

  std::vector<DiscoveryNetworkInfo> network_info_list =
      GetDiscoveryNetworkInfoList();
  ASSERT_EQ(network_info_list.size(), 1u);

  EXPECT_EQ(network_info_list[0].name, kWifiAdapterName);
  EXPECT_EQ(network_info_list[0].network_id, kWifiSsid);
}

TEST_F(DiscoveryNetworkListWinTest,
       GetDiscoveryNetworkInfoListWithUnwantedAdapterType) {
  AddWifiConnectionProfile(WinrtConnectivity::NetworkConnectivityLevel::
                               NetworkConnectivityLevel_InternetAccess);

  // Adapters that are not ethernet or wifi must be ignored.
  fake_ip_helper_.AddNetworkInterface(kWiredAdapterName, kWiredNetworkAdapterId,
                                      kWiredMacAddressVector, IF_TYPE_TUNNEL,
                                      IfOperStatusUp);

  std::vector<DiscoveryNetworkInfo> network_info_list =
      GetDiscoveryNetworkInfoList();
  ASSERT_EQ(network_info_list.size(), 1u);

  EXPECT_EQ(network_info_list[0].name, kWifiAdapterName);
  EXPECT_EQ(network_info_list[0].network_id, kWifiSsid);
}

}  // namespace media_router
