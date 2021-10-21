// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_resource_metadata.h"

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace cc {

SkottieResourceMetadataMap::SkottieResourceMetadataMap() = default;

SkottieResourceMetadataMap::SkottieResourceMetadataMap(
    const SkottieResourceMetadataMap&) = default;

SkottieResourceMetadataMap& SkottieResourceMetadataMap::operator=(
    const SkottieResourceMetadataMap&) = default;

SkottieResourceMetadataMap::~SkottieResourceMetadataMap() = default;

bool SkottieResourceMetadataMap::RegisterAsset(base::StringPiece resource_path,
                                               base::StringPiece resource_name,
                                               base::StringPiece resource_id) {
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
                                       .Append(resource_name_component))
                      .second;
  if (!inserted) {
    LOG(ERROR) << "Skottie animation has assets with duplicate resource_id: "
               << resource_id;
  }
  return inserted;
}

SkottieResourceIdHash HashSkottieResourceId(base::StringPiece resource_id) {
  return SkottieResourceIdHash::FromUnsafeValue(base::FastHash(resource_id));
}

}  // namespace cc
