// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/router/discovery/test_support/win/fake_mib_table.h"

namespace media_router {

FakeMibTable::FakeMibTable(
    const std::vector<MIB_IF_ROW2>& source_network_interfaces) {
  size_t mib_table_byte_count = sizeof(MIB_IF_TABLE2);
  if (source_network_interfaces.size() > 1) {
    // MIB_IF_TABLE2 contains one MIB_IF_ROW2, so we need to add space for the
    // rest of the array.
    mib_table_byte_count +=
        (source_network_interfaces.size() - 1) * sizeof(MIB_IF_ROW2);
  }
  mib_table_bytes_.resize(mib_table_byte_count);

  MIB_IF_TABLE2* mib_table = Get();
  mib_table->NumEntries = source_network_interfaces.size();
  for (size_t i = 0; i < source_network_interfaces.size(); ++i) {
    mib_table->Table[i] = source_network_interfaces[i];
  }
}

FakeMibTable::FakeMibTable(const FakeMibTable& source) = default;

FakeMibTable::~FakeMibTable() = default;

MIB_IF_TABLE2* FakeMibTable::Get() {
  return reinterpret_cast<MIB_IF_TABLE2*>(&mib_table_bytes_[0]);
}

}  // namespace media_router
