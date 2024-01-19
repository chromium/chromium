// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_SEA_PEN_FETCHER_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_SEA_PEN_FETCHER_H_

#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "components/manta/proto/manta.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

class MockSeaPenFetcher : public SeaPenFetcher {
 public:
  MockSeaPenFetcher();

  MockSeaPenFetcher(const MockSeaPenFetcher&) = delete;
  MockSeaPenFetcher& operator=(const MockSeaPenFetcher&) = delete;

  ~MockSeaPenFetcher() override;

  MOCK_METHOD(void,
              FetchThumbnails,
              (manta::proto::FeatureName feature_name,
               const ash::personalization_app::mojom::SeaPenQueryPtr& query,
               SeaPenFetcher::OnFetchThumbnailsComplete callback),
              (override));

  MOCK_METHOD(void,
              FetchWallpaper,
              (manta::proto::FeatureName feature_name,
               const ash::SeaPenImage& image,
               const ash::personalization_app::mojom::SeaPenQueryPtr& query,
               SeaPenFetcher::OnFetchWallpaperComplete callback),
              (override));
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_SEA_PEN_FETCHER_H_
