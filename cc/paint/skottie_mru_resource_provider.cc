// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_mru_resource_provider.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
namespace {

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
      case SkottieWrapper::FrameDataFetchResult::NEW_DATA_AVAILABLE:
        current_frame_data_ = std::move(new_frame_data);
        break;
      case SkottieWrapper::FrameDataFetchResult::NO_UPDATE:
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
    FrameDataCallback frame_data_cb)
    : frame_data_cb_(std::move(frame_data_cb)) {}

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
  if (!image_asset_metadata_.RegisterAsset(resource_path, resource_name,
                                           resource_id)) {
    return nullptr;
  }
  return sk_make_sp<ImageAssetImpl>(HashSkottieResourceId(resource_id),
                                    frame_data_cb_);
}

}  // namespace cc
