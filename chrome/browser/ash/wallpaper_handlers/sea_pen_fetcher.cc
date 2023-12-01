// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-shared.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/snapper_provider.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Uncomment below to enable a fake API for local debugging purposes.
// #define FAKE_SEA_PEN_FETCHER_FOR_DEBUG

#if defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

static_assert(DCHECK_IS_ON(),
              "FakeSeaPenFetcher only allowed in DCHECK builds");

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/cxx23_to_underlying.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"

#endif  // defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

namespace wallpaper_handlers {

namespace {

#if defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

std::string MakeFakeJpgData() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(512, 512);
  bitmap.eraseColor(SkColorSetARGB(base::RandInt(0, 255), base::RandInt(0, 255),
                                   base::RandInt(0, 255),
                                   base::RandInt(0, 255)));
  std::vector<unsigned char> encoded_data;
  CHECK(gfx::JPEGCodec::Encode(bitmap, /*quality=*/10, &encoded_data));
  return std::string(encoded_data.begin(), encoded_data.end());
}

std::vector<ash::SeaPenImage> MakeFakeSeaPenImages(const std::string& query) {
  std::vector<ash::SeaPenImage> result;
  for (int i = 0; i < base::RandInt(0, 6); i++) {
    result.emplace_back(MakeFakeJpgData(), base::RandInt(0, INT32_MAX), query,
                        manta::proto::RESOLUTION_1024);
  }
  return result;
}

class FakeSeaPenFetcher : public SeaPenFetcher {
 public:
  FakeSeaPenFetcher()
      : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  FakeSeaPenFetcher(const FakeSeaPenFetcher&) = delete;
  FakeSeaPenFetcher& operator=(const FakeSeaPenFetcher&) = delete;

  ~FakeSeaPenFetcher() override = default;

