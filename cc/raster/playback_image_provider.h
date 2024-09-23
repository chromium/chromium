// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_
#define CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "cc/cc_export.h"
#include "cc/paint/image_id.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/target_color_params.h"
#include "ui/gfx/color_space.h"

namespace cc {
class ImageDecodeCache;

// PlaybackImageProvider is used to replace lazy generated PaintImages with
// decoded images for raster from the ImageDecodeCache.
class CC_EXPORT PlaybackImageProvider : public ImageProvider {
 public:
  enum class RasterMode { kSoftware, kGpu, kOop };
  struct CC_EXPORT Settings {
    Settings();
    Settings(const Settings&) = delete;
    Settings(Settings&&);
    ~Settings();

    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&);

    // The set of image ids to skip during raster.
    PaintImageIdFlatSet images_to_skip;

    // The frame index to use for the given image id. If no index is provided,
    // the frame index provided in the PaintImage will be used.
    base::flat_map<PaintImage::Id, size_t> image_to_current_frame_index;

    // Indicates the raster backend that will be consuming the decoded images.
    RasterMode raster_mode = RasterMode::kSoftware;
  };

  // If no settings are provided, all images are skipped during rasterization.
  PlaybackImageProvider(ImageDecodeCache* cache,
                        const TargetColorParams& target_color_params,
                        std::optional<Settings>&& settings);
  PlaybackImageProvider(const PlaybackImageProvider&) = delete;
  PlaybackImageProvider(PlaybackImageProvider&& other);
  ~PlaybackImageProvider() override;

  PlaybackImageProvider& operator=(const PlaybackImageProvider&) = delete;
  PlaybackImageProvider& operator=(PlaybackImageProvider&& other);

  // ImageProvider implementation.
  ImageProvider::ScopedResult GetRasterContent(
      const DrawImage& draw_image) override;

 private:
  // RAW_PTR_EXCLUSION: ImageDecodeCache is marked as not supported by raw_ptr.
  // See raw_ptr.h for more information.
  RAW_PTR_EXCLUSION ImageDecodeCache* cache_ = nullptr;
  TargetColorParams target_color_params_;
  std::optional<Settings> settings_;
};

}  // namespace cc

#endif  // CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_
