// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_MOCK_SEA_PEN_FETCHER_H_
#define ASH_WALLPAPER_MOCK_SEA_PEN_FETCHER_H_

#include "ash/wallpaper/sea_pen_fetcher.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "components/manta/proto/manta.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockSeaPenFetcher : public SeaPenFetcher {
 public:
  MockSeaPenFetcher();

  MockSeaPenFetcher(const MockSeaPenFetcher&) = delete;
  MockSeaPenFetcher& operator=(const MockSeaPenFetcher&) = delete;

  ~MockSeaPenFetcher() override;

  MOCK_METHOD(void,
              FetchThumbnails,
              (manta::proto::FeatureName feature_name,
               const personalization_app::mojom::SeaPenQueryPtr& query,
               SeaPenFetcher::OnFetchThumbnailsComplete callback),
              (override));

  MOCK_METHOD(void,
              FetchWallpaper,
              (manta::proto::FeatureName feature_name,
               const SeaPenImage& image,
               const personalization_app::mojom::SeaPenQueryPtr& query,
               SeaPenFetcher::OnFetchWallpaperComplete callback),
              (override));
};

}  // namespace ash

#endif  // ASH_WALLPAPER_MOCK_SEA_PEN_FETCHER_H_
