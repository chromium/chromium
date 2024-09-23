// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/router/discovery/test_support/win/fake_ip_helper.h"

#include "base/check_op.h"
#include "base/notreached.h"

namespace media_router {

FakeIpHelper::FakeIpHelper() = default;

FakeIpHelper::~FakeIpHelper() {
  CHECK_EQ(mib_tables_.size(), 0u);
}

void FakeIpHelper::SimulateError(FakeIpHelperStatus error_status) {
  error_status_ = error_status;
}

void FakeIpHelper::AddNetworkInterface(
    const std::string& adapter_name,
    const GUID& network_interface_guid,
    const std::vector<uint8_t>& physical_address,
    IFTYPE adapter_type,
    IF_OPER_STATUS adapter_status) {
  AddMibTableRow(network_interface_guid, physical_address);
  AddIpAdapterAddresses(adapter_name, physical_address, adapter_type,
                        adapter_status);
}

ULONG FakeIpHelper::GetAdaptersAddresses(
    ULONG family,
    ULONG flags,
    void* reserved,
    IP_ADAPTER_ADDRESSES* adapter_addresses,
    ULONG* size_pointer) {
  if (error_status_ ==
      FakeIpHelperStatus::kErrorGetAdaptersAddressesBufferOverflow) {
    return ERROR_BUFFER_OVERFLOW;
  }

  if (ip_adapter_addresses_.empty()) {
    return ERROR_NO_DATA;
  }

  const ULONG adapter_addresses_byte_count =
      (ip_adapter_addresses_.size() * sizeof(IP_ADAPTER_ADDRESSES));

  if (*size_pointer < adapter_addresses_byte_count) {
    *size_pointer = adapter_addresses_byte_count;
    return ERROR_BUFFER_OVERFLOW;
  }

  for (size_t i = 0; i < ip_adapter_addresses_.size(); ++i) {
    adapter_addresses[i] = *ip_adapter_addresses_[i].Get();
    if (i > 0) {
      adapter_addresses[i - 1].Next = &adapter_addresses[i];
    }
  }
  return ERROR_SUCCESS;
}

DWORD FakeIpHelper::GetIfTable2(MIB_IF_TABLE2** out_table) {
  if (error_status_ == FakeIpHelperStatus::kErrorGetIfTable2Failed) {
    return ERROR_NOT_FOUND;
  }

  mib_tables_.emplace_back(mib_table_rows_);
  *out_table = mib_tables_.back().Get();
  return ERROR_SUCCESS;
}

void FakeIpHelper::FreeMibTable(void* table) {
  for (auto it = mib_tables_.begin(); it != mib_tables_.end(); ++it) {
    if (it->Get() == table) {
      mib_tables_.erase(it);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void FakeIpHelper::AddIpAdapterAddresses(
    const std::string& adapter_name,
    const std::vector<uint8_t>& physical_address,
    IFTYPE adapter_type,
    IF_OPER_STATUS adapter_status) {
  ip_adapter_addresses_.emplace_back(adapter_name, physical_address,
                                     adapter_type, adapter_status);
}

void FakeIpHelper::AddMibTableRow(
    const GUID& network_interface_guid,
    const std::vector<uint8_t>& physical_address) {
  MIB_IF_ROW2 network_interface = {};
  network_interface.InterfaceGuid = network_interface_guid;

  CHECK_LE(physical_address.size(),
           static_cast<size_t>(IF_MAX_PHYS_ADDRESS_LENGTH));

  network_interface.PhysicalAddressLength = physical_address.size();
  memcpy(network_interface.PhysicalAddress, &physical_address[0],
         physical_address.size());

  mib_table_rows_.push_back(network_interface);
}

}  // namespace media_router
