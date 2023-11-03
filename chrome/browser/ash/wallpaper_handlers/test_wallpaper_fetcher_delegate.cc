// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"

#include <memory>
#include <string>

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/wallpaper_handlers/mock_sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "chrome/browser/profiles/profile.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

TestWallpaperFetcherDelegate::TestWallpaperFetcherDelegate() = default;

TestWallpaperFetcherDelegate::~TestWallpaperFetcherDelegate() = default;

std::unique_ptr<BackdropCollectionInfoFetcher>
TestWallpaperFetcherDelegate::CreateBackdropCollectionInfoFetcher() const {
  return std::make_unique<
      testing::NiceMock<MockBackdropCollectionInfoFetcher>>();
}

std::unique_ptr<BackdropImageInfoFetcher>
TestWallpaperFetcherDelegate::CreateBackdropImageInfoFetcher(
    const std::string& collection_id) const {
  return std::make_unique<testing::NiceMock<MockBackdropImageInfoFetcher>>(
      collection_id);
}

std::unique_ptr<BackdropSurpriseMeImageFetcher>
TestWallpaperFetcherDelegate::CreateBackdropSurpriseMeImageFetcher(
    const std::string& collection_id) const {
  return std::make_unique<
      testing::NiceMock<MockBackdropSurpriseMeImageFetcher>>(collection_id);
}

std::unique_ptr<GooglePhotosAlbumsFetcher>
TestWallpaperFetcherDelegate::CreateGooglePhotosAlbumsFetcher(
    Profile* profile) const {
  return std::make_unique<testing::NiceMock<MockGooglePhotosAlbumsFetcher>>(
      profile);
}

std::unique_ptr<GooglePhotosSharedAlbumsFetcher>
TestWallpaperFetcherDelegate::CreateGooglePhotosSharedAlbumsFetcher(
    Profile* profile) const {
  return std::make_unique<
      testing::NiceMock<MockGooglePhotosSharedAlbumsFetcher>>(profile);
}

std::unique_ptr<GooglePhotosEnabledFetcher>
TestWallpaperFetcherDelegate::CreateGooglePhotosEnabledFetcher(
    Profile* profile) const {
  return std::make_unique<testing::NiceMock<MockGooglePhotosEnabledFetcher>>(
      profile);
}

std::unique_ptr<GooglePhotosPhotosFetcher>
TestWallpaperFetcherDelegate::CreateGooglePhotosPhotosFetcher(
    Profile* profile) const {
  return std::make_unique<testing::NiceMock<MockGooglePhotosPhotosFetcher>>(
      profile);
}

void TestWallpaperFetcherDelegate::FetchGooglePhotosAccessToken(
    const AccountId& account_id,
    ash::WallpaperControllerClient::FetchGooglePhotosAccessTokenCallback
        callback) const {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), "fake_google_photos_access_token"));
}

std::unique_ptr<SeaPenFetcher>
TestWallpaperFetcherDelegate::CreateSeaPenFetcher(Profile* profile) const {
  return std::make_unique<testing::NiceMock<MockSeaPenFetcher>>();
}

}  // namespace wallpaper_handlers
