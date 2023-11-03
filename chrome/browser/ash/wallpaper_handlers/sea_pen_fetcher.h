// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace manta {

class SnapperProvider;

namespace proto {
class Response;
}  // namespace proto

}  // namespace manta

namespace wallpaper_handlers {

class SeaPenFetcher {
 public:
  using OnWallpaperSearchComplete = base::OnceCallback<void(
      const absl::optional<std::vector<ash::SeaPenImage>>& images)>;

  SeaPenFetcher(const SeaPenFetcher&) = delete;
  SeaPenFetcher& operator=(const SeaPenFetcher&) = delete;

  virtual ~SeaPenFetcher();

  // Run `query` against the Manta API. `query` is required to be a valid UTF-8
  // string no longer than `kMaximumSearchWallpaperTextBytes`.
  virtual void Start(const std::string& query,
                     OnWallpaperSearchComplete callback);

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  explicit SeaPenFetcher(Profile* profile);

 private:
  // Allow delegate to view the constructor.
  friend class WallpaperFetcherDelegateImpl;

  void OnSnapperDone(std::unique_ptr<manta::proto::Response> response,
                     manta::MantaStatus status);

  OnWallpaperSearchComplete pending_callback_;
  std::unique_ptr<manta::SnapperProvider> snapper_provider_;
  base::WeakPtrFactory<SeaPenFetcher> weak_ptr_factory_{this};
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_SEA_PEN_FETCHER_H_
