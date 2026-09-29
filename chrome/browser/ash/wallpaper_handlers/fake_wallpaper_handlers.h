// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_FAKE_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_FAKE_WALLPAPER_HANDLERS_H_

#include <stdint.h>

#include <string>

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"

namespace wallpaper_handlers {

// Fetcher that returns a list of backdrop image collections. Used to avoid
// network requests in unit tests.
class FakeBackdropCollectionInfoFetcher : public BackdropCollectionInfoFetcher {
 public:
  FakeBackdropCollectionInfoFetcher();

  FakeBackdropCollectionInfoFetcher(const FakeBackdropCollectionInfoFetcher&) =
      delete;
  FakeBackdropCollectionInfoFetcher& operator=(
      const FakeBackdropCollectionInfoFetcher&) = delete;

  ~FakeBackdropCollectionInfoFetcher() override;

  // BackdropCollectionInfoFetcher:
  void Start(OnCollectionsInfoFetched callback) override;
};

// Fetcher that returns a list of backdrop images. Used to avoid network
// requests in unit tests.
class FakeBackdropImageInfoFetcher : public BackdropImageInfoFetcher {
 public:
  static constexpr uint64_t kTimeOfDayUnitId = 77;

  explicit FakeBackdropImageInfoFetcher(const std::string& collection_id);

  FakeBackdropImageInfoFetcher(const FakeBackdropImageInfoFetcher&) = delete;
  FakeBackdropImageInfoFetcher& operator=(const FakeBackdropImageInfoFetcher&) =
      delete;

  ~FakeBackdropImageInfoFetcher() override;

  // BackdropImageInfoFetcher:
  void Start(OnImagesInfoFetched callback) override;

 private:
  const std::string collection_id_;
};

// Fetcher that returns a backdrop image and empty resume token. Used to avoid
// network requests in unit tests.
class FakeBackdropSurpriseMeImageFetcher
    : public BackdropSurpriseMeImageFetcher {
 public:
  explicit FakeBackdropSurpriseMeImageFetcher(const std::string& collection_id);

  FakeBackdropSurpriseMeImageFetcher(
      const FakeBackdropSurpriseMeImageFetcher&) = delete;
  FakeBackdropSurpriseMeImageFetcher& operator=(
      const FakeBackdropSurpriseMeImageFetcher&) = delete;

  ~FakeBackdropSurpriseMeImageFetcher() override;

  // BackdropSurpriseMeImageFetcher:
  void Start(OnSurpriseMeImageFetched callback) override;

 private:
  const std::string collection_id_;
  int id_incrementer_ = 0;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_FAKE_WALLPAPER_HANDLERS_H_
