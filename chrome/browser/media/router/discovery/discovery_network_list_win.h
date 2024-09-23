// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_LIST_WIN_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_LIST_WIN_H_

#include <ws2tcpip.h>

#include <iphlpapi.h>
#include <roapi.h>
#include <windows.networking.connectivity.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/small_map.h"
#include "base/functional/callback.h"
#include "base/win/core_winrt_util.h"

namespace media_router {
// Declares helper functions and constants used to implement
// GetDiscoveryNetworkInfoList() on Windows.  Exposes these helpers for unit
// testing.

// To receive the output from GetAdaptersAddresses(), use an initial buffer size
// of 15KB, as recommended by MSDN. See:
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365915(v=vs.85).aspx
inline constexpr int kGetAdaptersAddressesInitialBufferSize = 15000;

// Returned by GetProfileWifiSSID() for network profiles that do not use WiFi.
inline constexpr HRESULT kWifiNotSupported =
    HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

struct GuidOperatorLess {
  bool operator()(const GUID& guid1, const GUID& guid2) const;
};

base::small_map<std::map<GUID, std::string, GuidOperatorLess>>
GetInterfaceGuidMacMap();

[[nodiscard]] bool IsProfileConnectedToNetwork(
    ABI::Windows::Networking::Connectivity::IConnectionProfile*
        connection_profile);

[[nodiscard]] HRESULT GetProfileNetworkAdapterId(
    ABI::Windows::Networking::Connectivity::IConnectionProfile*
        connection_profile,
    GUID* network_adapter_id);

[[nodiscard]] HRESULT GetProfileWifiSSID(
    ABI::Windows::Networking::Connectivity::IConnectionProfile*
        connection_profile,
    HSTRING* out_ssid);

[[nodiscard]] HRESULT GetAllConnectionProfiles(
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVectorView<
        ABI::Windows::Networking::Connectivity::ConnectionProfile*>>*
        out_connection_profiles,
    uint32_t* out_connection_profiles_size);

base::small_map<std::map<std::string, std::string>> GetMacSsidMapUsingWinrt();

// Enable tests to override Windows OS APIs to simulate different networking
// environments.  Contains callbacks for each OS API function used by
// GetDiscoveryNetworkInfoList().  By default, the callbacks bind to the actual
// OS API function.  Tests may bind these callbacks to fake implementations of
// the OS APIs and then call OverrideWindowsOsApiForTesting().
struct WindowsOsApi {
  WindowsOsApi();
  WindowsOsApi(const WindowsOsApi& source);
  ~WindowsOsApi();

  // Override Win32 functions declared in iphlpapi.h.
  struct IpHelperApi {
    IpHelperApi();
    IpHelperApi(const IpHelperApi& source);
    ~IpHelperApi();

    // Callbacks for GetAdaptersAddresses, GetIfTable2, and FreeMibTable.
    // https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses
    using GetAdaptersAddressesCallback =
        base::RepeatingCallback<ULONG(ULONG family,
                                      ULONG flags,
                                      void* reserved,
                                      IP_ADAPTER_ADDRESSES* adapter_addresses,
                                      ULONG* size_pointer)>;
    GetAdaptersAddressesCallback get_adapters_addresses_callback =
        base::BindRepeating(&GetAdaptersAddresses);

    // https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getiftable2
    using GetIfTable2Callback =
        base::RepeatingCallback<DWORD(MIB_IF_TABLE2** out_table)>;
    GetIfTable2Callback get_if_table2_callback =
        base::BindRepeating(&GetIfTable2);

    // https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-freemibtable
    using FreeMibTableCallback = base::RepeatingCallback<void(void* table)>;
    FreeMibTableCallback free_mib_table_callback =
        base::BindRepeating(&FreeMibTable);
  };
  IpHelperApi ip_helper_api;

  // Override RoGetActivationFactory() to return fake WinRT objects.
  struct WinrtApi {
    WinrtApi();
    WinrtApi(const WinrtApi& source);
    ~WinrtApi();

    // https://learn.microsoft.com/en-us/windows/win32/api/roapi/nf-roapi-rogetactivationfactory
    using RoGetActivationFactoryCallback =
        base::RepeatingCallback<HRESULT(HSTRING, const IID&, void**)>;
    RoGetActivationFactoryCallback ro_get_activation_factory_callback =
        base::BindRepeating(&base::win::RoGetActivationFactory);
  };
  WinrtApi winrt_api;
};
void OverrideWindowsOsApiForTesting(WindowsOsApi overridden_api);

// Set to true to always use the WinRT implementation of
// GetDiscoveryNetworkInfoList().
void OverrideRequiresNetworkLocationPermissionForTesting(
    bool requires_permission);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_LIST_WIN_H_