  void FetchThumbnails(const std::string& query,
                       OnFetchThumbnailsComplete callback) override {
    VLOG(1) << "Running query: " << query;
    sequenced_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&MakeFakeSeaPenImages, query),
        std::move(callback));
  }

  void FetchWallpaper(ash::SeaPenImage image,
                      OnFetchWallpaperComplete callback) override {
    VLOG(1) << "Fetching wallpaper: " << image.query
            << " target_resolution=" << base::to_underlying(target_resolution);
    std::move(callback).Run(std::move(image));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

#else  // defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

constexpr std::string_view kTemplateIdTag = "chromeos_wallpaper_template_id";

// Helper function to validate the Manta API output data.
bool IsValidOutput(manta::proto::OutputData output,
                   const std::string_view source) {
  if (!output.has_generation_seed()) {
    LOG(WARNING) << "Manta output data missing id for " << source;
    return false;
  }
  if (!output.has_image() || !output.image().has_serialized_bytes()) {
    LOG(WARNING) << "Manta output data missing image for" << source;
    return false;
  }
  return true;
}

std::string TemplateIdToString(
    ash::personalization_app::mojom::SeaPenTemplateId id) {
  switch (id) {
    case ash::personalization_app::mojom::SeaPenTemplateId::kFlower:
      return "flower";
    case ash::personalization_app::mojom::SeaPenTemplateId::kMineral:
      return "mineral";
  }
}

std::string TemplateChipToString(
    ash::personalization_app::mojom::SeaPenTemplateChip chip) {
  switch (chip) {
    case ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType:
      return "<flower_type>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor:
      return "<flower_color>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kMineralName:
      return "<mineral_name>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kMineralColor:
      return "<mineral_color>";
  }
}

std::string TemplateOptionToString(
    ash::personalization_app::mojom::SeaPenTemplateOption option) {
  switch (option) {
    case ash::personalization_app::mojom::SeaPenTemplateOption::kFlowerTypeRose:
      return "rose";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeCallaLily:
      return "calla_lily";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeWindflower:
      return "windflower";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeTulip:
      return "tulip";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeLilyOfTheValley:
      return "lily_of_the_valley";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeBirdOfParadise:
      return "bird_of_paradise";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeOrchid:
      return "orchid";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeRanunculus:
      return "ranunculus";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeDaisy:
      return "daisy";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeHydrangeas:
      return "hydrangeas";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorPink:
      return "pink";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorPurple:
      return "purple";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorBlue:
      return "blue";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorWhite:
      return "white";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorCoral:
      return "coral";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorYellow:
      return "yellow";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorGreen:
      return "green";
    case ash::personalization_app::mojom::SeaPenTemplateOption::kFlowerColorRed:
      return "red";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameWhiteQuartz:
      return "white_quartz";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameAmethyst:
      return "amethyst";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameBlueSapphire:
      return "blue_sapphire";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameAmberCarnelian:
      return "amber_carnelian";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameEmerald:
      return "emerald";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameRuby:
      return "ruby";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralColorWhite:
      return "white";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralColorPeriwinkle:
      return "periwinkle";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralColorPink:
      return "pink";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralColorLavender:
      return "lavender";
  }
}

bool IsValidTemplateQuery(
    const ash::personalization_app::mojom::SeaPenTemplateQueryPtr& query) {
  auto id = query->id;
  auto options = query->options;
  switch (id) {
    case ash::personalization_app::mojom::SeaPenTemplateId::kFlower: {
      auto flower_type = options
                             .find(ash::personalization_app::mojom::
                                       SeaPenTemplateChip::kFlowerType)
                             ->second;
      auto flower_color = options
                              .find(ash::personalization_app::mojom::
                                        SeaPenTemplateChip::kFlowerColor)
                              ->second;
      return (flower_type >= ash::personalization_app::mojom::
                                 SeaPenTemplateOption::kFlowerTypeRose &&
              flower_type <= ash::personalization_app::mojom::
                                 SeaPenTemplateOption::kFlowerTypeHydrangeas &&
              flower_color >= ash::personalization_app::mojom::
                                  SeaPenTemplateOption::kFlowerColorPink &&
              flower_color <= ash::personalization_app::mojom::
                                  SeaPenTemplateOption::kFlowerColorRed);
    }
    case ash::personalization_app::mojom::SeaPenTemplateId::kMineral: {
      auto mineral_name = options
                              .find(ash::personalization_app::mojom::
                                        SeaPenTemplateChip::kMineralName)
                              ->second;
      auto mineral_color = options
                               .find(ash::personalization_app::mojom::
                                         SeaPenTemplateChip::kMineralColor)
                               ->second;
      return (mineral_name >=
                  ash::personalization_app::mojom::SeaPenTemplateOption::
                      kMineralNameWhiteQuartz &&
              mineral_name <= ash::personalization_app::mojom::
                                  SeaPenTemplateOption::kMineralNameRuby &&
              mineral_color >= ash::personalization_app::mojom::
                                   SeaPenTemplateOption::kMineralColorWhite &&
              mineral_color <= ash::personalization_app::mojom::
                                   SeaPenTemplateOption::kMineralColorLavender);
    }
  }
  return true;
}

// Common helper function between `FetchThumbnails` and `FetchWallpaper`.
manta::proto::Request CreateMantaRequest(
    const ash::personalization_app::mojom::SeaPenQueryPtr& query,
    absl::optional<uint32_t> generation_seed,
    int num_output,
    manta::proto::ImageResolution target_resolution) {
  manta::proto::Request request;
  request.set_feature_name(manta::proto::FeatureName::CHROMEOS_WALLPAPER);
  manta::proto::RequestConfig& request_config =
      *request.mutable_request_config();
  if (generation_seed) {
    request_config.set_generation_seed(*generation_seed);
  }
  request_config.set_num_outputs(num_output);
  request_config.set_image_resolution(target_resolution);
  manta::proto::InputData& input_data = *request.add_input_data();
  if (query->is_text_query()) {
    input_data.set_text(query->get_text_query());
  } else if (query->is_template_query() &&
             IsValidTemplateQuery(query->get_template_query())) {
    input_data.set_tag(kTemplateIdTag.data());
    input_data.set_text(TemplateIdToString(query->get_template_query()->id));
    for (auto option : query->get_template_query()->options) {
      manta::proto::InputData& input_option = *request.add_input_data();
      input_option.set_tag(TemplateChipToString(option.first));
      input_option.set_text(TemplateOptionToString(option.second));
    }
  }
  return request;
}

class SeaPenFetcherImpl : public SeaPenFetcher {
 public:
  explicit SeaPenFetcherImpl(Profile* profile) {
    CHECK(ash::features::IsSeaPenEnabled());
    CHECK(manta::features::IsMantaServiceEnabled());
    auto* manta_service = manta::MantaServiceFactory::GetForProfile(profile);
    if (manta_service) {
      snapper_provider_ = manta_service->CreateSnapperProvider();
    }
  }

  SeaPenFetcherImpl(const SeaPenFetcherImpl&) = delete;
  SeaPenFetcherImpl& operator=(const SeaPenFetcherImpl&) = delete;

  ~SeaPenFetcherImpl() override = default;

  void FetchThumbnails(
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      OnFetchThumbnailsComplete callback) override {
    if (!snapper_provider_) {
      LOG(WARNING) << "SnapperProvider not available";
      std::move(callback).Run(absl::nullopt);
      return;
    }
    if (query->is_text_query() &&
        query->get_text_query().size() >
            ash::personalization_app::mojom::kMaximumSearchWallpaperTextBytes) {
      LOG(WARNING) << "Query too long. Size received: "
                   << query->get_text_query().size();
      std::move(callback).Run(absl::nullopt);
      return;
    }
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (pending_fetch_thumbnails_callback_) {
      std::move(pending_fetch_thumbnails_callback_).Run(absl::nullopt);
    }
    pending_fetch_thumbnails_callback_ = std::move(callback);
    auto target_resolution = manta::proto::ImageResolution::RESOLUTION_1024;
    auto request = CreateMantaRequest(query, absl::nullopt,
                                      /*num_output=*/6, target_resolution);
    snapper_provider_->Call(
        request, base::BindOnce(&SeaPenFetcherImpl::OnFetchThumbnailsDone,
                                weak_ptr_factory_.GetWeakPtr(), query.Clone(),
                                target_resolution));
  }

  void OnFetchThumbnailsDone(
      const ash::personalization_app::mojom::SeaPenQueryPtr& query,
      manta::proto::ImageResolution resolution,
      std::unique_ptr<manta::proto::Response> response,
      manta::MantaStatus status) {
    DCHECK(pending_fetch_thumbnails_callback_);
    if (status.status_code != manta::MantaStatusCode::kOk || !response) {
      LOG(WARNING) << "Failed to fetch manta response: " << status.message;
      std::move(pending_fetch_thumbnails_callback_).Run(absl::nullopt);
      return;
    }

    // TODO(b/309679160): Save template query to SeaPenImage
    auto thumbnail_query =
        query->is_text_query() ? query->get_text_query() : std::string();

    std::vector<ash::SeaPenImage> images;
    for (auto& data : *response->mutable_output_data()) {
      if (!IsValidOutput(data, __func__)) {
        continue;
      }
      images.emplace_back(
          std::move(*data.mutable_image()->mutable_serialized_bytes()),
          data.generation_seed(), thumbnail_query, resolution);
    }
    std::move(pending_fetch_thumbnails_callback_).Run(std::move(images));
  }

  void FetchWallpaper(const ash::SeaPenImage& thumbnail,
                      OnFetchWallpaperComplete callback) override {
    if (!snapper_provider_) {
      LOG(WARNING) << "SnapperProvider not available";
      std::move(callback).Run(absl::nullopt);
      return;
    }
    CHECK_LE(thumbnail.query.size(),
             ash::personalization_app::mojom::kMaximumSearchWallpaperTextBytes);
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (pending_fetch_wallpaper_callback_) {
      std::move(pending_fetch_wallpaper_callback_).Run(absl::nullopt);
    }
    pending_fetch_wallpaper_callback_ = std::move(callback);
    // TODO(b/300129219): Add higher resolution when supported
    auto target_resolution = manta::proto::ImageResolution::RESOLUTION_1024;

    // TODO(b/309679160): Update when ash::SeaPenImage holds SeaPenQuery.
    ash::personalization_app::mojom::SeaPenQueryPtr thumbnail_query =
        ash::personalization_app::mojom::SeaPenQuery::NewTextQuery(
            thumbnail.query);
    snapper_provider_->Call(
        CreateMantaRequest(thumbnail_query, thumbnail.id, /*num_output=*/1,
                           target_resolution),
        base::BindOnce(&SeaPenFetcherImpl::OnFetchWallpaperDone,
                       weak_ptr_factory_.GetWeakPtr(), thumbnail.query,
                       target_resolution));
  }

  void OnFetchWallpaperDone(const std::string& query,
                            manta::proto::ImageResolution resolution,
                            std::unique_ptr<manta::proto::Response> response,
                            manta::MantaStatus status) {
    DCHECK(pending_fetch_wallpaper_callback_);
    if (status.status_code != manta::MantaStatusCode::kOk || !response) {
      LOG(WARNING) << "Failed to fetch manta response: " << status.message;
      std::move(pending_fetch_wallpaper_callback_).Run(absl::nullopt);
      return;
    }
    std::vector<ash::SeaPenImage> images;
    for (auto& data : *response->mutable_output_data()) {
      if (!IsValidOutput(data, __func__)) {
        continue;
      }
      images.emplace_back(
          std::move(*data.mutable_image()->mutable_serialized_bytes()),
          data.generation_seed(), query, resolution);
    }
    if (images.empty()) {
      LOG(WARNING) << "Got empty images";
      std::move(pending_fetch_wallpaper_callback_).Run(absl::nullopt);
      return;
    }
    if (images.size() > 1) {
      LOG(WARNING) << "Got more than 1 output image";
    }
    std::move(pending_fetch_wallpaper_callback_).Run(std::move(images.at(0)));
  }

 private:
  OnFetchThumbnailsComplete pending_fetch_thumbnails_callback_;
  OnFetchWallpaperComplete pending_fetch_wallpaper_callback_;
  std::unique_ptr<manta::SnapperProvider> snapper_provider_;
  base::WeakPtrFactory<SeaPenFetcherImpl> weak_ptr_factory_{this};
};

#endif  // defined(FAKE_SEA_PEN_FETCHER_FOR_DEBUG)

}  // namespace

SeaPenFetcher::SeaPenFetcher() = default;

SeaPenFetcher::~SeaPenFetcher() = default;

std::unique_ptr<SeaPenFetcher> SeaPenFetcher::MakeSeaPenFetcher(
    Profile* profile) {
#ifdef FAKE_SEA_PEN_FETCHER_FOR_DEBUG
  return std::make_unique<FakeSeaPenFetcher>();
#else   // FAKE_SEA_PEN_FETCHER_FOR_DEBUG
  return std::make_unique<SeaPenFetcherImpl>(profile);
#endif  // FAKE_SEA_PEN_FETCHER_FOR_DEBUG
}

}  // namespace wallpaper_handlers
