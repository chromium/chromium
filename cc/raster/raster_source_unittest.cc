// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/raster/raster_source.h"

#include <stddef.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "cc/raster/playback_image_provider.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "cc/test/test_skcanvas.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"

using ::testing::_;
using ::testing::StrictMock;
using ::testing::Sequence;

namespace cc {
namespace {

struct Color {
  SkColor color;
  bool operator==(const Color& other) const { return color == other.color; }
};
std::ostream& operator<<(std::ostream& os, const Color& color) {
  return os << base::StringPrintf("#%08x", color.color);
}
#define EXPECT_COLOR_EQ(a, b) EXPECT_EQ(Color{a}, Color{b})

TEST(RasterSourceTest, AnalyzeIsSolidUnscaled) {
  gfx::Size layer_bounds(400, 400);

  FakeRecordingSource recording_source(layer_bounds);

  PaintFlags solid_flags;
  SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
  solid_flags.setColor(solid_color);

  SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
  SkColor4f color = SkColors::kTransparent;
  PaintFlags non_solid_flags;
  bool is_solid_color = false;
  non_solid_flags.setColor(non_solid_color);

  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            solid_flags);
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();

  // Ensure everything is solid.
  for (int y = 0; y <= 300; y += 100) {
    for (int x = 0; x <= 300; x += 100) {
      gfx::Rect rect(x, y, 100, 100);
      is_solid_color = raster->PerformSolidColorAnalysis(rect, &color);
      EXPECT_TRUE(is_solid_color) << rect.ToString();
      EXPECT_COLOR_EQ(solid_color, color.toSkColor()) << rect.ToString();
    }
  }

  // Add one non-solid pixel and recreate the raster source.
  recording_source.add_draw_rect_with_flags(gfx::Rect(50, 50, 1, 1),
                                            non_solid_flags);
  recording_source.Rerecord();
  raster = recording_source.CreateRasterSource();

  color = SkColors::kTransparent;
  is_solid_color =
      raster->PerformSolidColorAnalysis(gfx::Rect(0, 0, 100, 100), &color);
  EXPECT_FALSE(is_solid_color);

  color = SkColors::kTransparent;
  is_solid_color =
      raster->PerformSolidColorAnalysis(gfx::Rect(100, 0, 100, 100), &color);
  EXPECT_TRUE(is_solid_color);
  EXPECT_COLOR_EQ(solid_color, color.toSkColor());

  // Boundaries should be clipped.
  color = SkColors::kTransparent;
  is_solid_color =
      raster->PerformSolidColorAnalysis(gfx::Rect(350, 0, 100, 100), &color);
  EXPECT_TRUE(is_solid_color);
  EXPECT_COLOR_EQ(solid_color, color.toSkColor());

  color = SkColors::kTransparent;
  is_solid_color =
      raster->PerformSolidColorAnalysis(gfx::Rect(0, 350, 100, 100), &color);
  EXPECT_TRUE(is_solid_color);
  EXPECT_COLOR_EQ(solid_color, color.toSkColor());

