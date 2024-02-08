// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/test_support/win/fake_ip_adapter_addresses.h"

#include "base/check_op.h"

namespace media_router {

FakeIpAdapterAddresses::FakeIpAdapterAddresses(
    const std::string& adapter_name,
    const std::vector<uint8_t>& physical_address,
    IFTYPE adapter_type,
    IF_OPER_STATUS adapter_status)
    : adapter_name_(adapter_name) {
  // Only populate struct members that are used by Chromium.
  value_ = {};
  value_.Length = sizeof(IP_ADAPTER_ADDRESSES);
  value_.IfType = adapter_type;
  value_.OperStatus = adapter_status;
  value_.AdapterName = const_cast<char*>(adapter_name_.c_str());

  CHECK_LE(physical_address.size(),
           static_cast<size_t>(MAX_ADAPTER_ADDRESS_LENGTH));

  value_.PhysicalAddressLength = physical_address.size();
  memcpy(value_.PhysicalAddress, physical_address.data(),
         physical_address.size());
}

FakeIpAdapterAddresses::FakeIpAdapterAddresses(
    const FakeIpAdapterAddresses& source) {
  adapter_name_ = source.adapter_name_;
  value_ = source.value_;
  value_.AdapterName = const_cast<char*>(adapter_name_.c_str());
}

FakeIpAdapterAddresses::~FakeIpAdapterAddresses() = default;

IP_ADAPTER_ADDRESSES* FakeIpAdapterAddresses::Get() {
  return &value_;
}

}  // namespace media_router
