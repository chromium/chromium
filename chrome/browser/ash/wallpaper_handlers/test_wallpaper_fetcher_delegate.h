// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_TEST_WALLPAPER_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_TEST_WALLPAPER_FETCHER_DELEGATE_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"

namespace wallpaper_handlers {

class BackdropCollectionInfoFetcher;
class BackdropImageInfoFetcher;
class BackdropSurpriseMeImageFetcher;
class SeaPenFetcher;

class TestWallpaperFetcherDelegate : public WallpaperFetcherDelegate {
 public:
  TestWallpaperFetcherDelegate();

  TestWallpaperFetcherDelegate(const TestWallpaperFetcherDelegate&) = delete;
  TestWallpaperFetcherDelegate& operator=(const TestWallpaperFetcherDelegate&) =
      delete;

  ~TestWallpaperFetcherDelegate() override;

  // WallpaperFetcherDelegate:
  std::unique_ptr<BackdropCollectionInfoFetcher>
  CreateBackdropCollectionInfoFetcher() const override;
  std::unique_ptr<BackdropImageInfoFetcher> CreateBackdropImageInfoFetcher(
      const std::string& collection_id) const override;
  std::unique_ptr<BackdropSurpriseMeImageFetcher>
  CreateBackdropSurpriseMeImageFetcher(
      const std::string& collection_id) const override;
  std::unique_ptr<GooglePhotosAlbumsFetcher> CreateGooglePhotosAlbumsFetcher(
      Profile* profile) const override;
  std::unique_ptr<GooglePhotosSharedAlbumsFetcher>
  CreateGooglePhotosSharedAlbumsFetcher(Profile* profile) const override;
  std::unique_ptr<GooglePhotosEnabledFetcher> CreateGooglePhotosEnabledFetcher(
      Profile* profile) const override;
  std::unique_ptr<GooglePhotosPhotosFetcher> CreateGooglePhotosPhotosFetcher(
      Profile* profile) const override;
  void FetchGooglePhotosAccessToken(
      const AccountId& account_id,
      ash::WallpaperControllerClient::FetchGooglePhotosAccessTokenCallback
          callback) const override;
  std::unique_ptr<SeaPenFetcher> CreateSeaPenFetcher(
      Profile* profile) const override;
};

}  // namespace wallpaper_handlers
#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_TEST_WALLPAPER_FETCHER_DELEGATE_H_
