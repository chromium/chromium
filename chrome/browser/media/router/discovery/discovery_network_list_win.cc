// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iphlpapi.h>  // NOLINT
#include <windot11.h>  // NOLINT
#include <wlanapi.h>   // NOLINT

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/small_map.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"

namespace media_router {
namespace {

struct GuidOperatorLess {
  bool operator()(const GUID& guid1, const GUID& guid2) const {
    return memcmp(&guid1, &guid2, sizeof(GUID)) < 0;
  }
};

void IfTable2Deleter(PMIB_IF_TABLE2 interface_table) {
  if (interface_table) {
    FreeMibTable(interface_table);
  }
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
    static const wchar_t* kWlanDllPath = L"%WINDIR%\\system32\\wlanapi.dll";
    wchar_t path[MAX_PATH] = {0};
    ExpandEnvironmentStrings(kWlanDllPath, path, std::size(path));
    HINSTANCE library =
        LoadLibraryEx(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
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
  auto result = GetIfTable2(&interface_table_raw);
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

}  // namespace

std::vector<DiscoveryNetworkInfo> GetDiscoveryNetworkInfoList() {
  // Max number of times to retry GetAdaptersAddresses due to
  // ERROR_BUFFER_OVERFLOW. If GetAdaptersAddresses returns this indefinitely
  // due to an unforeseen reason, we don't want to be stuck in an endless loop.
  constexpr int kMaxGetAdaptersAddressTries = 10;

  // Use an initial buffer size of 15KB, as recommended by MSDN. See:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365915(v=vs.85).aspx
  constexpr int kInitialAddressBufferSize = 15000;

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
  ULONG addresses_buffer_size = kInitialAddressBufferSize;
  std::unique_ptr<char[]> addresses_buffer;
  PIP_ADAPTER_ADDRESSES adapter_addresses = nullptr;
  ULONG result = ERROR_BUFFER_OVERFLOW;
  for (int i = 0;
       result == ERROR_BUFFER_OVERFLOW && i < kMaxGetAdaptersAddressTries;
       ++i) {
    addresses_buffer.reset(new char[addresses_buffer_size]);
    adapter_addresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(addresses_buffer.get());
    result = GetAdaptersAddresses(AF_UNSPEC, kAddressFlags, nullptr,
                                  adapter_addresses, &addresses_buffer_size);
  }

  if (result != NO_ERROR) {
    return {};
  }

  std::vector<DiscoveryNetworkInfo> network_ids;
  auto mac_ssid_map = GetMacSsidMap();
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

}  // namespace media_router
