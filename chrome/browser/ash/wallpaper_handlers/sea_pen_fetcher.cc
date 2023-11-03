// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-forward.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/snapper_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace wallpaper_handlers {

SeaPenFetcher::SeaPenFetcher(Profile* profile) {
  CHECK(ash::features::IsSeaPenEnabled());
  CHECK(manta::features::IsMantaServiceEnabled());
  auto* manta_service = manta::MantaServiceFactory::GetForProfile(profile);
  if (manta_service) {
    snapper_provider_ = manta_service->CreateSnapperProvider();
  }
}

SeaPenFetcher::~SeaPenFetcher() = default;

void SeaPenFetcher::Start(const std::string& query,
                          OnWallpaperSearchComplete callback) {
  if (!snapper_provider_) {
    LOG(WARNING) << "SnapperProvider not available";
    std::move(callback).Run(absl::nullopt);
    return;
  }
  if (query.size() >
      ash::personalization_app::mojom::kMaximumSearchWallpaperTextBytes) {
    LOG(WARNING) << "Query too long. Size received: " << query.size();
    std::move(callback).Run(absl::nullopt);
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (pending_callback_) {
    std::move(pending_callback_).Run(absl::nullopt);
  }
  pending_callback_ = std::move(callback);
  manta::proto::Request request;
  request.set_feature_name(manta::proto::FeatureName::CHROMEOS_WALLPAPER);
  manta::proto::RequestConfig& request_config =
      *request.mutable_request_config();
  request_config.set_num_outputs(6);
  request_config.set_image_resolution(
      manta::proto::ImageResolution::RESOLUTION_1024);
  manta::proto::InputData& input_data = *request.add_input_data();
  input_data.set_text(query);
  snapper_provider_->Call(request,
                          base::BindOnce(&SeaPenFetcher::OnSnapperDone,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void SeaPenFetcher::OnSnapperDone(
    std::unique_ptr<manta::proto::Response> response,
    manta::MantaStatus status) {
  DCHECK(pending_callback_);
  if (status.status_code != manta::MantaStatusCode::kOk || !response) {
    LOG(WARNING) << "Failed to fetch manta response: " << status.message;
    std::move(pending_callback_).Run(absl::nullopt);
    return;
  }
  std::vector<ash::SeaPenImage> images;
  for (auto& data : *response->mutable_output_data()) {
    if (!data.has_generation_seed()) {
      LOG(WARNING) << "Manta output data missing generation seed";
      continue;
    }
    if (!data.has_image() || !data.image().has_serialized_bytes()) {
      LOG(WARNING) << "Manta output data missing image";
      continue;
    }
    images.emplace_back(
        std::move(*data.mutable_image()->mutable_serialized_bytes()),
        data.generation_seed());
  }
  std::move(pending_callback_).Run(std::move(images));
}

}  // namespace wallpaper_handlers
