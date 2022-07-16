// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_mru_resource_provider.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"

namespace cc {

SkottieMRUResourceProvider::ImageAsset::ImageAsset(
    base::StringPiece resource_id)
    : resource_id_(resource_id) {
  // The thread on which the animation is loaded may not necessarily match the
  // thread on which rendering occurs; hence, detach the SequenceChecker.
  DETACH_FROM_SEQUENCE(render_sequence_checker_);
}

SkottieMRUResourceProvider::ImageAsset::~ImageAsset() = default;

void SkottieMRUResourceProvider::ImageAsset::SetCurrentFrameData(
    FrameData frame_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(render_sequence_checker_);
  current_frame_data_ = std::move(frame_data);
}

bool SkottieMRUResourceProvider::ImageAsset::isMultiFrame() {
  return true;
}

SkottieMRUResourceProvider::FrameData
SkottieMRUResourceProvider::ImageAsset::getFrameData(float t) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(render_sequence_checker_);
  DCHECK(current_frame_data_.image)
      << "No image available for asset " << resource_id_ << " at time " << t;
  return current_frame_data_;
}

SkottieMRUResourceProvider::SkottieMRUResourceProvider() = default;

SkottieMRUResourceProvider::~SkottieMRUResourceProvider() = default;

const SkottieResourceMetadataMap&
SkottieMRUResourceProvider::GetImageAssetMetadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return image_asset_metadata_;
}

const SkottieMRUResourceProvider::ImageAssetMap&
SkottieMRUResourceProvider::GetImageAssetMap() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return image_asset_map_;
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
  auto new_asset = sk_sp<ImageAsset>(new ImageAsset(resource_id));
  image_asset_map_[HashSkottieResourceId(resource_id)] = new_asset;
  return new_asset;
}

}  // namespace cc
