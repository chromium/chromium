// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKOTTIE_RESOURCE_METADATA_H_
#define CC_PAINT_SKOTTIE_RESOURCE_METADATA_H_

#include <string>

#include <optional>
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/types/id_type.h"
#include "cc/paint/paint_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// Each asset in a Skottie animation has a unique "resource_id" in string
// format. This is a map from resource_id to its corresponding location.
class CC_PAINT_EXPORT SkottieResourceMetadataMap {
 public:
  // Metadata for a single image asset.
  struct ImageAssetMetadata {
    ImageAssetMetadata(base::FilePath resource_path_in,
                       std::optional<gfx::Size> size_in);

    base::FilePath resource_path;
    // May be null if asset's dimensions are not specified in the animation
    // file.
    std::optional<gfx::Size> size;
  };

  using Storage =
      base::flat_map<std::string /*resource_id*/, ImageAssetMetadata>;

  SkottieResourceMetadataMap();
  SkottieResourceMetadataMap(const SkottieResourceMetadataMap&);
  SkottieResourceMetadataMap& operator=(const SkottieResourceMetadataMap&);
  ~SkottieResourceMetadataMap();

  // Adds a new asset to the map. Returns true on success; false if an asset
  // with the provided |resource_id| already exists or if the |resource_id| is
  // invalid.
  bool RegisterAsset(base::StringPiece resource_path,
                     base::StringPiece resource_name,
                     base::StringPiece resource_id,
                     std::optional<gfx::Size> size);

  const Storage& asset_storage() const { return asset_storage_; }

 private:
  Storage asset_storage_;
};

// For performance reasons, the resource_id can be hashed, and the caller can
// circulate the resulting integer throughout the system.
using SkottieResourceIdHash =
    base::IdType<SkottieResourceMetadataMap, size_t, 0>;
SkottieResourceIdHash CC_PAINT_EXPORT
HashSkottieResourceId(base::StringPiece resource_id);

}  // namespace cc

#endif  // CC_PAINT_SKOTTIE_RESOURCE_METADATA_H_
