// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_loading_context.h"

#include <string>
#include <utility>

namespace tabs {

StorageLoadingContext::StorageLoadingContext() = default;
StorageLoadingContext::~StorageLoadingContext() = default;

StorageLoadingContext::StorageLoadingContext(StorageLoadingContext&&) = default;
StorageLoadingContext& StorageLoadingContext::operator=(
    StorageLoadingContext&&) = default;

void StorageLoadingContext::SetStatus(StorageLoadingStatus status,
                                      std::string message) {
  if (status_ == StorageLoadingStatus::kSuccess) {
    status_ = status;
    error_message_ = std::move(message);
  }
}

bool StorageLoadingContext::HasError() const {
  return status_ != StorageLoadingStatus::kSuccess;
}

StorageLoadingStatus StorageLoadingContext::status() const {
  return status_;
}

const std::optional<std::string>& StorageLoadingContext::error_message() const {
  return error_message_;
}

}  // namespace tabs
