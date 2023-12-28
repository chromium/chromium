// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_mru_resource_provider.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
namespace {

constexpr std::string_view kAssetsKey = "assets";
constexpr std::string_view kIdKey = "id";
constexpr std::string_view kWidthKey = "w";
constexpr std::string_view kHeightKey = "h";

// TODO(fmalita): Remove explicit parsing and pass size param directly from
// Skottie.
base::flat_map</*asset_id*/ std::string, gfx::Size> ParseImageAssetDimensions(
    std::string_view animation_json) {
  base::flat_map<std::string, gfx::Size> image_asset_sizes;

  std::optional<base::Value> animation_dict =
      base::JSONReader::Read(animation_json);
  if (!animation_dict || !animation_dict->is_dict()) {
    LOG(ERROR) << "Failed to parse Lottie animation json";
    return image_asset_sizes;
  }

  const base::Value::List* assets =
      animation_dict->GetDict().FindList(kAssetsKey);
  // An animation may legitimately have no assets in it.
  if (!assets)
    return image_asset_sizes;

  for (const base::Value& asset : *assets) {
    if (!asset.is_dict()) {
      LOG(ERROR) << "Found invalid asset in animation with type "
                 << base::Value::GetTypeName(asset.type());
      continue;
    }
    const base::Value::Dict& asset_dict = asset.GetDict();

    const std::string* id = asset_dict.FindString(kIdKey);
    std::optional<int> width = asset_dict.FindInt(kWidthKey);
    std::optional<int> height = asset_dict.FindInt(kHeightKey);
    if (id && width && height && *width > 0 && *height > 0 &&
        !image_asset_sizes.emplace(*id, gfx::Size(*width, *height)).second) {
      LOG(WARNING) << "Multiple assets found in animation with id " << *id;
    }
  }
  return image_asset_sizes;
}

class ImageAssetImpl : public skresources::ImageAsset {
 public:
  using FrameData = skresources::ImageAsset::FrameData;
  using FrameDataCallback = SkottieWrapper::FrameDataCallback;

  ImageAssetImpl(SkottieResourceIdHash asset_id,
                 FrameDataCallback frame_data_cb)
      : asset_id_(asset_id), frame_data_cb_(std::move(frame_data_cb)) {
    DCHECK(frame_data_cb_);
  }

  bool isMultiFrame() override { return true; }

  FrameData getFrameData(float t) override {
    FrameData new_frame_data;
    SkottieWrapper::FrameDataFetchResult result = frame_data_cb_.Run(
        asset_id_, t, new_frame_data.image, new_frame_data.sampling);
    switch (result) {
      case SkottieWrapper::FrameDataFetchResult::kNewDataAvailable:
        current_frame_data_ = std::move(new_frame_data);
        break;
      case SkottieWrapper::FrameDataFetchResult::kNoUpdate:
        break;
    }
    return current_frame_data_;
  }

 private:
  const SkottieResourceIdHash asset_id_;
  const FrameDataCallback frame_data_cb_;
  FrameData current_frame_data_;
};

}  // namespace

SkottieMRUResourceProvider::SkottieMRUResourceProvider(
    FrameDataCallback frame_data_cb,
    std::string_view animation_json)
    : frame_data_cb_(std::move(frame_data_cb)),
      image_asset_sizes_(ParseImageAssetDimensions(animation_json)) {}

SkottieMRUResourceProvider::~SkottieMRUResourceProvider() = default;

const SkottieResourceMetadataMap&
SkottieMRUResourceProvider::GetImageAssetMetadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return image_asset_metadata_;
}

sk_sp<skresources::ImageAsset> SkottieMRUResourceProvider::loadImageAsset(
    const char resource_path[],
    const char resource_name[],
    const char resource_id[]) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<gfx::Size> size;
  if (image_asset_sizes_.contains(resource_id))
    size.emplace(image_asset_sizes_.at(resource_id));

  if (!image_asset_metadata_.RegisterAsset(resource_path, resource_name,
                                           resource_id, std::move(size))) {
    return nullptr;
  }
  return sk_make_sp<ImageAssetImpl>(HashSkottieResourceId(resource_id),
                                    frame_data_cb_);
}

}  // namespace cc
