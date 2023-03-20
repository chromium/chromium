// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/test_backdrop_fetcher_delegate.h"

#include <memory>
#include <string>

#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

TestBackdropFetcherDelegate::TestBackdropFetcherDelegate() = default;

TestBackdropFetcherDelegate::~TestBackdropFetcherDelegate() = default;

std::unique_ptr<BackdropCollectionInfoFetcher>
TestBackdropFetcherDelegate::CreateBackdropCollectionInfoFetcher() const {
  return std::make_unique<
      testing::NiceMock<MockBackdropCollectionInfoFetcher>>();
}

std::unique_ptr<BackdropImageInfoFetcher>
TestBackdropFetcherDelegate::CreateBackdropImageInfoFetcher(
    const std::string& collection_id) const {
  return std::make_unique<testing::NiceMock<MockBackdropImageInfoFetcher>>(
      collection_id);
}

}  // namespace wallpaper_handlers
