// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/playback_image_provider.h"

#include <utility>
#include "base/functional/bind.h"
#include "cc/tiles/image_decode_cache.h"
#include "gpu/command_buffer/common/mailbox.h"

namespace cc {
namespace {
void UnrefImageFromCache(DrawImage draw_image,
                         ImageDecodeCache* cache,
                         DecodedDrawImage decoded_draw_image) {
  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
}

}  // namespace

PlaybackImageProvider::PlaybackImageProvider(
    ImageDecodeCache* cache,
    const TargetColorParams& target_color_params,
    std::optional<Settings>&& settings)
    : cache_(cache),
      target_color_params_(target_color_params),
      settings_(std::move(settings)) {
  DCHECK(cache_);
}

PlaybackImageProvider::~PlaybackImageProvider() = default;

PlaybackImageProvider::PlaybackImageProvider(PlaybackImageProvider&& other) =
    default;

PlaybackImageProvider& PlaybackImageProvider::operator=(
    PlaybackImageProvider&& other) = default;

ImageProvider::ScopedResult PlaybackImageProvider::GetRasterContent(
    const DrawImage& draw_image) {
  DCHECK(!draw_image.paint_image().IsPaintWorklet());
  // Return an empty decoded image if we are skipping all images during this
  // raster.
  if (!settings_.has_value())
    return ScopedResult();

  const PaintImage& paint_image = draw_image.paint_image();
  if (settings_->images_to_skip.count(paint_image.stable_id()) != 0) {
    DCHECK(paint_image.IsLazyGenerated());
    return ScopedResult();
  }

  const auto& it =
      settings_->image_to_current_frame_index.find(paint_image.stable_id());
  size_t frame_index = it == settings_->image_to_current_frame_index.end()
                           ? PaintImage::kDefaultFrameIndex
                           : it->second;

  DrawImage adjusted_image(draw_image, 1.f, frame_index, target_color_params_);
  if (!cache_->UseCacheForDrawImage(adjusted_image)) {
    if (settings_->raster_mode == RasterMode::kOop) {
      return ScopedResult(DecodedDrawImage(paint_image.GetMailbox(),
                                           draw_image.filter_quality()));
    } else if (settings_->raster_mode == RasterMode::kGpu) {
      return ScopedResult(DecodedDrawImage(
          paint_image.GetAcceleratedSkImage(), nullptr, SkSize::Make(0, 0),
          SkSize::Make(1.f, 1.f), draw_image.filter_quality(),
          true /* is_budgeted */));
    } else {
      return ScopedResult(DecodedDrawImage(
          paint_image.GetSwSkImage(), nullptr, SkSize::Make(0, 0),
          SkSize::Make(1.f, 1.f), draw_image.filter_quality(),
          true /* is_budgeted */));
    }
  }

  auto decoded_draw_image = cache_->GetDecodedImageForDraw(adjusted_image);
  return ScopedResult(
      decoded_draw_image,
      base::BindOnce(&UnrefImageFromCache, std::move(adjusted_image), cache_,
                     decoded_draw_image));
}

PlaybackImageProvider::Settings::Settings() = default;
PlaybackImageProvider::Settings::Settings(PlaybackImageProvider::Settings&&) =
    default;
PlaybackImageProvider::Settings::~Settings() = default;
PlaybackImageProvider::Settings& PlaybackImageProvider::Settings::operator=(
    PlaybackImageProvider::Settings&&) = default;

}  // namespace cc