  color = SkColors::kTransparent;
  is_solid_color =
      raster->PerformSolidColorAnalysis(gfx::Rect(350, 350, 100, 100), &color);
  EXPECT_TRUE(is_solid_color);
  EXPECT_COLOR_EQ(solid_color, color.toSkColor());
}

TEST(RasterSourceTest, AnalyzeIsSolidScaled) {
  gfx::Size layer_bounds(400, 400);
  const std::vector<float> recording_scales = {1.25f, 1.33f, 1.5f,  1.6f,
                                               1.66f, 2.f,   2.25f, 2.5f};
  for (float recording_scale : recording_scales) {
    FakeRecordingSource recording_source(layer_bounds);
    recording_source.SetRecordingScaleFactor(recording_scale);

    PaintFlags solid_flags;
    SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
    solid_flags.setColor(solid_color);

    SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
    SkColor4f color = SkColors::kTransparent;
    PaintFlags non_solid_flags;
    bool is_solid_color = false;
    non_solid_flags.setColor(non_solid_color);

    recording_source.add_draw_rect_with_flags(
        gfx::ScaleToEnclosingRect(gfx::Rect(layer_bounds), recording_scale),
        solid_flags);
    recording_source.Rerecord();

    scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();

    // Ensure everything is solid.
    for (int y = 0; y <= 300; y += 100) {
      for (int x = 0; x <= 300; x += 100) {
        gfx::Rect rect(x, y, 100, 100);
        is_solid_color = raster->PerformSolidColorAnalysis(rect, &color);
        EXPECT_TRUE(is_solid_color)
            << rect.ToString() << " recording_scale: " << recording_scale;
        EXPECT_COLOR_EQ(solid_color, color.toSkColor())
            << rect.ToString() << " recording_scale: " << recording_scale;
      }
    }

    // Add one non-solid pixel and recreate the raster source.
    recording_source.add_draw_rect_with_flags(
        gfx::Rect(std::round(50 * recording_scale),
                  std::round(50 * recording_scale), 1, 1),
        non_solid_flags);
    recording_source.Rerecord();
    raster = recording_source.CreateRasterSource();

    color = SkColors::kTransparent;
    is_solid_color =
        raster->PerformSolidColorAnalysis(gfx::Rect(0, 0, 100, 100), &color);
    EXPECT_FALSE(is_solid_color) << " recording_scale: " << recording_scale;

    color = SkColors::kTransparent;
    is_solid_color =
        raster->PerformSolidColorAnalysis(gfx::Rect(0, 0, 51, 51), &color);
    EXPECT_FALSE(is_solid_color) << " recording_scale: " << recording_scale;

    color = SkColors::kTransparent;
    is_solid_color =
        raster->PerformSolidColorAnalysis(gfx::Rect(51, 0, 100, 100), &color);
    EXPECT_TRUE(is_solid_color) << " recording_scale: " << recording_scale;
    EXPECT_COLOR_EQ(solid_color, color.toSkColor())
        << " recording_scale: " << recording_scale;

    // Boundaries should be clipped.
    color = SkColors::kTransparent;
    is_solid_color =
        raster->PerformSolidColorAnalysis(gfx::Rect(350, 0, 100, 100), &color);
    EXPECT_TRUE(is_solid_color) << " recording_scale: " << recording_scale;
    EXPECT_COLOR_EQ(solid_color, color.toSkColor())
        << " recording_scale: " << recording_scale;

    color = SkColors::kTransparent;
    is_solid_color =
        raster->PerformSolidColorAnalysis(gfx::Rect(0, 350, 100, 100), &color);
    EXPECT_TRUE(is_solid_color) << " recording_scale: " << recording_scale;
    EXPECT_COLOR_EQ(solid_color, color.toSkColor())
        << " recording_scale: " << recording_scale;

    color = SkColors::kTransparent;
    is_solid_color = raster->PerformSolidColorAnalysis(
        gfx::Rect(350, 350, 100, 100), &color);
    EXPECT_TRUE(is_solid_color) << " recording_scale: " << recording_scale;
    EXPECT_COLOR_EQ(solid_color, color.toSkColor())
        << " recording_scale: " << recording_scale;
  }
}

TEST(RasterSourceTest, PixelRefIteratorDiscardableRefsOneTile) {
  gfx::Size layer_bounds(512, 512);

  FakeRecordingSource recording_source(layer_bounds);

  PaintImage discardable_image[2][2];
  discardable_image[0][0] = CreateDiscardablePaintImage(gfx::Size(32, 32));
  discardable_image[0][1] = CreateDiscardablePaintImage(gfx::Size(32, 32));
  discardable_image[1][1] = CreateDiscardablePaintImage(gfx::Size(32, 32));

  // Discardable pixel refs are found in the following cells:
  // |---|---|
  // | x | x |
  // |---|---|
  // |   | x |
  // |---|---|
  recording_source.add_draw_image(discardable_image[0][0], gfx::Point(0, 0));
  recording_source.add_draw_image(discardable_image[0][1], gfx::Point(260, 0));
  recording_source.add_draw_image(discardable_image[1][1],
                                  gfx::Point(260, 260));
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();
  scoped_refptr<DiscardableImageMap> image_map =
      raster->GetDisplayItemList()->GenerateDiscardableImageMap(
          ScrollOffsetMap());

  // Tile sized iterators. These should find only one pixel ref.
  {
    TargetColorParams target_color_params;
    std::vector<const DrawImage*> images =
        image_map->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256));
    ASSERT_EQ(1u, images.size());
    DrawImage image(*images[0], 1.f, PaintImage::kDefaultFrameIndex,
                    target_color_params);
    EXPECT_TRUE(
        discardable_image[0][0].IsSameForTesting(images[0]->paint_image()));
    EXPECT_EQ(target_color_params.color_space, image.target_color_space());
  }
  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    TargetColorParams target_color_params;
    target_color_params.color_space = gfx::ColorSpace::CreateXYZD50();
    std::vector<const DrawImage*> images =
        image_map->GetDiscardableImagesInRect(gfx::Rect(260, 260, 256, 256));
    ASSERT_EQ(1u, images.size());
    DrawImage image(*images[0], 1.f, PaintImage::kDefaultFrameIndex,
                    target_color_params);
    EXPECT_TRUE(
        discardable_image[1][1].IsSameForTesting(images[0]->paint_image()));
    EXPECT_EQ(target_color_params.color_space, image.target_color_space());
  }
  // Ensure there's no discardable pixel refs in the empty cell
  {
    std::vector<const DrawImage*> images =
        image_map->GetDiscardableImagesInRect(gfx::Rect(0, 256, 256, 256));
    EXPECT_EQ(0u, images.size());
  }
  // Layer sized iterators. These should find three pixel ref.
  {
    std::vector<const DrawImage*> images =
        image_map->GetDiscardableImagesInRect(gfx::Rect(0, 0, 512, 512));
    ASSERT_EQ(3u, images.size());
    EXPECT_TRUE(
        discardable_image[0][0].IsSameForTesting(images[0]->paint_image()));
    EXPECT_TRUE(
        discardable_image[0][1].IsSameForTesting(images[1]->paint_image()));
    EXPECT_TRUE(
        discardable_image[1][1].IsSameForTesting(images[2]->paint_image()));
  }
}

