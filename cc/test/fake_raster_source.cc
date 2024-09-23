// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_raster_source.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/synchronization/waitable_event.h"
#include "cc/base/features.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateInfiniteFilled() {
  gfx::Size size(std::numeric_limits<int>::max() / 10,
                 std::numeric_limits<int>::max() / 10);
  return CreateFilled(size);
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFilled(
    const gfx::Size& size) {
  FakeRecordingSource recording_source(size);

  PaintFlags red_flags;
  red_flags.setColor(SK_ColorRED);
  recording_source.add_draw_rect_with_flags(gfx::Rect(size), red_flags);

  // Note that we set the blend mode to kMultiply to prevent this raster source
  // from being detected as a solid color (and we add a check for this below).
  // An alternative would have been to create a pattern, but this would not work
  // for tests that require |size| to be 1x1.
  PaintFlags salmon_pink_flags;
  salmon_pink_flags.setColor(SK_ColorRED);
  salmon_pink_flags.setBlendMode(SkBlendMode::kMultiply);
  salmon_pink_flags.setAlphaf(0.5f);
  recording_source.add_draw_rect_with_flags(gfx::Rect(size), salmon_pink_flags);

  recording_source.Rerecord();
  auto raster_source = base::MakeRefCounted<FakeRasterSource>(recording_source);
  if (raster_source->IsSolidColor())
    ADD_FAILURE() << "Unexpected solid color!";
  return raster_source;
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFilledWithImages(
    const gfx::Size& size) {
  FakeRecordingSource recording_source(size);

  for (int y = 0; y < size.height(); y += 100) {
    for (int x = 0; x < size.width(); x += 100) {
      recording_source.add_draw_image(
          CreateDiscardablePaintImage(gfx::Size(100, 100)), gfx::Point(x, y));
    }
  }
  recording_source.Rerecord();
  return base::MakeRefCounted<FakeRasterSource>(recording_source);
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFilledWithText(
    const gfx::Size& size) {
  FakeRecordingSource recording_source(size);
  recording_source.add_draw_rect(gfx::Rect(size));
  recording_source.set_has_draw_text_op();
  recording_source.Rerecord();
  return base::MakeRefCounted<FakeRasterSource>(recording_source);
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFilledWithPaintWorklet(
    const gfx::Size& size) {
  FakeRecordingSource recording_source(size);

  auto input = base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(size));
  recording_source.add_draw_image(
      CreatePaintWorkletPaintImage(std::move(input)), gfx::Point(0, 0));

  recording_source.Rerecord();
  return base::MakeRefCounted<FakeRasterSource>(recording_source);
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFilledSolidColor(
    const gfx::Size& size) {
  FakeRecordingSource recording_source(size);

  PaintFlags red_flags;
  red_flags.setColor(SK_ColorRED);
  recording_source.add_draw_rect_with_flags(gfx::Rect(size), red_flags);
  recording_source.Rerecord();
  auto raster_source = base::MakeRefCounted<FakeRasterSource>(recording_source);
  if (!raster_source->IsSolidColor())
    ADD_FAILURE() << "Not solid color!";
  return raster_source;
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreatePartiallyFilled(
    const gfx::Size& size,
    const gfx::Rect& recorded_bounds) {
  // Otherwise the caller should call CreateEmpty().
  DCHECK(!size.IsEmpty());
  DCHECK(!recorded_bounds.IsEmpty());
  DCHECK(gfx::Rect(size).Contains(recorded_bounds));
  FakeRecordingSource recording_source(size);

  PaintFlags red_flags;
  red_flags.setColor(SK_ColorRED);
  recording_source.add_draw_rect_with_flags(recorded_bounds, red_flags);

  gfx::Rect smaller_rect(recorded_bounds.origin(),
                         recorded_bounds.size() - gfx::Size(10, 10));
  PaintFlags green_flags;
  green_flags.setColor(SK_ColorGREEN);
  recording_source.add_draw_rect_with_flags(smaller_rect, green_flags);

  recording_source.Rerecord();
  return base::MakeRefCounted<FakeRasterSource>(recording_source);
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateEmpty(
    const gfx::Size& size) {
  return base::MakeRefCounted<FakeRasterSource>(FakeRecordingSource(size));
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFromRecordingSource(
    const RecordingSource& recording_source) {
  return base::MakeRefCounted<FakeRasterSource>(recording_source);
}

scoped_refptr<FakeRasterSource>
FakeRasterSource::CreateFromRecordingSourceWithWaitable(
    const RecordingSource& recording_source,
    base::WaitableEvent* playback_allowed_event) {
  return base::MakeRefCounted<FakeRasterSource>(recording_source,
                                                playback_allowed_event);
}

FakeRasterSource::FakeRasterSource(const RecordingSource& recording_source)
    : RasterSource(recording_source), playback_allowed_event_(nullptr) {}

FakeRasterSource::FakeRasterSource(const RecordingSource& recording_source,
                                   base::WaitableEvent* playback_allowed_event)
    : RasterSource(recording_source),
      playback_allowed_event_(playback_allowed_event) {}

FakeRasterSource::~FakeRasterSource() = default;

void FakeRasterSource::PlaybackDisplayListToCanvas(
    SkCanvas* canvas,
    const PlaybackSettings& settings) const {
  if (playback_allowed_event_)
    playback_allowed_event_->Wait();
  RasterSource::PlaybackDisplayListToCanvas(canvas, settings);
}

void FakeRasterSource::SetDirectlyCompositedImageDefaultRasterScale(
    gfx::Vector2dF scale) {
  directly_composited_image_info_.emplace();
  directly_composited_image_info_->default_raster_scale = scale;
}

}  // namespace cc
