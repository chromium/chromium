// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/router/discovery/discovery_network_list_win.h"

#include <winsock2.h>

#include <windot11.h>
#include <wlanapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/hstring_reference.h"
#include "base/win/scoped_hstring.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/media/router/discovery/discovery_network_list.h"

namespace WinrtConnectivity = ABI::Windows::Networking::Connectivity;
namespace WinrtCollections = ABI::Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;

namespace media_router {
namespace {

bool g_requires_network_location_permission_for_testing = false;

WindowsOsApi& GetWindowsOsApi() {
  static base::NoDestructor<WindowsOsApi> windows_os_api;
  return *windows_os_api;
}

void IfTable2Deleter(PMIB_IF_TABLE2 interface_table) {
  if (interface_table) {
    GetWindowsOsApi().ip_helper_api.free_mib_table_callback.Run(
        interface_table);
  }
}

}  // namespace

bool GuidOperatorLess::operator()(const GUID& guid1, const GUID& guid2) const {
  return memcmp(&guid1, &guid2, sizeof(GUID)) < 0;
}

typedef DWORD(WINAPI* WlanOpenHandleFunction)(DWORD dwClientVersion,
                                              PVOID pReserved,
                                              PDWORD pdwNegotiatedVersion,
                                              PHANDLE phClientHandle);
typedef DWORD(WINAPI* WlanCloseHandleFunction)(HANDLE hClientHandle,
                                               PVOID pReserved);
typedef DWORD(WINAPI* WlanEnumInterfacesFunction)(
    HANDLE hClientHandle,
    PVOID pReserved,
    PWLAN_INTERFACE_INFO_LIST* ppInterfaceList);
typedef DWORD(WINAPI* WlanQueryInterfaceFunction)(
    HANDLE hClientHandle,
    const GUID* pInterfaceGuid,
    WLAN_INTF_OPCODE OpCode,
    PVOID pReserved,
    PDWORD pdwDataSize,
    PVOID* ppData,
    PWLAN_OPCODE_VALUE_TYPE pWlanOpcodeValueType);
typedef VOID(WINAPI* WlanFreeMemoryFunction)(PVOID pMemory);

class WlanApi {
 public:
  const WlanOpenHandleFunction wlan_open_handle;
  const WlanCloseHandleFunction wlan_close_handle;
  const WlanEnumInterfacesFunction wlan_enum_interfaces;
  const WlanQueryInterfaceFunction wlan_query_interface;
  const WlanFreeMemoryFunction wlan_free_memory;

  static std::unique_ptr<WlanApi> Create() {
    static constexpr wchar_t kWlanDllPath[] =
        L"%WINDIR%\\system32\\wlanapi.dll";
    auto path = base::win::ExpandEnvironmentVariables(kWlanDllPath);
    if (!path) {
      return nullptr;
    }

    HINSTANCE library =
        LoadLibraryEx(path->c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!library) {
      return nullptr;
    }

    return base::WrapUnique(new WlanApi(library));
  }

  WlanApi(const WlanApi&) = delete;
  WlanApi& operator=(const WlanApi&) = delete;

  ~WlanApi() { FreeLibrary(library_); }

 private:
  explicit WlanApi(HINSTANCE library)
      : wlan_open_handle(reinterpret_cast<WlanOpenHandleFunction>(
            GetProcAddress(library, "WlanOpenHandle"))),
        wlan_close_handle(reinterpret_cast<WlanCloseHandleFunction>(
            GetProcAddress(library, "WlanCloseHandle"))),
        wlan_enum_interfaces(reinterpret_cast<WlanEnumInterfacesFunction>(
            GetProcAddress(library, "WlanEnumInterfaces"))),
        wlan_query_interface(reinterpret_cast<WlanQueryInterfaceFunction>(
            GetProcAddress(library, "WlanQueryInterface"))),
        wlan_free_memory(reinterpret_cast<WlanFreeMemoryFunction>(
            GetProcAddress(library, "WlanFreeMemory"))),
        library_(library) {
    DCHECK(library);
    DCHECK(wlan_open_handle);
    DCHECK(wlan_close_handle);
    DCHECK(wlan_enum_interfaces);
    DCHECK(wlan_query_interface);
    DCHECK(wlan_free_memory);
  }

  HINSTANCE library_;
};

class ScopedWlanClientHandle {
 public:
  explicit ScopedWlanClientHandle(
      const WlanCloseHandleFunction wlan_close_handle)
      : wlan_close_handle_(wlan_close_handle) {}

