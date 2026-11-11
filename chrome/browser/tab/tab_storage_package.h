// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/storage_package.h"

namespace base {
class Pickle;
}  // namespace base

namespace tabs {

// Stores the serialized metadata for a Tab. Live tabs pass their navigation
// Pickle so its bytes are moved into tabs_pb::TabState on the storage thread
// rather than copied on the UI thread.
class TabStoragePackage : public StoragePackage {
 public:
  TabStoragePackage(tabs_pb::TabState tab_state,
                    std::unique_ptr<base::Pickle> web_contents_state_pickle);
  ~TabStoragePackage() override;

  TabStoragePackage(const TabStoragePackage&) = delete;
  TabStoragePackage& operator=(const TabStoragePackage&) = delete;

  // Serializes the data contained within this package into a byte array for
  // storage. The first call moves the pickle bytes into `tab_state_`, so this
  // must not be called concurrently; repeated calls return the same bytes.
  std::vector<uint8_t> SerializePayload() const override;
  std::vector<uint8_t> SerializeChildren() const override;

 private:
  // Mutable so SerializePayload() can move the pickle bytes into `tab_state_`
  // in place and free the pickle, instead of copying the whole proto.
  mutable tabs_pb::TabState tab_state_;
  mutable std::unique_ptr<base::Pickle> web_contents_state_pickle_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_
