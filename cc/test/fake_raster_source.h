// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RASTER_SOURCE_H_
#define CC_TEST_FAKE_RASTER_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/raster/raster_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class WaitableEvent;
}

namespace cc {

class RecordingSource;

class FakeRasterSource : public RasterSource {
 public:
  explicit FakeRasterSource(const RecordingSource& recording_source);
  FakeRasterSource(const RecordingSource& recording_source,
                   base::WaitableEvent* playback_allowed_event);

  static scoped_refptr<FakeRasterSource> CreateInfiniteFilled();
  static scoped_refptr<FakeRasterSource> CreateFilled(const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreateFilledWithImages(
      const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreateFilledWithText(
      const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreateFilledWithPaintWorklet(
      const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreateFilledSolidColor(
      const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreatePartiallyFilled(
      const gfx::Size& size,
      const gfx::Rect& recorded_bounds);
  static scoped_refptr<FakeRasterSource> CreateEmpty(const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreateFromRecordingSource(
      const RecordingSource& recording_source);
  static scoped_refptr<FakeRasterSource> CreateFromRecordingSourceWithWaitable(
      const RecordingSource& recording_source,
      base::WaitableEvent* playback_allowed_event);

  void SetDirectlyCompositedImageDefaultRasterScale(gfx::Vector2dF scale);

 protected:
  ~FakeRasterSource() override;

  void PlaybackDisplayListToCanvas(
      SkCanvas* canvas,
      const PlaybackSettings& settings) const override;

 private:
  raw_ptr<base::WaitableEvent> playback_allowed_event_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RASTER_SOURCE_H_