TEST(RasterSourceTest, RasterFullContents) {
  gfx::Size layer_bounds(3, 5);
  float contents_scale = 1.5f;
  float raster_divisions = 2.f;

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.SetBackgroundColor(SkColors::kBlack);

  // Because the caller sets content opaque, it also promises that it
  // has at least filled in layer_bounds opaquely.
  PaintFlags white_flags;
  white_flags.setColor(SK_ColorWHITE);
  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            white_flags);
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();

  gfx::Size content_bounds(
      gfx::ScaleToCeiledSize(layer_bounds, contents_scale));

  // Simulate drawing into different tiles at different offsets.
  int step_x = std::ceil(content_bounds.width() / raster_divisions);
  int step_y = std::ceil(content_bounds.height() / raster_divisions);
  for (int offset_x = 0; offset_x < content_bounds.width();
       offset_x += step_x) {
    for (int offset_y = 0; offset_y < content_bounds.height();
         offset_y += step_y) {
      gfx::Rect content_rect(offset_x, offset_y, step_x, step_y);
      content_rect.Intersect(gfx::Rect(content_bounds));

      // Simulate a canvas rect larger than the content rect.  Every pixel
      // up to one pixel outside the content rect is guaranteed to be opaque.
      // Outside of that is undefined.
      gfx::Rect canvas_rect(content_rect);
      canvas_rect.Inset(gfx::Insets::TLBR(0, 0, -1, -1));

      SkBitmap bitmap;
      bitmap.allocN32Pixels(canvas_rect.width(), canvas_rect.height());
      SkCanvas canvas(bitmap, SkSurfaceProps{});
      canvas.clear(SkColors::kTransparent);

      raster->PlaybackToCanvas(
          &canvas, content_bounds, canvas_rect, canvas_rect,
          gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()),
          RasterSource::PlaybackSettings());

      bool all_white = true;
      for (int i = 0; i < bitmap.width(); i++) {
        for (int j = 0; j < bitmap.height(); j++) {
          SkColor pixel = bitmap.getColor(i, j);
          EXPECT_EQ(SkColorGetA(pixel), 255u)
              << offset_x + i << "," << offset_y + j;
          all_white &= (pixel == SK_ColorWHITE);
        }
      }

      // If the canvas doesn't extend past the edge of the content,
      // it should be entirely white. Otherwise, the edge of the content
      // will be non-white.
      EXPECT_EQ(all_white, gfx::Rect(content_bounds).Contains(canvas_rect));
    }
  }
}

