// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"

#include <vector>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
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
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(response)));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](absl::optional<base::Value> value) {
        return GooglePhotosAlbumsFetcher::ParseResponse(std::move(value));
      });
}

MockGooglePhotosAlbumsFetcher::~MockGooglePhotosAlbumsFetcher() = default;

MockGooglePhotosCountFetcher::MockGooglePhotosCountFetcher(Profile* profile)
    : GooglePhotosCountFetcher(profile) {
  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault([](base::OnceCallback<void(int)> callback) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), /*count=*/0));
      });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](absl::optional<base::Value> value) {
        return GooglePhotosCountFetcher::ParseResponse(std::move(value));
      });
}

MockGooglePhotosCountFetcher::~MockGooglePhotosCountFetcher() = default;

MockGooglePhotosPhotosFetcher::MockGooglePhotosPhotosFetcher(Profile* profile)
    : GooglePhotosPhotosFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse;
  using ash::personalization_app::mojom::GooglePhotosPhotoPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const absl::optional<std::string>& item_id,
             const absl::optional<std::string>& album_id,
             const absl::optional<std::string>& resume_token,
             base::OnceCallback<void(GooglePhotosPhotosCbkArgs)> callback) {
            auto response = FetchGooglePhotosPhotosResponse::New(
                std::vector<GooglePhotosPhotoPtr>(), absl::nullopt);
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(response)));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](absl::optional<base::Value> value) {
        return GooglePhotosPhotosFetcher::ParseResponse(std::move(value));
      });
}

MockGooglePhotosPhotosFetcher::~MockGooglePhotosPhotosFetcher() = default;

}  // namespace wallpaper_handlers
