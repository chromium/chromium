// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
#define CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/layers/content_layer_client.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {

class FakeContentLayerClient : public ContentLayerClient {
 public:
  struct ImageData {
    ImageData(PaintImage image,
              const gfx::Point& point,
              const SkSamplingOptions&,
              const PaintFlags& flags);
    ImageData(PaintImage image,
              const gfx::Transform& transform,
              const SkSamplingOptions&,
              const PaintFlags& flags);
    ImageData(const ImageData& other);
    ~ImageData();
    PaintImage image;
    gfx::Point point;
    gfx::Transform transform;
    SkSamplingOptions sampling;
    PaintFlags flags;
  };

  struct SkottieData {
    SkottieData(scoped_refptr<SkottieWrapper> skottie,
                const gfx::Rect& dst,
                float t,
                SkottieFrameDataMap images,
                SkottieColorMap color_map,
                SkottieTextPropertyValueMap text_map);
    SkottieData(const SkottieData& other);
    SkottieData& operator=(const SkottieData& other);
    ~SkottieData();

    scoped_refptr<SkottieWrapper> skottie;
    gfx::Rect dst;
    float t;
    SkottieFrameDataMap images;
    SkottieColorMap color_map;
    SkottieTextPropertyValueMap text_map;
  };

  FakeContentLayerClient();
  ~FakeContentLayerClient() override;

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override;
  bool FillsBoundsCompletely() const override;

  void set_fill_with_nonsolid_color(bool nonsolid) {
    fill_with_nonsolid_color_ = nonsolid;
  }

  void set_contains_slow_paths(bool contains_slow_paths) {
    contains_slow_paths_ = contains_slow_paths;
  }

  void set_has_non_aa_paint(bool has_non_aa_paint) {
    has_non_aa_paint_ = has_non_aa_paint;
  }

  void set_has_draw_text_op() { has_draw_text_op_ = true; }

  void add_draw_rect(const gfx::Rect& rect, const PaintFlags& flags) {
    draw_rects_.push_back(std::make_pair(gfx::RectF(rect), flags));
  }

  void add_draw_rectf(const gfx::RectF& rect, const PaintFlags& flags) {
    draw_rects_.push_back(std::make_pair(rect, flags));
  }

  void add_draw_image(sk_sp<SkImage> image,
                      const gfx::Point& point,
                      const SkSamplingOptions& sampling,
                      const PaintFlags& flags) {
    add_draw_image(
        PaintImageBuilder::WithDefault()
            .set_id(PaintImage::GetNextId())
            .set_image(std::move(image), PaintImage::GetNextContentId())
            .TakePaintImage(),
        point, sampling, flags);
  }
  void add_draw_image(sk_sp<SkImage> image, const gfx::Point& point) {
    add_draw_image(
        PaintImageBuilder::WithDefault()
            .set_id(PaintImage::GetNextId())
            .set_image(std::move(image), PaintImage::GetNextContentId())
            .TakePaintImage(),
        point, SkSamplingOptions(), PaintFlags());
  }
  void add_draw_image(PaintImage image,
                      const gfx::Point& point,
                      const SkSamplingOptions& sampling,
                      const PaintFlags& flags) {
    ImageData data(std::move(image), point, sampling, flags);
    draw_images_.push_back(data);
  }
  void add_draw_image(PaintImage image, const gfx::Point& point) {
    ImageData data(std::move(image), point, SkSamplingOptions(), PaintFlags());
    draw_images_.push_back(data);
  }

  void add_draw_image_with_transform(PaintImage image,
                                     const gfx::Transform& transform,
                                     const SkSamplingOptions& sampling,
                                     const PaintFlags& flags) {
    ImageData data(std::move(image), transform, sampling, flags);
    draw_images_.push_back(data);
  }

  void add_draw_skottie(SkottieData skottie_data) {
    skottie_data_.push_back(std::move(skottie_data));
  }

  // PaintContentsToDisplayList() will return this display_list instead of
  // drawing the rects / images.
  void set_display_item_list(scoped_refptr<DisplayItemList> display_list) {
    display_list_ = std::move(display_list);
  }

  SkCanvas* last_canvas() const { return last_canvas_; }

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
  raw_ptr<SkCanvas> last_canvas_ = nullptr;
  gfx::Size bounds_;
  bool bounds_set_ = false;
  bool contains_slow_paths_ = false;
  bool has_non_aa_paint_ = false;
  bool has_draw_text_op_ = false;
  std::vector<SkottieData> skottie_data_;
  scoped_refptr<DisplayItemList> display_list_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