TEST(RasterSourceTest, RasterFullContentsWithRasterTranslation) {
  gfx::Size layer_bounds(3, 5);
  float raster_divisions = 2.f;

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.SetBackgroundColor(SkColors::kBlack);

  // Because the caller sets content opaque, it also promises that it
  // has at least filled in layer_bounds opaquely.
  PaintFlags white_flags;
  white_flags.setColor(SK_ColorWHITE);
  white_flags.setAntiAlias(true);
  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            white_flags);
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();

  gfx::Size content_bounds = layer_bounds;

  // Simulate drawing into different tiles at different offsets.
  int step_x = std::ceil(content_bounds.width() / raster_divisions);
  int step_y = std::ceil(content_bounds.height() / raster_divisions);
  for (int offset_x = 0; offset_x < content_bounds.width();
       offset_x += step_x) {
    for (int offset_y = 0; offset_y < content_bounds.height();
         offset_y += step_y) {
      gfx::Rect content_rect(offset_x, offset_y, step_x, step_y);
      content_rect.Intersect(gfx::Rect(content_bounds));

      gfx::Rect canvas_rect = content_rect;

      SkBitmap bitmap;
      bitmap.allocN32Pixels(canvas_rect.width(), canvas_rect.height());
      SkCanvas canvas(bitmap, SkSurfaceProps{});
      canvas.clear(SkColors::kTransparent);

      raster->PlaybackToCanvas(
          &canvas, content_bounds, canvas_rect, canvas_rect,
          gfx::AxisTransform2d(1.0f, gfx::Vector2dF(0.3f, 0.7f)),
          RasterSource::PlaybackSettings());

      for (int i = 0; i < bitmap.width(); i++) {
        for (int j = 0; j < bitmap.height(); j++) {
          SkColor pixel = bitmap.getColor(i, j);
          int content_x = offset_x + i;
          int content_y = offset_y + j;
          EXPECT_EQ(255u, SkColorGetA(pixel)) << content_x << "," << content_y;
          // Pixels on the top and left edges of the content are blended with
          // the background.
          bool expect_white = content_x && content_y;
          EXPECT_EQ(expect_white, pixel == SK_ColorWHITE)
              << content_x << "," << content_y;
        }
      }
    }
  }
}

TEST(RasterSourceTest, RasterPartialContents) {
  gfx::Size layer_bounds(3, 5);
  float contents_scale = 1.5f;

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.SetBackgroundColor(SkColors::kGreen);

  // First record everything as white.
  PaintFlags white_flags;
  white_flags.setColor(SK_ColorWHITE);
  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            white_flags);
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();

  gfx::Size content_bounds(
      gfx::ScaleToCeiledSize(layer_bounds, contents_scale));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(content_bounds.width(), content_bounds.height());
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.clear(SK_ColorTRANSPARENT);

  // Playback the full rect which should make everything white.
  gfx::Rect raster_full_rect(content_bounds);
  gfx::Rect playback_rect(content_bounds);
  raster->PlaybackToCanvas(
      &canvas, content_bounds, raster_full_rect, playback_rect,
      gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()),
      RasterSource::PlaybackSettings());

  for (int i = 0; i < bitmap.width(); ++i) {
    for (int j = 0; j < bitmap.height(); ++j)
      EXPECT_COLOR_EQ(SK_ColorWHITE, bitmap.getColor(i, j)) << i << "," << j;
  }

  // Re-record everything as black.
  PaintFlags black_flags;
  black_flags.setColor(SK_ColorBLACK);
  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            black_flags);
  recording_source.Rerecord();

  // Make a new RasterSource from the new recording.
  raster = recording_source.CreateRasterSource();

  // We're going to playback from "everything is black" into a smaller area,
  // that touches the edge pixels of the recording.
  playback_rect.Inset(gfx::Insets::TLBR(2, 1, 1, 0));
  raster->PlaybackToCanvas(
      &canvas, content_bounds, raster_full_rect, playback_rect,
      gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()),
      RasterSource::PlaybackSettings());

  int num_black = 0;
  int num_white = 0;
  for (int i = 0; i < bitmap.width(); ++i) {
    for (int j = 0; j < bitmap.height(); ++j) {
      SkColor pixel = bitmap.getColor(i, j);
      bool expect_black = playback_rect.Contains(i, j);
      if (expect_black) {
        EXPECT_COLOR_EQ(SK_ColorBLACK, pixel) << i << "," << j;
        ++num_black;
      } else {
        EXPECT_COLOR_EQ(SK_ColorWHITE, pixel) << i << "," << j;
        ++num_white;
      }
    }
  }
  EXPECT_GT(num_black, 0);
  EXPECT_GT(num_white, 0);
}

