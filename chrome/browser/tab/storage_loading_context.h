// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_LOADING_CONTEXT_H_
#define CHROME_BROWSER_TAB_STORAGE_LOADING_CONTEXT_H_

#include <string>
#include <vector>

#include "chrome/browser/tab/tab_storage_type.h"

namespace tabs {

// Holds the error state for storage loading.
class StorageLoadingContext {
 public:
  StorageLoadingContext();
  ~StorageLoadingContext();

  StorageLoadingContext(const StorageLoadingContext&) = delete;
  StorageLoadingContext& operator=(const StorageLoadingContext&) = delete;

  StorageLoadingContext(StorageLoadingContext&&);
  StorageLoadingContext& operator=(StorageLoadingContext&&);

  struct Warning {
    StorageLoadWarningCode status;
    std::string message;
  };

  void AddWarning(StorageLoadWarningCode status, std::string message);
  const std::vector<Warning>& warnings() const;

 private:
  std::vector<Warning> warnings_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_LOADING_CONTEXT_H_
