// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/online_wallpaper_variant_info_fetcher.h"

#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_metrics_manager.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_online_variant_utils.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"

namespace ash {
namespace {

OnlineWallpaperVariantInfoFetcher* g_instance = nullptr;

// The filtered results from a set of backdrop::Images for a given |location|
// and |unit_id| value.
class VariantMatches {
 public:
  VariantMatches(VariantMatches&&) = default;

  VariantMatches(const VariantMatches&) = delete;
  VariantMatches& operator=(const VariantMatches&) = delete;

  ~VariantMatches() = default;

  // Filters |images| to only the entries that match |location|.
  static std::optional<VariantMatches> FromImages(
      const std::string& location,
      const std::vector<backdrop::Image>& images) {
    // Find the exact image in the |images| collection.
    auto image_iter =
        base::ranges::find(images, location, &backdrop::Image::image_url);

    if (image_iter == images.end()) {
      return std::nullopt;
    }

    uint64_t unit_id = image_iter->unit_id();
    return FromImages(unit_id, images);
  }

  // Same semantic as the method above but instead of matching against
  // `location`, `unit_id` is used instead.
  static std::optional<VariantMatches> FromImages(
      uint64_t unit_id,
      const std::vector<backdrop::Image>& images) {
    std::vector<OnlineWallpaperVariant> variants;
    for (const auto& image : images) {
      if (image.unit_id() == unit_id) {
        variants.emplace_back(image.asset_id(), GURL(image.image_url()),
                              image.has_image_type()
                                  ? image.image_type()
                                  : backdrop::Image::IMAGE_TYPE_UNKNOWN);
      }
    }
    if (variants.empty()) {
      return std::nullopt;
    }
    return VariantMatches(unit_id, std::move(variants));
  }

  // The unit id of the Variant that matched |location|.
  const uint64_t unit_id;

  // The set of images that are appropriate for |location|.
  const std::vector<OnlineWallpaperVariant> variants;

 private:
  VariantMatches(uint64_t unit_id_in,
                 std::vector<OnlineWallpaperVariant>&& variants_in)
      : unit_id(unit_id_in), variants(variants_in) {}
};

bool IsDaily(const WallpaperInfo& info) {
  return info.type == WallpaperType::kDaily;
}

}  // namespace

OnlineWallpaperVariantInfoFetcher::OnlineWallpaperRequest::
    OnlineWallpaperRequest(const AccountId& account_id_in,
                           const std::string& collection_id_in,
                           WallpaperLayout layout_in,
                           bool daily_refresh_enabled_in)
    : account_id(account_id_in),
      collection_id(collection_id_in),
      layout(layout_in),
      daily_refresh_enabled(daily_refresh_enabled_in) {}

OnlineWallpaperVariantInfoFetcher::OnlineWallpaperRequest::
    ~OnlineWallpaperRequest() = default;

OnlineWallpaperVariantInfoFetcher::OnlineWallpaperVariantInfoFetcher() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

OnlineWallpaperVariantInfoFetcher::~OnlineWallpaperVariantInfoFetcher() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
OnlineWallpaperVariantInfoFetcher*
OnlineWallpaperVariantInfoFetcher::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

void OnlineWallpaperVariantInfoFetcher::SetClient(
    WallpaperControllerClient* client) {
  wallpaper_controller_client_ = client;
}

void OnlineWallpaperVariantInfoFetcher::FetchOnlineWallpaper(
    const AccountId& account_id,
    const WallpaperInfo& info,
    FetchParamsCallback callback) {
  DCHECK(IsOnlineWallpaper(info.type));

  DCHECK(wallpaper_controller_client_);

  if (info.unit_id.has_value() && !info.variants.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  OnlineWallpaperParams{
                                      account_id, info.collection_id,
                                      info.layout, /*preview_mode=*/false,
                                      /*from_user=*/false, IsDaily(info),
                                      info.unit_id.value(), info.variants}));
    return;
  }

  // For requests from existing WallpaperInfo, location should always be
  // populated. In the event of very old wallpapers, treat them as failure.
  if (info.location.empty()) {
    LOG(WARNING)
        << "Failed to determine wallpaper url. This should only happen for "
           "very old wallpapers.";
    base::UmaHistogramEnumeration(
        WallpaperMetricsManager::ToResultHistogram(WallpaperType::kOnline),
        SetWallpaperResult::kInvalidState);
    std::move(callback).Run(std::nullopt);
    return;
  }

  bool daily = IsDaily(info);
  auto request = std::make_unique<OnlineWallpaperRequest>(
      account_id, info.collection_id, info.layout, daily);

  auto collection_id = request->collection_id;
  wallpaper_controller_client_->FetchImagesForCollection(
      collection_id,
      base::BindOnce(
          &OnlineWallpaperVariantInfoFetcher::FindAndSetOnlineWallpaperVariants,
          weak_factory_.GetWeakPtr(), std::move(request), info.location,
          std::move(callback)));
}

