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
  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const absl::optional<std::string>& resume_token,
             base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
            std::vector<ash::personalization_app::mojom::GooglePhotosAlbumPtr>
                albums;
            auto response = ash::personalization_app::mojom::
                FetchGooglePhotosAlbumsResponse::New(std::move(albums),
                                                     absl::nullopt);
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(response)));
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
}

MockGooglePhotosCountFetcher::~MockGooglePhotosCountFetcher() = default;

}  // namespace wallpaper_handlers
