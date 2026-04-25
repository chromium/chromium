// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_API_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_API_TEST_UTIL_H_

#include "components/storage_monitor/storage_info.h"

namespace extensions::test {

struct TestStorageUnitInfo {
  const char* device_id;
  const char* name;
  // Total amount of the storage device space, in bytes.
  double capacity;
  // The available amount of the storage space, in bytes.
  double available_capacity;
};

extern const struct TestStorageUnitInfo kRemovableStorageData;

storage_monitor::StorageInfo BuildStorageInfoFromTestStorageUnitInfo(
    const TestStorageUnitInfo& unit);

}  // namespace extensions::test

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_STORAGE_API_TEST_UTIL_H_
