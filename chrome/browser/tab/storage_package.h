// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_PACKAGE_H_
#define CHROME_BROWSER_TAB_STORAGE_PACKAGE_H_

#include <string>
#include <vector>

#include "chrome/browser/tab/payload.h"

namespace tabs {

// Interface for a package of data that can be serialized for storage.
class StoragePackage : public Payload {
 public:
  // Serializes the identity and order of the direct children of the tab
  // collection represented by this package into a vector payload for storage.
  virtual std::vector<uint8_t> SerializeChildren() const = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_PACKAGE_H_
