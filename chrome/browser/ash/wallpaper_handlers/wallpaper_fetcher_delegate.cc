// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"

#include <memory>
#include <string>

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/abseil-cpp/absl/memory/memory.h"

namespace wallpaper_handlers {

WallpaperFetcherDelegateImpl::WallpaperFetcherDelegateImpl() = default;

WallpaperFetcherDelegateImpl::~WallpaperFetcherDelegateImpl() = default;

std::unique_ptr<BackdropCollectionInfoFetcher>
WallpaperFetcherDelegateImpl::CreateBackdropCollectionInfoFetcher() const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(new BackdropCollectionInfoFetcher());
}

std::unique_ptr<BackdropImageInfoFetcher>
WallpaperFetcherDelegateImpl::CreateBackdropImageInfoFetcher(
    const std::string& collection_id) const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(new BackdropImageInfoFetcher(collection_id));
}

std::unique_ptr<GooglePhotosAlbumsFetcher>
WallpaperFetcherDelegateImpl::CreateGooglePhotosAlbumsFetcher(
    Profile* profile) const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(new GooglePhotosAlbumsFetcher(profile));
}

std::unique_ptr<GooglePhotosSharedAlbumsFetcher>
WallpaperFetcherDelegateImpl::CreateGooglePhotosSharedAlbumsFetcher(
    Profile* profile) const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(new GooglePhotosSharedAlbumsFetcher(profile));
}

std::unique_ptr<GooglePhotosEnabledFetcher>
WallpaperFetcherDelegateImpl::CreateGooglePhotosEnabledFetcher(
    Profile* profile) const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(new GooglePhotosEnabledFetcher(profile));
}

std::unique_ptr<GooglePhotosPhotosFetcher>
WallpaperFetcherDelegateImpl::CreateGooglePhotosPhotosFetcher(
    Profile* profile) const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(new GooglePhotosPhotosFetcher(profile));
}

}  // namespace wallpaper_handlers
