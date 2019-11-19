// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
#define CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "cc/layers/content_layer_client.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"

namespace cc {

class FakeContentLayerClient : public ContentLayerClient {
 public:
  struct ImageData {
    ImageData(PaintImage image,
              const gfx::Point& point,
              const PaintFlags& flags);
    ImageData(PaintImage image,
              const gfx::Transform& transform,
              const PaintFlags& flags);
    ImageData(const ImageData& other);
    ~ImageData();
    PaintImage image;
    gfx::Point point;
    gfx::Transform transform;
    PaintFlags flags;
  };

  FakeContentLayerClient();
  ~FakeContentLayerClient() override;

  gfx::Rect PaintableRegion() override;
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_control) override;
  bool FillsBoundsCompletely() const override;
  size_t GetApproximateUnsharedMemoryUsage() const override;

  void set_fill_with_nonsolid_color(bool nonsolid) {
    fill_with_nonsolid_color_ = nonsolid;
  }

  void set_contains_slow_paths(bool contains_slow_paths) {
    contains_slow_paths_ = contains_slow_paths;
  }

  void set_has_non_aa_paint(bool has_non_aa_paint) {
    has_non_aa_paint_ = has_non_aa_paint;
  }

  void add_draw_rect(const gfx::Rect& rect, const PaintFlags& flags) {
    draw_rects_.push_back(std::make_pair(gfx::RectF(rect), flags));
  }

  void add_draw_rectf(const gfx::RectF& rect, const PaintFlags& flags) {
    draw_rects_.push_back(std::make_pair(rect, flags));
  }

  void add_draw_image(sk_sp<SkImage> image,
                      const gfx::Point& point,
                      const PaintFlags& flags) {
    add_draw_image(
        PaintImageBuilder::WithDefault()
            .set_id(PaintImage::GetNextId())
            .set_image(std::move(image), PaintImage::GetNextContentId())
            .TakePaintImage(),
        point, flags);
  }
  void add_draw_image(PaintImage image,
                      const gfx::Point& point,
                      const PaintFlags& flags) {
    ImageData data(std::move(image), point, flags);
    draw_images_.push_back(data);
  }

  void add_draw_image_with_transform(PaintImage image,
                                     const gfx::Transform& transform,
                                     const PaintFlags& flags) {
    ImageData data(std::move(image), transform, flags);
    draw_images_.push_back(data);
  }

  SkCanvas* last_canvas() const { return last_canvas_; }

  PaintingControlSetting last_painting_control() const {
    return last_painting_control_;
  }

  void set_reported_memory_usage(size_t reported_memory_usage) {
    reported_memory_usage_ = reported_memory_usage;
  }

  void set_bounds(gfx::Size bounds) {
    bounds_ = bounds;
    bounds_set_ = true;
  }

 private:
  using RectPaintVector = std::vector<std::pair<gfx::RectF, PaintFlags>>;
  using ImageVector = std::vector<ImageData>;

  bool fill_with_nonsolid_color_ = false;
  RectPaintVector draw_rects_;
  ImageVector draw_images_;
  SkCanvas* last_canvas_ = nullptr;
  PaintingControlSetting last_painting_control_ = PAINTING_BEHAVIOR_NORMAL;
  size_t reported_memory_usage_ = 0;
  gfx::Size bounds_;
  bool bounds_set_ = false;
  bool contains_slow_paths_ = false;
  bool has_non_aa_paint_ = false;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
