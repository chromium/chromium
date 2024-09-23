// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_IP_HELPER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_IP_HELPER_H_

#include <ws2tcpip.h>

#include <iphlpapi.h>

#include <string>
#include <vector>

#include "chrome/browser/media/router/discovery/test_support/win/fake_ip_adapter_addresses.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_mib_table.h"

namespace media_router {

// Each value represents a different IP Helper Win32 API that can fail during
// GetDiscoveryNetworkInfoList().  Use with
// `FakeIpHelper::SimulateError` to simulate Win32 API failures
// that return Windows system error codes.
enum class FakeIpHelperStatus {
  kOk = 0,
  kErrorGetIfTable2Failed = 1,
  kErrorGetAdaptersAddressesBufferOverflow = 2,
};

// Provides a fake implementation of the Win32 APIs used to enumerate network
// adapters.  Tests should use this class to simulate different
// network environments with different types of adapters and different statuses.
class FakeIpHelper final {
 public:
  FakeIpHelper();
  ~FakeIpHelper();

  void SimulateError(FakeIpHelperStatus error_status);

  // Sets up the fake network environment by creating fake network adapters.
  // Stores fake network adapters in the `mib_table_rows_` and
  // `ip_adapter_addresses_` members.
  void AddNetworkInterface(const std::string& adapter_name,
                           const GUID& network_interface_guid,
                           const std::vector<uint8_t>& physical_address,
                           IFTYPE adapter_type,
                           IF_OPER_STATUS adapter_status);

  // Implements the `GetAdaptersAddresses()` Win32 API.  Copies
  // `ip_adapter_addresses_` to `adapter_addresses`.
  ULONG GetAdaptersAddresses(ULONG family,
                             ULONG flags,
                             void* reserved,
                             IP_ADAPTER_ADDRESSES* adapter_addresses,
                             ULONG* size_pointer);

  // Implements the `GetIfTable2()` Win32 API. Creates a new `FakeMibTable`
  // stored in `mib_tables_` that is returned through `out_table`.
  // `FreeMibTable()` removes the `FakeMibTable` from `mib_tables_`.
  DWORD GetIfTable2(MIB_IF_TABLE2** out_table);
  void FreeMibTable(void* table);

 private:
  // Adds a fake network adapter to `ip_adapter_addresses_`.
  void AddIpAdapterAddresses(const std::string& adapter_name,
                             const std::vector<uint8_t>& physical_address,
                             IFTYPE adapter_type,
                             IF_OPER_STATUS adapter_status);

  // Adds a fake network adapter to `mib_table_rows_`.
  void AddMibTableRow(const GUID& network_interface_guid,
                      const std::vector<uint8_t>& physical_address);

  FakeIpHelperStatus error_status_ = FakeIpHelperStatus::kOk;

  std::vector<FakeIpAdapterAddresses> ip_adapter_addresses_;

  std::vector<MIB_IF_ROW2> mib_table_rows_;
  std::vector<FakeMibTable> mib_tables_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_IP_HELPER_H_
