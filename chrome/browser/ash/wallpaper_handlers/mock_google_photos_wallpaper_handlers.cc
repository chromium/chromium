// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/mock_google_photos_wallpaper_handlers.h"

#include <optional>
#include <string>
#include <vector>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace wallpaper_handlers {
namespace {

constexpr char kDataUrlPrefix[] = "data:image/png;base64,";

ash::personalization_app::mojom::GooglePhotosPhotoPtr
CreateFakeGooglePhotosPhoto(const std::string& id) {
  auto photo = ash::personalization_app::mojom::GooglePhotosPhoto::New();
  photo->id = id;
  photo->name = id;
  photo->dedup_key = id;
  photo->url = GURL(kDataUrlPrefix + id);
  return photo;
}

ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr
CreateFakeGooglePhotosPhotosResponse(
    const std::optional<std::string>& item_id) {
  auto response =
      ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse::New();
  std::vector<ash::personalization_app::mojom::GooglePhotosPhotoPtr> photos;
  if (item_id.has_value()) {
    // Request for a specific photo with matching id.
    photos.push_back(CreateFakeGooglePhotosPhoto(item_id.value()));
  } else {
    // Request for list of photos.
    for (auto i = 0; i < 3; i++) {
      auto photo = ash::personalization_app::mojom::GooglePhotosPhoto::New();
      std::string id = base::StringPrintf("fake_google_photos_photo_id_%i", i);
      photo->id = id;
      photo->name = id;
      photo->dedup_key = id;
      photo->url = GURL(kDataUrlPrefix + base::NumberToString(i));
      photos.push_back(std::move(photo));
    }
  }

  response->photos = std::move(photos);
  response->resume_token = std::nullopt;
  return response;
}

ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponsePtr
CreateFakeGooglePhotosSharedAlbumsResponse() {
  std::vector<ash::personalization_app::mojom::GooglePhotosAlbumPtr> result;
  for (int i = 0; i < 3; i++) {
    auto album = ash::personalization_app::mojom::GooglePhotosAlbum::New();
    std::string id =
        base::StringPrintf("fake_google_photos_shared_album_id_%i", i);
    album->id = id;
    album->is_shared = true;
    // Shared albums always have `photo_count == 0` due to technical debt on
    // server side.
    album->photo_count = 0;
    album->preview = GURL(kDataUrlPrefix + base::NumberToString(i));
    album->timestamp = base::Time::Now();
    album->title = id;
    result.push_back(std::move(album));
  }
  return ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse::New(
      std::move(result), std::nullopt);
}

}  // namespace

MockGooglePhotosAlbumsFetcher::MockGooglePhotosAlbumsFetcher(Profile* profile)
    : GooglePhotosAlbumsFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
  using ash::personalization_app::mojom::GooglePhotosAlbumPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const std::optional<std::string>& resume_token,
             base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
            auto response = FetchGooglePhotosAlbumsResponse::New(
                std::vector<GooglePhotosAlbumPtr>(), std::nullopt);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(response)));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosAlbumsFetcher::ParseResponse(response);
      });
}

MockGooglePhotosAlbumsFetcher::~MockGooglePhotosAlbumsFetcher() = default;

std::optional<size_t> MockGooglePhotosAlbumsFetcher::GetResultCount(
    const GooglePhotosAlbumsCbkArgs& result) {
  return GooglePhotosAlbumsFetcher::GetResultCount(result);
}

MockGooglePhotosSharedAlbumsFetcher::MockGooglePhotosSharedAlbumsFetcher(
    Profile* profile)
    : GooglePhotosSharedAlbumsFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
  using ash::personalization_app::mojom::GooglePhotosAlbumPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const std::optional<std::string>& resume_token,
             base::OnceCallback<void(ash::personalization_app::mojom::
                                         FetchGooglePhotosAlbumsResponsePtr)>
                 callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               CreateFakeGooglePhotosSharedAlbumsResponse()));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosSharedAlbumsFetcher::ParseResponse(response);
      });
}

MockGooglePhotosSharedAlbumsFetcher::~MockGooglePhotosSharedAlbumsFetcher() =
    default;

std::optional<size_t> MockGooglePhotosSharedAlbumsFetcher::GetResultCount(
    const GooglePhotosAlbumsCbkArgs& result) {
  return GooglePhotosSharedAlbumsFetcher::GetResultCount(result);
}

MockGooglePhotosEnabledFetcher::MockGooglePhotosEnabledFetcher(Profile* profile)
    : GooglePhotosEnabledFetcher(profile) {
  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](base::OnceCallback<void(GooglePhotosEnablementState)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               GooglePhotosEnablementState::kEnabled));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosEnabledFetcher::ParseResponse(response);
      });
}

MockGooglePhotosEnabledFetcher::~MockGooglePhotosEnabledFetcher() = default;

std::optional<size_t> MockGooglePhotosEnabledFetcher::GetResultCount(
    const GooglePhotosEnablementState& result) {
  return GooglePhotosEnabledFetcher::GetResultCount(result);
}

MockGooglePhotosPhotosFetcher::MockGooglePhotosPhotosFetcher(Profile* profile)
    : GooglePhotosPhotosFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse;
  using ash::personalization_app::mojom::GooglePhotosPhotoPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const std::optional<std::string>& item_id,
             const std::optional<std::string>& album_id,
             const std::optional<std::string>& resume_token, bool shuffle,
             base::OnceCallback<void(GooglePhotosPhotosCbkArgs)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               CreateFakeGooglePhotosPhotosResponse(item_id)));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosPhotosFetcher::ParseResponse(response);
      });
}

MockGooglePhotosPhotosFetcher::~MockGooglePhotosPhotosFetcher() = default;

std::optional<size_t> MockGooglePhotosPhotosFetcher::GetResultCount(
    const GooglePhotosPhotosCbkArgs& result) {
  return GooglePhotosPhotosFetcher::GetResultCount(result);
}
}  // namespace wallpaper_handlers