TEST(RasterSourceTest, RasterPartialContentsWithRasterTranslation) {
  gfx::Size layer_bounds(3, 5);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.SetBackgroundColor(SkColors::kGreen);

  // First record everything as white.
  PaintFlags white_flags;
  white_flags.setAntiAlias(true);
  white_flags.setColor(SK_ColorWHITE);
  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            white_flags);
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();

  gfx::Size content_bounds = layer_bounds;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(content_bounds.width(), content_bounds.height());
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.clear(SK_ColorTRANSPARENT);

  // Playback the full rect which should make everything white.
  gfx::Rect raster_full_rect(content_bounds);
  gfx::Rect playback_rect(content_bounds);
  raster->PlaybackToCanvas(
      &canvas, content_bounds, raster_full_rect, playback_rect,
      gfx::AxisTransform2d(1.0f, gfx::Vector2dF(0.3f, 0.7f)),
      RasterSource::PlaybackSettings());

  for (int i = 0; i < bitmap.width(); ++i) {
    for (int j = 0; j < bitmap.height(); ++j) {
      SkColor pixel = bitmap.getColor(i, j);
      EXPECT_EQ(255u, SkColorGetA(pixel)) << i << "," << j;
      // Pixels on the top and left edges of the content are blended with
      // the background.
      bool expect_white = i && j;
      EXPECT_EQ(expect_white, pixel == SK_ColorWHITE)
          << Color{pixel} << " " << i << "," << j;
    }
  }

  // Re-record everything as black.
  PaintFlags black_flags;
  black_flags.setColor(SK_ColorBLACK);
  black_flags.setAntiAlias(true);
  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            black_flags);
  recording_source.Rerecord();

  // Make a new RasterSource from the new recording.
  raster = recording_source.CreateRasterSource();

  // We're going to playback from "everything is black" into a smaller area,
  // that touches the edge pixels of the recording.
  playback_rect.Inset(gfx::Insets::TLBR(2, 1, 1, 0));
  raster->PlaybackToCanvas(
      &canvas, content_bounds, raster_full_rect, playback_rect,
      gfx::AxisTransform2d(1.0f, gfx::Vector2dF(0.3f, 0.7f)),
      RasterSource::PlaybackSettings());

  int num_black = 0;
  int num_white = 0;
  for (int i = 0; i < bitmap.width(); ++i) {
    for (int j = 0; j < bitmap.height(); ++j) {
      SkColor pixel = bitmap.getColor(i, j);
      EXPECT_EQ(255u, SkColorGetA(pixel)) << i << "," << j;
      bool expect_other = !i || !j;
      bool expect_black = playback_rect.Contains(i, j);
      bool expect_white = !expect_other && !expect_black;
      if (expect_black) {
        EXPECT_COLOR_EQ(SK_ColorBLACK, pixel) << i << "," << j;
        ++num_black;
      } else if (expect_white) {
        EXPECT_COLOR_EQ(SK_ColorWHITE, pixel) << i << "," << j;
        ++num_white;
      } else {
        ASSERT_TRUE(expect_other);
        EXPECT_TRUE(pixel != SK_ColorBLACK && pixel != SK_ColorWHITE)
            << Color{pixel};
      }
    }
  }
  EXPECT_GT(num_black, 0);
  EXPECT_GT(num_white, 0);
}

