// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RECORDING_SOURCE_H_
#define CC_TEST_FAKE_RECORDING_SOURCE_H_

#include <stddef.h>

#include "cc/base/region.h"
#include "cc/layers/recording_source.h"
#include "cc/paint/paint_filter.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace cc {

// This class provides method for test to add bitmap and draw rect to content
// layer client. This class also provides function to rerecord to generate a new
// display list.
class FakeRecordingSource : public RecordingSource {
 public:
  FakeRecordingSource();
  ~FakeRecordingSource() override {}

  static std::unique_ptr<FakeRecordingSource> CreateRecordingSource(
      const gfx::Rect& recorded_viewport,
      const gfx::Size& layer_bounds) {
    std::unique_ptr<FakeRecordingSource> recording_source(
        new FakeRecordingSource);
    recording_source->SetRecordedViewport(recorded_viewport);
    recording_source->SetLayerBounds(layer_bounds);
    return recording_source;
  }

  static std::unique_ptr<FakeRecordingSource> CreateFilledRecordingSource(
      const gfx::Size& layer_bounds) {
    std::unique_ptr<FakeRecordingSource> recording_source(
        new FakeRecordingSource);
    recording_source->SetRecordedViewport(gfx::Rect(layer_bounds));
    recording_source->SetLayerBounds(layer_bounds);
    return recording_source;
  }

  // RecordingSource overrides.
  scoped_refptr<RasterSource> CreateRasterSource() const override;

  void SetRecordedViewport(const gfx::Rect& recorded_viewport) {
    recorded_viewport_ = recorded_viewport;
  }

  void SetLayerBounds(const gfx::Size& layer_bounds) {
    size_ = layer_bounds;
    client_.set_bounds(layer_bounds);
  }

  void SetClearCanvasWithDebugColor(bool clear) {
    clear_canvas_with_debug_color_ = clear;
  }

  void set_fill_with_nonsolid_color(bool nonsolid) {
    client_.set_fill_with_nonsolid_color(nonsolid);
  }

  void set_has_non_aa_paint(bool has_non_aa_paint) {
    client_.set_has_non_aa_paint(has_non_aa_paint);
  }

  void set_has_slow_paths(bool slow_paths) {
    client_.set_contains_slow_paths(slow_paths);
  }

  void Rerecord() {
    SetNeedsDisplayRect(recorded_viewport_);
    Region invalidation;
    gfx::Rect new_recorded_viewport = client_.PaintableRegion();
    scoped_refptr<DisplayItemList> display_list =
        client_.PaintContentsToDisplayList(
            ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
    size_t painter_reported_memory_usage =
        client_.GetApproximateUnsharedMemoryUsage();
    UpdateAndExpandInvalidation(&invalidation, size_, new_recorded_viewport);
    UpdateDisplayItemList(display_list, painter_reported_memory_usage,
                          recording_scale_factor_);
  }

  void add_draw_rect(const gfx::Rect& rect) {
    client_.add_draw_rect(rect, default_flags_);
  }

  void add_draw_rect_with_flags(const gfx::Rect& rect,
                                const PaintFlags& flags) {
    client_.add_draw_rect(rect, flags);
  }

  void add_draw_rectf(const gfx::RectF& rect) {
    client_.add_draw_rectf(rect, default_flags_);
  }

  void add_draw_rectf_with_flags(const gfx::RectF& rect,
                                 const PaintFlags& flags) {
    client_.add_draw_rectf(rect, flags);
  }

  void add_draw_image(sk_sp<SkImage> image, const gfx::Point& point) {
    client_.add_draw_image(std::move(image), point, default_flags_);
  }
  void add_draw_image(PaintImage image, const gfx::Point& point) {
    client_.add_draw_image(std::move(image), point, default_flags_);
  }

  void add_draw_image_with_transform(PaintImage image,
                                     const gfx::Transform& transform) {
    client_.add_draw_image_with_transform(std::move(image), transform,
                                          default_flags_);
  }

  void add_draw_image_with_flags(sk_sp<SkImage> image,
                                 const gfx::Point& point,
                                 const PaintFlags& flags) {
    client_.add_draw_image(std::move(image), point, flags);
  }

  void set_default_flags(const PaintFlags& flags) { default_flags_ = flags; }

  void set_reported_memory_usage(size_t reported_memory_usage) {
    client_.set_reported_memory_usage(reported_memory_usage);
  }

  void reset_draws() {
    client_ = FakeContentLayerClient();
    client_.set_bounds(size_);
  }

  void SetPlaybackAllowedEvent(base::WaitableEvent* event) {
    playback_allowed_event_ = event;
  }

  // Checks that the basic properties of the |other| match |this|.  For the
  // DisplayItemList, it checks that the painted result matches the painted
  // result of |other|.
  bool EqualsTo(const FakeRecordingSource& other);

  void SetRecordingScaleFactor(float recording_scale_factor) {
    recording_scale_factor_ = recording_scale_factor;
  }

  const scoped_refptr<DisplayItemList> GetDisplayItemList() const {
    return display_list_;
  }

 private:
  FakeContentLayerClient client_;
  PaintFlags default_flags_;
  base::WaitableEvent* playback_allowed_event_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RECORDING_SOURCE_H_
