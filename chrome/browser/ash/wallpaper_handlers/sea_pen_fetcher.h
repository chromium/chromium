// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom.h"
#include "base/functional/callback_forward.h"
#include "components/manta/proto/manta.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace wallpaper_handlers {

class SeaPenFetcher {
 public:
  using OnFetchThumbnailsComplete = base::OnceCallback<void(
      absl::optional<std::vector<ash::SeaPenImage>> images)>;
  using OnFetchWallpaperComplete =
      base::OnceCallback<void(absl::optional<ash::SeaPenImage> image)>;

  SeaPenFetcher();

  SeaPenFetcher(const SeaPenFetcher&) = delete;
  SeaPenFetcher& operator=(const SeaPenFetcher&) = delete;

  virtual ~SeaPenFetcher();

  // Run `query` against the Manta API. `query` is required to be a valid UTF-8
  // string no longer than `kMaximumSearchWallpaperTextBytes`.
  virtual void FetchThumbnails(
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      OnFetchThumbnailsComplete callback) = 0;

  // Calls the Manta API to fetch a higher resolution image of the thumbnail.
  virtual void FetchWallpaper(const ash::SeaPenImage& thumbnail,
                              OnFetchWallpaperComplete callback) = 0;

 private:
  // Allow delegate to view the constructor function.
  friend class WallpaperFetcherDelegateImpl;

  // Private forces creation via `WallpaperFetcherDelegate` to set up mocking
  // in test code.
  static std::unique_ptr<SeaPenFetcher> MakeSeaPenFetcher(Profile* profile);
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_