TEST(RasterSourceTest, RasterPartialClear) {
  gfx::Size layer_bounds(3, 5);
  gfx::Size partial_bounds(2, 4);
  float contents_scale = 1.5f;

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.SetBackgroundColor(SkColors::kGreen);
  recording_source.SetRequiresClear(true);

  // First record everything as white.
  const float alpha_dark = 0.04f;
  PaintFlags white_flags;
  white_flags.setColor(SK_ColorWHITE);
  white_flags.setAlphaf(alpha_dark);
  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            white_flags);
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();

  gfx::Size content_bounds(
      gfx::ScaleToCeiledSize(layer_bounds, contents_scale));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(content_bounds.width(), content_bounds.height());
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.clear(SK_ColorTRANSPARENT);

  // Playback the full rect which should make everything light gray (alpha=10).
  gfx::Rect raster_full_rect(content_bounds);
  gfx::Rect playback_rect(content_bounds);
  raster->PlaybackToCanvas(
      &canvas, content_bounds, raster_full_rect, playback_rect,
      gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()),
      RasterSource::PlaybackSettings());

  SkColor pixel_dark = SkColor4f{1, 1, 1, alpha_dark}.toSkColor();
  for (int i = 0; i < bitmap.width(); ++i) {
    for (int j = 0; j < bitmap.height(); ++j)
      EXPECT_COLOR_EQ(pixel_dark, bitmap.getColor(i, j)) << i << "," << j;
  }

  FakeRecordingSource recording_source_light(layer_bounds);
  recording_source_light.SetBackgroundColor(SkColors::kGreen);
  recording_source_light.SetRequiresClear(true);

  // Record everything as a slightly lighter white.
  const float alpha_light = 0.1f;
  white_flags.setAlphaf(alpha_light);
  recording_source_light.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                                  white_flags);
  recording_source_light.Rerecord();

  // Make a new RasterSource from the new recording.
  raster = recording_source_light.CreateRasterSource();

  // We're going to playback from alpha(18) white rectangle into a smaller area
  // of the recording resulting in a smaller lighter white rectangle over a
  // darker white background rectangle.
  playback_rect =
      gfx::Rect(gfx::ScaleToCeiledSize(partial_bounds, contents_scale));
  raster->PlaybackToCanvas(
      &canvas, content_bounds, raster_full_rect, playback_rect,
      gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()),
      RasterSource::PlaybackSettings());

  // Test that the whole playback_rect was cleared and repainted with new alpha.
  SkColor pixel_light = SkColor4f{1, 1, 1, alpha_light}.toSkColor();
  for (int i = 0; i < playback_rect.width(); ++i) {
    for (int j = 0; j < playback_rect.height(); ++j)
      EXPECT_COLOR_EQ(pixel_light, bitmap.getColor(i, j)) << i << "," << j;
  }
}

TEST(RasterSourceTest, RasterContentsTransparent) {
  gfx::Size layer_bounds(5, 3);
  float contents_scale = 0.5f;

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.SetBackgroundColor(SkColors::kTransparent);
  recording_source.SetRequiresClear(true);
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();
  gfx::Size content_bounds(
      gfx::ScaleToCeiledSize(layer_bounds, contents_scale));

  gfx::Rect canvas_rect(content_bounds);
  canvas_rect.Inset(gfx::Insets::TLBR(0, 0, -1, -1));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(canvas_rect.width(), canvas_rect.height());
  SkCanvas canvas(bitmap, SkSurfaceProps{});

  raster->PlaybackToCanvas(
      &canvas, content_bounds, canvas_rect, canvas_rect,
      gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()),
      RasterSource::PlaybackSettings());

  for (int i = 0; i < bitmap.width(); i++) {
    for (int j = 0; j < bitmap.height(); j++)
      EXPECT_EQ(0u, SkColorGetA(bitmap.getColor(i, j))) << i << "," << j;
  }
}

TEST(RasterSourceTest, RasterTransformWithoutRecordingScale) {
  gfx::Size size(100, 100);
  float recording_scale = 2.f;
  FakeRecordingSource recording_source(size);
  recording_source.Rerecord();
  recording_source.SetRecordingScaleFactor(recording_scale);
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  StrictMock<MockCanvas> mock_canvas;
  Sequence s;

  SkScalar scale = 1.f / recording_scale;

  // The recording source has no ops, so will only do the setup.
  EXPECT_CALL(mock_canvas, willSave()).InSequence(s);
  EXPECT_CALL(mock_canvas, didScale(scale, scale)).InSequence(s);
  EXPECT_CALL(mock_canvas, willRestore()).InSequence(s);

  gfx::Size small_size(50, 50);
  raster_source->PlaybackToCanvas(&mock_canvas, size, gfx::Rect(small_size),
                                  gfx::Rect(small_size), gfx::AxisTransform2d(),
                                  RasterSource::PlaybackSettings());
}

}  // namespace
}  // namespace cc
