// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_MIB_TABLE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_MIB_TABLE_H_

#include <ws2tcpip.h>

#include <iphlpapi.h>

#include <vector>

namespace media_router {

// Contains the `MIB_IF_TABLE2` structure returned by the fake implementation of
// `GetIfTable2()` in `FakeIpHelper`.
class FakeMibTable final {
 public:
  explicit FakeMibTable(
      const std::vector<MIB_IF_ROW2>& source_network_interfaces);
  FakeMibTable(const FakeMibTable& source);
  ~FakeMibTable();

  MIB_IF_TABLE2* Get();

 private:
  // A blob which makes up the `MIB_IF_TABLE2` structure. The `MIB_IF_TABLE2` is
  // a length prefixed array of `MIB_IF_ROW2` structures with some additional
  // padding in between the length and the array.  Here's the format:
  //   <ULONG> NumEntries
  //   <Padding>
  //   <MIB_IF_ROW2>  Array[NumEntries]
  std::vector<uint8_t> mib_table_bytes_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_MIB_TABLE_H_
