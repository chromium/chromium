// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_IP_ADAPTER_ADDRESSES_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_IP_ADAPTER_ADDRESSES_H_

#include <ws2tcpip.h>

#include <iphlpapi.h>

#include <string>
#include <vector>

namespace media_router {

// Implements the IP_ADAPTER_ADDRESSES structure returned by the fake
// implementation of GetAdaptersAddresses in FakeIpHelper.
class FakeIpAdapterAddresses final {
 public:
  FakeIpAdapterAddresses(const std::string& adapter_name,
                         const std::vector<uint8_t>& physical_address,
                         IFTYPE adapter_type,
                         IF_OPER_STATUS adapter_status);
  FakeIpAdapterAddresses(const FakeIpAdapterAddresses& source);
  ~FakeIpAdapterAddresses();

  IP_ADAPTER_ADDRESSES* Get();

 private:
  // In `value_`, the struct member, `IP_ADAPTER_ADDRESSES::AdapterName`, points
  // to this string.
  std::string adapter_name_;

  IP_ADAPTER_ADDRESSES value_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_IP_ADAPTER_ADDRESSES_H_
