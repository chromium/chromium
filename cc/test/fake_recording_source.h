// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RECORDING_SOURCE_H_
#define CC_TEST_FAKE_RECORDING_SOURCE_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "cc/base/region.h"
#include "cc/layers/recording_source.h"
#include "cc/paint/paint_filter.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

// This class provides method for test to add bitmap and draw rect to content
// layer client. This class also provides function to rerecord to generate a new
// display list.
class FakeRecordingSource : public RecordingSource {
 public:
  explicit FakeRecordingSource(const gfx::Size& layer_bounds);
  ~FakeRecordingSource();

  void SetLayerBounds(const gfx::Size& layer_bounds) {
    size_ = layer_bounds;
    client_->set_bounds(layer_bounds);
  }

  void set_fill_with_nonsolid_color(bool nonsolid) {
    client_->set_fill_with_nonsolid_color(nonsolid);
  }

  void set_has_non_aa_paint(bool has_non_aa_paint) {
    client_->set_has_non_aa_paint(has_non_aa_paint);
  }

  void set_has_slow_paths(bool slow_paths) {
    client_->set_contains_slow_paths(slow_paths);
  }

  void set_has_draw_text_op() { client_->set_has_draw_text_op(); }

  void Rerecord() {
    SetNeedsDisplayRect(gfx::Rect(size_));
    Region invalidation;
    Update(size_, recording_scale_factor_, *client_, invalidation);
  }

  void add_draw_rect(const gfx::Rect& rect) {
    client_->add_draw_rect(rect, default_flags_);
  }

  void add_draw_rect_with_flags(const gfx::Rect& rect,
                                const PaintFlags& flags) {
    client_->add_draw_rect(rect, flags);
  }

  void add_draw_rectf(const gfx::RectF& rect) {
    client_->add_draw_rectf(rect, default_flags_);
  }

  void add_draw_rectf_with_flags(const gfx::RectF& rect,
                                 const PaintFlags& flags) {
    client_->add_draw_rectf(rect, flags);
  }

  void add_draw_image(sk_sp<SkImage> image, const gfx::Point& point) {
    client_->add_draw_image(std::move(image), point, SkSamplingOptions(),
                            default_flags_);
  }
  void add_draw_image(PaintImage image, const gfx::Point& point) {
    client_->add_draw_image(std::move(image), point, SkSamplingOptions(),
                            default_flags_);
  }

  void add_draw_image_with_transform(PaintImage image,
                                     const gfx::Transform& transform) {
    client_->add_draw_image_with_transform(std::move(image), transform,
                                           SkSamplingOptions(), default_flags_);
  }

  void add_draw_image_with_flags(sk_sp<SkImage> image,
                                 const gfx::Point& point,
                                 const SkSamplingOptions& sampling,
                                 const PaintFlags& flags) {
    client_->add_draw_image(std::move(image), point, sampling, flags);
  }

  void set_default_flags(const PaintFlags& flags) { default_flags_ = flags; }

  void reset_draws() {
    client_.emplace();
    client_->set_bounds(size_);
  }

  void SetRecordingScaleFactor(float recording_scale_factor) {
    recording_scale_factor_ = recording_scale_factor;
  }

 private:
  std::optional<FakeContentLayerClient> client_;
  PaintFlags default_flags_;
};

inline FakeRecordingSource::FakeRecordingSource(const gfx::Size& layer_bounds) {
  client_.emplace();
  SetCanUseRecordedBounds(true);
  SetLayerBounds(layer_bounds);
}

inline FakeRecordingSource::~FakeRecordingSource() = default;

}  // namespace cc

#endif  // CC_TEST_FAKE_RECORDING_SOURCE_H_
