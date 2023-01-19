// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"

#include <vector>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

MockGooglePhotosAlbumsFetcher::MockGooglePhotosAlbumsFetcher(Profile* profile)
    : GooglePhotosAlbumsFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
  using ash::personalization_app::mojom::GooglePhotosAlbumPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const absl::optional<std::string>& resume_token,
             base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
            auto response = FetchGooglePhotosAlbumsResponse::New(
                std::vector<GooglePhotosAlbumPtr>(), absl::nullopt);
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

absl::optional<size_t> MockGooglePhotosAlbumsFetcher::GetResultCount(
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
          [](const absl::optional<std::string>& resume_token,
             base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
            auto response = FetchGooglePhotosAlbumsResponse::New(
                std::vector<GooglePhotosAlbumPtr>(), absl::nullopt);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(response)));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosSharedAlbumsFetcher::ParseResponse(response);
      });
}

MockGooglePhotosSharedAlbumsFetcher::~MockGooglePhotosSharedAlbumsFetcher() =
    default;

absl::optional<size_t> MockGooglePhotosSharedAlbumsFetcher::GetResultCount(
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

absl::optional<size_t> MockGooglePhotosEnabledFetcher::GetResultCount(
    const GooglePhotosEnablementState& result) {
  return GooglePhotosEnabledFetcher::GetResultCount(result);
}

MockGooglePhotosPhotosFetcher::MockGooglePhotosPhotosFetcher(Profile* profile)
    : GooglePhotosPhotosFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse;
  using ash::personalization_app::mojom::GooglePhotosPhotoPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const absl::optional<std::string>& item_id,
             const absl::optional<std::string>& album_id,
             const absl::optional<std::string>& resume_token, bool shuffle,
             base::OnceCallback<void(GooglePhotosPhotosCbkArgs)> callback) {
            auto response = FetchGooglePhotosPhotosResponse::New(
                std::vector<GooglePhotosPhotoPtr>(), absl::nullopt);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(response)));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosPhotosFetcher::ParseResponse(response);
      });
}

MockGooglePhotosPhotosFetcher::~MockGooglePhotosPhotosFetcher() = default;

absl::optional<size_t> MockGooglePhotosPhotosFetcher::GetResultCount(
    const GooglePhotosPhotosCbkArgs& result) {
  return GooglePhotosPhotosFetcher::GetResultCount(result);
}

}  // namespace wallpaper_handlers