  ScopedWlanClientHandle(const ScopedWlanClientHandle&) = delete;
  ScopedWlanClientHandle& operator=(const ScopedWlanClientHandle&) = delete;

  ~ScopedWlanClientHandle() {
    if (handle != nullptr) {
      wlan_close_handle_(handle, nullptr);
    }
  }

  HANDLE handle = nullptr;

 private:
  const WlanCloseHandleFunction wlan_close_handle_;
};

// Returns a map from a network interface's GUID to its MAC address.  This
// enumerates all network interfaces, not just wireless interfaces.
base::small_map<std::map<GUID, std::string, GuidOperatorLess>>
GetInterfaceGuidMacMap() {
  PMIB_IF_TABLE2 interface_table_raw = nullptr;
  auto result = GetWindowsOsApi().ip_helper_api.get_if_table2_callback.Run(
      &interface_table_raw);
  if (result != ERROR_SUCCESS) {
    return {};
  }
  std::unique_ptr<MIB_IF_TABLE2, decltype(&IfTable2Deleter)> interface_table(
      interface_table_raw, IfTable2Deleter);

  base::small_map<std::map<GUID, std::string, GuidOperatorLess>> guid_mac_map;
  for (ULONG i = 0; i < interface_table->NumEntries; ++i) {
    const auto* interface_row = &interface_table->Table[i];
    guid_mac_map.emplace(interface_row->InterfaceGuid,
                         std::string{reinterpret_cast<const char*>(
                                         interface_row->PhysicalAddress),
                                     interface_row->PhysicalAddressLength});
  }

  return guid_mac_map;
}

// Returns the associated SSID of an interface identified by its interface GUID.
// If it is not a wireless interface or if it's not currently associated with a
// network, it returns an empty string.
std::string GetSsidForInterfaceGuid(const HANDLE wlan_client_handle,
                                    const WlanApi& wlan_api,
                                    const GUID& interface_guid) {
  WLAN_CONNECTION_ATTRIBUTES* connection_info_raw = nullptr;
  DWORD connection_info_size = 0;
  auto result = wlan_api.wlan_query_interface(
      wlan_client_handle, &interface_guid, wlan_intf_opcode_current_connection,
      nullptr, &connection_info_size,
      reinterpret_cast<void**>(&connection_info_raw), nullptr);
  if (result != ERROR_SUCCESS) {
    // We can't get the SSID for this interface so its network ID will
    // fall back to its MAC address below.
    return {};
  }
  std::unique_ptr<WLAN_CONNECTION_ATTRIBUTES, WlanFreeMemoryFunction>
      connection_info(connection_info_raw, wlan_api.wlan_free_memory);
  if (connection_info->isState != wlan_interface_state_connected) {
    return {};
  }
  const auto* ssid = &connection_info->wlanAssociationAttributes.dot11Ssid;
  return std::string(reinterpret_cast<const char*>(ssid->ucSSID),
                     ssid->uSSIDLength);
}

// Returns a map from a network adapter's MAC address to its currently
// associated WiFi SSID.
base::small_map<std::map<std::string, std::string>> GetMacSsidMap() {
  auto wlan_api = WlanApi::Create();
  if (!wlan_api) {
    return {};
  }
  ScopedWlanClientHandle wlan_client_handle(wlan_api->wlan_close_handle);
  constexpr DWORD kWlanClientVersion = 2;
  DWORD wlan_current_version = 0;

  auto result = wlan_api->wlan_open_handle(kWlanClientVersion, nullptr,
                                           &wlan_current_version,
                                           &wlan_client_handle.handle);
  if (result != ERROR_SUCCESS) {
    return {};
  }

  PWLAN_INTERFACE_INFO_LIST wlan_interface_list_raw = nullptr;
  result = wlan_api->wlan_enum_interfaces(wlan_client_handle.handle, nullptr,
                                          &wlan_interface_list_raw);
  if (result != ERROR_SUCCESS) {
    return {};
  }

  std::unique_ptr<WLAN_INTERFACE_INFO_LIST, WlanFreeMemoryFunction>
      wlan_interface_list(wlan_interface_list_raw, wlan_api->wlan_free_memory);
  auto guid_mac_map = GetInterfaceGuidMacMap();
  base::small_map<std::map<std::string, std::string>> mac_ssid_map;

  // This loop takes each wireless interface and maps its MAC address to its
  // associated SSID, if it has one.  Each wireless interface has an interface
  // GUID which we can use to get its MAC address via |guid_mac_map| and its
  // associated SSID via WlanQueryInterface.
  for (DWORD i = 0; i < wlan_interface_list->dwNumberOfItems; ++i) {
    const auto* interface_info = &wlan_interface_list->InterfaceInfo[i];
    const auto mac_entry = guid_mac_map.find(interface_info->InterfaceGuid);
    if (mac_entry == guid_mac_map.end()) {
      continue;
    }
    auto ssid = GetSsidForInterfaceGuid(wlan_client_handle.handle, *wlan_api,
                                        interface_info->InterfaceGuid);
    if (ssid.empty()) {
      continue;
    }
    mac_ssid_map.emplace(mac_entry->second, std::move(ssid));
  }
  return mac_ssid_map;
}

// Returns true when running on a Windows version that prompts for network
// location permission.  At the time of this writing, the permission prompt
// exists in Win11 24H2 only, which does not have a final build yet, but is
// shipping publicly to Windows Insiders.
//
// See the following documentation for more detail:
//
// Changes to API behavior for Wi-Fi access and location
// https://learn.microsoft.com/en-us/windows/win32/nativewifi/wi-fi-access-location-changes
//
// Announcing Windows 11 Insider Preview Build 25977 (Canary Channel)
// https://blogs.windows.com/windows-insider/2023/10/18/announcing-windows-11-insider-preview-build-25977-canary-channel/
[[nodiscard]] bool RequiresNetworkLocationPermission() {
  const base::win::OSInfo::VersionNumber& os_version =
      base::win::OSInfo::GetInstance()->version_number();

  // Win11 uses 10 for its major version number.
  const bool is_24h2_or_greater =
      (os_version.major > 10) ||
      (os_version.major == 10 && os_version.build >= 25977);
  return is_24h2_or_greater ||
         g_requires_network_location_permission_for_testing;
}

// Returns true when `connection_profile` is connected to a local network or the
// internet.
bool IsProfileConnectedToNetwork(
    WinrtConnectivity::IConnectionProfile* connection_profile) {
  WinrtConnectivity::NetworkConnectivityLevel connectivity;
  HRESULT hr = connection_profile->GetNetworkConnectivityLevel(&connectivity);
  if (hr != S_OK) {
    return false;
  }
  return connectivity != WinrtConnectivity::NetworkConnectivityLevel::
                             NetworkConnectivityLevel_None;
}

// Returns the GUID for the network interface adapter used by
// `connection_profile`.
HRESULT GetProfileNetworkAdapterId(
    WinrtConnectivity::IConnectionProfile* connection_profile,
    GUID* network_adapter_id) {
  ComPtr<WinrtConnectivity::INetworkAdapter> network_adapter;
  {
    // INetworkAdapter::get_NetworkAdapter() may load the module
    // Windows.Networking.HostName.dll. Temporarily boost the priority of this
    // background thread to avoid causing jank by blocking the UI thread from
    // loading modules. For more details, see https://crbug.com/973868.
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();

    HRESULT hr = connection_profile->get_NetworkAdapter(&network_adapter);
    if (hr != S_OK) {
      return hr;
    }
  }
  return network_adapter->get_NetworkAdapterId(network_adapter_id);
}

// Returns the WiFi SSID used by the`connection_profile` for network
// connectivity. Returns an error when `connection_profile` does not use a WiFi
// network adapter.
HRESULT GetProfileWifiSSID(
    WinrtConnectivity::IConnectionProfile* connection_profile,
    HSTRING* out_ssid) {
  ComPtr<WinrtConnectivity::IConnectionProfile2> connection_profile2;
  HRESULT hr =
      connection_profile->QueryInterface(IID_PPV_ARGS(&connection_profile2));
  if (hr != S_OK) {
    return hr;
  }

  ComPtr<WinrtConnectivity::IWlanConnectionProfileDetails>
      wlan_connection_details;
  hr = connection_profile2->get_WlanConnectionProfileDetails(
      &wlan_connection_details);
  if (hr != S_OK) {
    return hr;
  }

  if (wlan_connection_details == nullptr) {
    // `connection_profile` is not using WiFi.
    return kWifiNotSupported;
  }
  return wlan_connection_details->GetConnectedSsid(out_ssid);
}

HRESULT GetAllConnectionProfiles(
    ComPtr<WinrtCollections::IVectorView<
        WinrtConnectivity::ConnectionProfile*>>* out_connection_profiles,
    uint32_t* out_connection_profiles_size) {
  ComPtr<WinrtConnectivity::INetworkInformationStatics>
      network_information_statics;
  {
    // RoGetActivationFactory() may load the Windows.Networking.Connectivity.dll
    // module. Temporarily boost the priority of this background thread to avoid
    // causing jank by blocking the UI thread from loading modules. For more
    // details, see https://crbug.com/973868.
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();

    HRESULT hr =
        GetWindowsOsApi().winrt_api.ro_get_activation_factory_callback.Run(
            base::win::HStringReference(
                RuntimeClass_Windows_Networking_Connectivity_NetworkInformation)
                .Get(),
            IID_PPV_ARGS(&network_information_statics));
    if (hr != S_OK) {
      return hr;
    }
  }

  ComPtr<WinrtCollections::IVectorView<WinrtConnectivity::ConnectionProfile*>>
      connection_profiles;
  HRESULT hr =
      network_information_statics->GetConnectionProfiles(&connection_profiles);
  if (hr != S_OK) {
    return hr;
  }

  uint32_t connection_profiles_size;
  hr = connection_profiles->get_Size(&connection_profiles_size);
  if (hr != S_OK) {
    return hr;
  }

  *out_connection_profiles = connection_profiles;
  *out_connection_profiles_size = connection_profiles_size;
  return S_OK;
}

// Returns a map from a network adapter's MAC address to its currently
// associated WiFi SSID using WinRT Network APIs instead of Win32 WLAN APIS.
// In particular, uses IWlanConnectionProfileDetails::GetConnectedSsid() to get
// the SSID without prompting the user for network location permission.  The
// Win32 version uses WlanQueryInterface(), which prompts for permission in
// Win11 24H2.
base::small_map<std::map<std::string, std::string>> GetMacSsidMapUsingWinrt() {
  ComPtr<WinrtCollections::IVectorView<WinrtConnectivity::ConnectionProfile*>>
      connection_profiles;
  uint32_t connection_profiles_size = 0u;
  HRESULT hr =
      GetAllConnectionProfiles(&connection_profiles, &connection_profiles_size);
  if (hr != S_OK) {
    return {};
  }

  auto guid_mac_map = GetInterfaceGuidMacMap();
  base::small_map<std::map<std::string, std::string>> mac_ssid_map;

  // This loop finds each connected wireless interface, mapping its MAC address
  // to its SSID.
  for (uint32_t i = 0u; i < connection_profiles_size; ++i) {
    ComPtr<WinrtConnectivity::IConnectionProfile> connection_profile;
    hr = connection_profiles->GetAt(i, &connection_profile);
    if (hr != S_OK) {
      continue;
    }

    if (!IsProfileConnectedToNetwork(connection_profile.Get())) {
      // Skip disconnected profiles.
      continue;
    }

    HSTRING ssid_hstring;
    hr = GetProfileWifiSSID(connection_profile.Get(), &ssid_hstring);
    if (hr != S_OK) {
      // Skip ethernet and cellular profiles.
      continue;
    }
    base::win::ScopedHString ssid(ssid_hstring);

    GUID network_adapter_id;
    hr = GetProfileNetworkAdapterId(connection_profile.Get(),
                                    &network_adapter_id);
    if (hr != S_OK) {
      continue;
    }

    const auto mac_entry = guid_mac_map.find(network_adapter_id);
    if (mac_entry == guid_mac_map.end()) {
      continue;
    }

    mac_ssid_map.emplace(/*wifi_network_adapter_mac_address=*/mac_entry->second,
                         ssid.GetAsUTF8());
  }
  return mac_ssid_map;
}

std::vector<DiscoveryNetworkInfo> GetDiscoveryNetworkInfoList() {
  // Max number of times to retry GetAdaptersAddresses due to
  // ERROR_BUFFER_OVERFLOW. If GetAdaptersAddresses returns this indefinitely
  // due to an unforeseen reason, we don't want to be stuck in an endless loop.
  constexpr int kMaxGetAdaptersAddressTries = 10;

  constexpr ULONG kAddressFlags =
      GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
      GAA_FLAG_SKIP_DNS_SERVER;

  // Although we need to provide GetAdaptersAddresses with a buffer, there's no
  // way to know what size to use.  We use a best-guess here but when
  // GetAdaptersAddresses returns ERROR_BUFFER_OVERFLOW, it means our guess was
  // too small.  When this happens it will also reset |addresses_buffer_size| to
  // the required size.  Although it's very unlikely that two successive calls
  // will both require increasing the buffer size, there's no guarantee that
  // this won't happen; this is what the maximum retry count guards against.
  ULONG addresses_buffer_size = kGetAdaptersAddressesInitialBufferSize;
  std::unique_ptr<char[]> addresses_buffer;
  PIP_ADAPTER_ADDRESSES adapter_addresses = nullptr;
  ULONG result = ERROR_BUFFER_OVERFLOW;
  for (int i = 0;
       result == ERROR_BUFFER_OVERFLOW && i < kMaxGetAdaptersAddressTries;
       ++i) {
    addresses_buffer.reset(new char[addresses_buffer_size]);
    adapter_addresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(addresses_buffer.get());
    result =
        GetWindowsOsApi().ip_helper_api.get_adapters_addresses_callback.Run(
            AF_UNSPEC, kAddressFlags, nullptr, adapter_addresses,
            &addresses_buffer_size);
  }

  if (result != NO_ERROR) {
    return {};
  }

  std::vector<DiscoveryNetworkInfo> network_ids;
  base::small_map<std::map<std::string, std::string>> mac_ssid_map;
  if (RequiresNetworkLocationPermission()) {
    mac_ssid_map = GetMacSsidMapUsingWinrt();
  } else {
    mac_ssid_map = GetMacSsidMap();
  }
  for (const IP_ADAPTER_ADDRESSES* current_adapter = adapter_addresses;
       current_adapter != nullptr; current_adapter = current_adapter->Next) {
    // We only want adapters which are up and either Ethernet or wireless, so we
    // skip everything else here.
    if (current_adapter->OperStatus != IfOperStatusUp ||
        (current_adapter->IfType != IF_TYPE_ETHERNET_CSMACD &&
         current_adapter->IfType != IF_TYPE_IEEE80211)) {
      continue;
    }

    // We have to use a slightly roundabout way to get the SSID for each
    // adapter:
    // - Enumerate wifi devices to get list of interface GUIDs.
    // - Enumerate interfaces to get interface GUID -> physical address map.
    // - Map interface GUIDs to SSID.
    // - Use GUID -> MAC map to do MAC -> interface GUID  -> SSID.
    // Although it's theoretically possible to have multiple interfaces per
    // adapter, most wireless cards don't actually allow multiple
    // managed-mode interfaces.  However, in the event that there really
    // are multiple interfaces per adapter (i.e. physical address), we will
    // simply use the SSID of the first match.  It's unclear how Windows would
    // handle this case since it's somewhat loose with its use of the words
    // "adapter" and "interface".
    std::string name(current_adapter->AdapterName);
    if (current_adapter->IfType == IF_TYPE_IEEE80211) {
      std::string adapter_mac(
          reinterpret_cast<const char*>(current_adapter->PhysicalAddress),
          current_adapter->PhysicalAddressLength);
      const auto ssid_entry = mac_ssid_map.find(adapter_mac);
      if (ssid_entry != mac_ssid_map.end()) {
        network_ids.emplace_back(name, ssid_entry->second);
        continue;
      }
    }
    network_ids.emplace_back(
        name, base::HexEncode(current_adapter->PhysicalAddress,
                              current_adapter->PhysicalAddressLength));
  }

  StableSortDiscoveryNetworkInfo(network_ids.begin(), network_ids.end());

  return network_ids;
}

WindowsOsApi::WindowsOsApi() = default;
WindowsOsApi::WindowsOsApi(const WindowsOsApi& source) = default;
WindowsOsApi::~WindowsOsApi() = default;

WindowsOsApi::IpHelperApi::IpHelperApi() = default;
WindowsOsApi::IpHelperApi::IpHelperApi(const IpHelperApi& source) = default;
WindowsOsApi::IpHelperApi::~IpHelperApi() = default;

WindowsOsApi::WinrtApi::WinrtApi() = default;
WindowsOsApi::WinrtApi::WinrtApi(const WinrtApi& source) = default;
WindowsOsApi::WinrtApi::~WinrtApi() = default;

void OverrideWindowsOsApiForTesting(WindowsOsApi overridden_api) {
  GetWindowsOsApi() = overridden_api;
}

void OverrideRequiresNetworkLocationPermissionForTesting(  // IN-TEST
    bool requires_permission) {
  g_requires_network_location_permission_for_testing = requires_permission;
}

}  // namespace media_router
