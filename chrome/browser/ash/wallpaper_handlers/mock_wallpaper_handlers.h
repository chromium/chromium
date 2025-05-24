// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_WALLPAPER_HANDLERS_H_

#include <stdint.h>

#include <string>

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

// Fetcher that returns a list of backdrop image collections. Used to avoid
// network requests in unit tests.
class MockBackdropCollectionInfoFetcher : public BackdropCollectionInfoFetcher {
 public:
  MockBackdropCollectionInfoFetcher();

  MockBackdropCollectionInfoFetcher(const MockBackdropCollectionInfoFetcher&) =
      delete;
  MockBackdropCollectionInfoFetcher& operator=(
      const MockBackdropCollectionInfoFetcher&) = delete;

  ~MockBackdropCollectionInfoFetcher() override;

  MOCK_METHOD(void, Start, (OnCollectionsInfoFetched callback), (override));
};

// Fetcher that returns a list of backdrop images. Used to avoid network
// requests in unit tests.
class MockBackdropImageInfoFetcher : public BackdropImageInfoFetcher {
 public:
  static constexpr uint64_t kTimeOfDayUnitId = 77;

  explicit MockBackdropImageInfoFetcher(const std::string& collection_id);

  MockBackdropImageInfoFetcher(const MockBackdropImageInfoFetcher&) = delete;
  MockBackdropImageInfoFetcher& operator=(const MockBackdropImageInfoFetcher&) =
      delete;

  ~MockBackdropImageInfoFetcher() override;

  MOCK_METHOD(void, Start, (OnImagesInfoFetched callback), (override));

 private:
  std::string collection_id_;
};

// Fetcher that returns a backdrop image and empty resume token. Used to avoid
// network requests in unit tests.
class MockBackdropSurpriseMeImageFetcher
    : public BackdropSurpriseMeImageFetcher {
 public:
  explicit MockBackdropSurpriseMeImageFetcher(const std::string& collection_id);

  MockBackdropSurpriseMeImageFetcher(
      const MockBackdropSurpriseMeImageFetcher&) = delete;
  MockBackdropSurpriseMeImageFetcher& operator=(
      const MockBackdropSurpriseMeImageFetcher&) = delete;

  ~MockBackdropSurpriseMeImageFetcher() override;

  MOCK_METHOD(void, Start, (OnSurpriseMeImageFetched callback), (override));

 private:
  std::string collection_id_;
  int id_incrementer_ = 0;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_WALLPAPER_HANDLERS_H_
