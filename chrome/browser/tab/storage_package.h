// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_PACKAGE_H_
#define CHROME_BROWSER_TAB_STORAGE_PACKAGE_H_

#include <string>

namespace tabs {

// Interface for a package of data that can be serialized for storage.
class StoragePackage {
 public:
  virtual ~StoragePackage() = default;

  // Serializes the data contained within this package into a string payload for
  // storage.
  virtual std::string SerializePayload() const = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_PACKAGE_H_
