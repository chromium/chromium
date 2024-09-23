// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/manta/manta_service.h"
#include "components/manta/snapper_provider.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
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

std::unique_ptr<BackdropSurpriseMeImageFetcher>
WallpaperFetcherDelegateImpl::CreateBackdropSurpriseMeImageFetcher(
    const std::string& collection_id) const {
  // Use `WrapUnique` to access the protected constructor.
  return absl::WrapUnique(
      new BackdropSurpriseMeImageFetcher(collection_id, /*resume_token=*/""));
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

void WallpaperFetcherDelegateImpl::FetchGooglePhotosAccessToken(
    const AccountId& account_id,
    ash::WallpaperControllerClient::FetchGooglePhotosAccessTokenCallback
        callback) const {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  auto fetcher = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "wallpaper_fetcher_delegate",
      IdentityManagerFactory::GetForProfile(profile),
      signin::ScopeSet({GaiaConstants::kPhotosModuleImageOAuth2Scope}),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSignin);
  auto* fetcher_ptr = fetcher.get();
  fetcher_ptr->Start(base::BindOnce(
      [](
          // Fetcher is moved into lambda to keep it alive until network
          // request completes.
          std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>,
          ash::WallpaperControllerClient::FetchGooglePhotosAccessTokenCallback
              callback,
          GoogleServiceAuthError error,
          signin::AccessTokenInfo access_token_info) {
        if (error.state() != GoogleServiceAuthError::NONE) {
          LOG(ERROR)
              << "Failed to fetch auth token to download Google Photos photo:"
              << error.error_message();
          std::move(callback).Run(std::nullopt);
          return;
        }
        std::move(callback).Run(access_token_info.token);
      },
      std::move(fetcher), std::move(callback)));
}

std::unique_ptr<SeaPenFetcher>
WallpaperFetcherDelegateImpl::CreateSeaPenFetcher(Profile* profile) const {
  std::unique_ptr<manta::SnapperProvider> snapper_provider;
  auto* manta_service = manta::MantaServiceFactory::GetForProfile(profile);
  if (manta_service) {
    snapper_provider = manta_service->CreateSnapperProvider();
  }
  return SeaPenFetcher::MakeSeaPenFetcher(std::move(snapper_provider));
}

}  // namespace wallpaper_handlers
