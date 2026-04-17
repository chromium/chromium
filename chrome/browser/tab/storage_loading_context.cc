// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_loading_context.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"

namespace tabs {

StorageLoadingContext::StorageLoadingContext() = default;
StorageLoadingContext::~StorageLoadingContext() = default;

StorageLoadingContext::StorageLoadingContext(StorageLoadingContext&&) = default;
StorageLoadingContext& StorageLoadingContext::operator=(
    StorageLoadingContext&&) = default;

void StorageLoadingContext::AddWarning(StorageLoadWarningCode status,
                                       std::string message) {
  UMA_HISTOGRAM_ENUMERATION("Tabs.TabStateStore.LoadWarning", status,
                            StorageLoadWarningCode::kMaxValue);
  warnings_.push_back({status, std::move(message)});
}

const std::vector<StorageLoadingContext::Warning>&
StorageLoadingContext::warnings() const {
  return warnings_;
}

}  // namespace tabs
