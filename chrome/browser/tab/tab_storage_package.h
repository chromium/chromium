// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/token.h"
#include "chrome/browser/tab/android_tab_package.h"
#include "chrome/browser/tab/storage_package.h"

namespace tabs {

// This class is used to store the data for a Tab, making it thread-agnostic.
struct TabStoragePackage : public StoragePackage {
 public:
  TabStoragePackage(int user_agent,
                    base::Token tab_group_id,
                    bool is_pinned,
                    AndroidTabPackage android_tab_package);
  ~TabStoragePackage() override;

  TabStoragePackage(const TabStoragePackage&) = delete;
  TabStoragePackage& operator=(const TabStoragePackage&) = delete;

  // Serializes the data contained within this package into a byte array for
  // storage.
  std::vector<uint8_t> SerializePayload() const override;
  std::vector<uint8_t> SerializeChildren() const override;

  const int user_agent_;
  const base::Token tab_group_id_;
  const bool is_pinned_;
  const AndroidTabPackage android_tab_package_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_
