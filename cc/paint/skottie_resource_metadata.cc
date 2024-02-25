// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_resource_metadata.h"

#include <utility>

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace cc {

SkottieResourceMetadataMap::ImageAssetMetadata::ImageAssetMetadata(
    base::FilePath resource_path_in,
    std::optional<gfx::Size> size_in)
    : resource_path(std::move(resource_path_in)), size(std::move(size_in)) {}

SkottieResourceMetadataMap::SkottieResourceMetadataMap() = default;

SkottieResourceMetadataMap::SkottieResourceMetadataMap(
    const SkottieResourceMetadataMap&) = default;

SkottieResourceMetadataMap& SkottieResourceMetadataMap::operator=(
    const SkottieResourceMetadataMap&) = default;

SkottieResourceMetadataMap::~SkottieResourceMetadataMap() = default;

bool SkottieResourceMetadataMap::RegisterAsset(std::string_view resource_path,
                                               std::string_view resource_name,
                                               std::string_view resource_id,
                                               std::optional<gfx::Size> size) {
  DCHECK(!size || !size->IsEmpty());
  if (resource_id.empty()) {
    LOG(ERROR) << "Skottie animation has asset with empty resource_id";
    return false;
  }

  base::FilePath resource_name_component =
      base::FilePath::FromASCII(resource_name);
  if (resource_name_component.IsAbsolute()) {
    // If the path is absolute, base::FilePath::Append() will fail anyways,
    // likely with a fatal error.
    LOG(ERROR) << "Skottie animation specifies an absolute resource_name path: "
               << resource_name << ". Must be relative.";
    return false;
  }

  bool inserted = asset_storage_
                      .try_emplace(std::string(resource_id),
                                   base::FilePath::FromASCII(resource_path)
                                       .Append(resource_name_component),
                                   std::move(size))
                      .second;
  if (!inserted) {
    LOG(ERROR) << "Skottie animation has assets with duplicate resource_id: "
               << resource_id;
  }
  return inserted;
}

SkottieResourceIdHash HashSkottieResourceId(std::string_view resource_id) {
  return SkottieResourceIdHash::FromUnsafeValue(
      base::PersistentHash(resource_id));
}

}  // namespace cc