bool OnlineWallpaperVariantInfoFetcher::FetchDailyWallpaper(
    const AccountId& account_id,
    const WallpaperInfo& info,
    FetchParamsCallback callback) {
  DCHECK(IsDaily(info));

  // We might not have a client yet.
  if (!wallpaper_controller_client_) {
    return false;
  }

  bool daily = true;  // This is always a a daily wallpaper.
  auto request = std::make_unique<OnlineWallpaperRequest>(
      account_id, info.collection_id, info.layout, daily);
  wallpaper_controller_client_->FetchDailyRefreshWallpaper(
      info.collection_id,
      base::BindOnce(&OnlineWallpaperVariantInfoFetcher::OnSingleFetch,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
  return true;
}

void OnlineWallpaperVariantInfoFetcher::FetchTimeOfDayWallpaper(
    const AccountId& account_id,
    uint64_t unit_id,
    FetchParamsCallback callback) {
  auto request = std::make_unique<OnlineWallpaperRequest>(
      account_id, wallpaper_constants::kTimeOfDayWallpaperCollectionId,
      WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
      /*daily_refresh_enabled=*/false);

  wallpaper_controller_client_->FetchImagesForCollection(
      wallpaper_constants::kTimeOfDayWallpaperCollectionId,
      base::BindOnce(
          &OnlineWallpaperVariantInfoFetcher::OnTimeOfDayWallpapersFetched,
          weak_factory_.GetWeakPtr(), std::move(request), unit_id,
          std::move(callback)));
}

void OnlineWallpaperVariantInfoFetcher::OnSingleFetch(
    std::unique_ptr<OnlineWallpaperRequest> request,
    FetchParamsCallback callback,
    bool success,
    const backdrop::Image& image) {
  if (!success) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // If wallpaper_controller_client_ is null, we're shutting down.
  if (!wallpaper_controller_client_)
    return;

  auto collection_id = request->collection_id;
  wallpaper_controller_client_->FetchImagesForCollection(
      collection_id,
      base::BindOnce(
          &OnlineWallpaperVariantInfoFetcher::FindAndSetOnlineWallpaperVariants,
          weak_factory_.GetWeakPtr(), std::move(request), image.image_url(),
          std::move(callback)));
}

void OnlineWallpaperVariantInfoFetcher::FindAndSetOnlineWallpaperVariants(
    std::unique_ptr<OnlineWallpaperRequest> request,
    const std::string& location,
    FetchParamsCallback callback,
    bool success,
    const std::vector<backdrop::Image>& images) {
  if (!success) {
    LOG(WARNING) << "Failed to fetch online wallpapers";
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<VariantMatches> matches =
      VariantMatches::FromImages(location, images);
  if (!matches) {
    LOG(ERROR) << "No valid variants";
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(ash::OnlineWallpaperParams{
      request->account_id, request->collection_id, request->layout,
      /*preview_mode=*/false, /*from_user=*/false,
      request->daily_refresh_enabled, matches->unit_id, matches->variants});
}

void OnlineWallpaperVariantInfoFetcher::OnTimeOfDayWallpapersFetched(
    std::unique_ptr<OnlineWallpaperRequest> request,
    uint64_t unit_id,
    FetchParamsCallback callback,
    bool success,
    const std::vector<backdrop::Image>& images) {
  if (!success) {
    LOG(WARNING) << "Failed to fetch online wallpapers";
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<VariantMatches> matches =
      VariantMatches::FromImages(unit_id, images);
  if (!matches) {
    LOG(ERROR) << "No valid variants";
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(ash::OnlineWallpaperParams{
      request->account_id, request->collection_id, request->layout,
      /*preview_mode=*/false, /*from_user=*/false,
      request->daily_refresh_enabled, matches->unit_id, matches->variants});
}

}  // namespace ash
