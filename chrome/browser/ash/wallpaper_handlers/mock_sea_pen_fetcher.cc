// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/mock_sea_pen_fetcher.h"

#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "components/manta/proto/manta.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

namespace {

std::vector<ash::SeaPenImage> MakeFakeImageResults(const std::string& query) {
  std::vector<ash::SeaPenImage> image_results;
  for (uint32_t i = 1; i < 5; i++) {
    image_results.emplace_back(base::StringPrintf("fake_sea_pen_image_%d", i),
                               i, query,
                               manta::proto::ImageResolution::RESOLUTION_1024);
  }
  return image_results;
}

}  // namespace

MockSeaPenFetcher::MockSeaPenFetcher() {
  ON_CALL(*this, Start)
      .WillByDefault(
          [](const std::string& text, OnWallpaperSearchComplete callback) {
            DVLOG(2) << __PRETTY_FUNCTION__ << " text=" << text;
            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE, base::BindOnce(&MakeFakeImageResults, text),
                std::move(callback));
          });
}

MockSeaPenFetcher::~MockSeaPenFetcher() = default;

}  // namespace wallpaper_handlers
