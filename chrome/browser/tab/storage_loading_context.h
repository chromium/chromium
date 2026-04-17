// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_LOADING_CONTEXT_H_
#define CHROME_BROWSER_TAB_STORAGE_LOADING_CONTEXT_H_

#include <optional>
#include <string>

#include "chrome/browser/tab/tab_storage_type.h"

namespace tabs {

// Holds the error state for storage loading. Will be successful, unless
// marked otherwise.
class StorageLoadingContext {
 public:
  StorageLoadingContext();
  ~StorageLoadingContext();

  StorageLoadingContext(const StorageLoadingContext&) = delete;
  StorageLoadingContext& operator=(const StorageLoadingContext&) = delete;

  StorageLoadingContext(StorageLoadingContext&&);
  StorageLoadingContext& operator=(StorageLoadingContext&&);

  void SetStatus(StorageLoadingStatus status, std::string message);
  bool HasError() const;

  StorageLoadingStatus status() const;
  const std::optional<std::string>& error_message() const;

 private:
  StorageLoadingStatus status_ = StorageLoadingStatus::kSuccess;
  std::optional<std::string> error_message_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_LOADING_CONTEXT_H_
