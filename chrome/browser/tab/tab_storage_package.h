// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_

#include <cstdint>
#include <vector>

#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/storage_package.h"

namespace tabs {

// This class is used to store the data for a Tab, making it thread-agnostic.
class TabStoragePackage : public StoragePackage {
 public:
  explicit TabStoragePackage(tabs_pb::TabState tab_state);
  ~TabStoragePackage() override;

  TabStoragePackage(const TabStoragePackage&) = delete;
  TabStoragePackage& operator=(const TabStoragePackage&) = delete;

  // Serializes the data contained within this package into a byte array for
  // storage.
  std::vector<uint8_t> SerializePayload() const override;
  std::vector<uint8_t> SerializeChildren() const override;

  const tabs_pb::TabState& tab_state() const { return tab_state_; }

 private:
  tabs_pb::TabState tab_state_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_
