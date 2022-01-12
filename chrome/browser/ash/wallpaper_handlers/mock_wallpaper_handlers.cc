// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"

#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

MockGooglePhotosCountFetcher::MockGooglePhotosCountFetcher(Profile* profile)
    : GooglePhotosCountFetcher(profile) {
  ON_CALL(*this, AddCallbackAndStartIfNecessary)
      .WillByDefault([](base::OnceCallback<void(int)> callback) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), /*count=*/0));
      });
}

MockGooglePhotosCountFetcher::~MockGooglePhotosCountFetcher() = default;

}  // namespace wallpaper_handlers
