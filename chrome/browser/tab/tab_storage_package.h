// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
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
                    std::unique_ptr<AndroidTabPackage> android_tab_package);
  ~TabStoragePackage() override;

  TabStoragePackage(const TabStoragePackage&) = delete;
  TabStoragePackage& operator=(const TabStoragePackage&) = delete;

  // Serializes the data contained within this package into a string payload for
  // storage.
  std::string SerializePayload() const override;
  std::string SerializeChildren() const override;

  const int user_agent_;
  const base::Token tab_group_id_;
  const bool is_pinned_;
  const std::unique_ptr<AndroidTabPackage> android_tab_package_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGE_H_
