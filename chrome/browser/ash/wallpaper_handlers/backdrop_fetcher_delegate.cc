// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/backdrop_fetcher_delegate.h"

#include <memory>
#include <string>

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "third_party/abseil-cpp/absl/memory/memory.h"

namespace wallpaper_handlers {

BackdropFetcherDelegateImpl::BackdropFetcherDelegateImpl() = default;

BackdropFetcherDelegateImpl::~BackdropFetcherDelegateImpl() = default;

std::unique_ptr<BackdropCollectionInfoFetcher>
BackdropFetcherDelegateImpl::CreateBackdropCollectionInfoFetcher() const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(new BackdropCollectionInfoFetcher());
}

std::unique_ptr<BackdropImageInfoFetcher>
BackdropFetcherDelegateImpl::CreateBackdropImageInfoFetcher(
    const std::string& collection_id) const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(new BackdropImageInfoFetcher(collection_id));
}

}  // namespace wallpaper_handlers
