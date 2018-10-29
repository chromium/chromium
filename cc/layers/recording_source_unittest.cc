// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "cc/base/region.h"
#include "cc/raster/raster_source.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
namespace {

gfx::ColorSpace DefaultColorSpace() {
  return gfx::ColorSpace::CreateSRGB();
}

std::unique_ptr<FakeRecordingSource> CreateRecordingSource(
    const gfx::Rect& viewport) {
  gfx::Rect layer_rect(viewport.right(), viewport.bottom());
  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateRecordingSource(viewport, layer_rect.size());
  return recording_source;
}

TEST(RecordingSourceTest, DiscardableImagesWithTransform) {
  gfx::Rect recorded_viewport(256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(
          recorded_viewport.size());
  PaintImage discardable_image[2][2];
  gfx::Transform identity_transform;
  discardable_image[0][0] = CreateDiscardablePaintImage(gfx::Size(32, 32));
  // Translate transform is equivalent to moving using point.
  gfx::Transform translate_transform;
  translate_transform.Translate(0, 130);
  discardable_image[1][0] = CreateDiscardablePaintImage(gfx::Size(32, 32));
  // This moves the bitmap to center of viewport and rotate, this would make
  // this bitmap in all four tile grids.
  gfx::Transform rotate_transform;
  rotate_transform.Translate(112, 112);
  rotate_transform.Rotate(45);
  discardable_image[1][1] = CreateDiscardablePaintImage(gfx::Size(32, 32));

  gfx::RectF rect(0, 0, 32, 32);
  gfx::RectF translate_rect = rect;
  translate_transform.TransformRect(&translate_rect);
  gfx::RectF rotate_rect = rect;
  rotate_transform.TransformRect(&rotate_rect);

  recording_source->add_draw_image_with_transform(discardable_image[0][0],
                                                  identity_transform);
  recording_source->add_draw_image_with_transform(discardable_image[1][0],
                                                  translate_transform);
  recording_source->add_draw_image_with_transform(discardable_image[1][1],
                                                  rotate_transform);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  // Tile sized iterators. These should find only one pixel ref.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 128, 128),
                                              &images);
    EXPECT_EQ(2u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[0][0]);
    EXPECT_TRUE(images[1]->paint_image() == discardable_image[1][1]);
  }

  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(130, 140, 128, 128),
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[1][1]);
  }

  // The rotated bitmap would still be in the top right tile.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(130, 0, 128, 128),
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[1][1]);
  }

  // Layer sized iterators. These should find all pixel refs.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256),
                                              &images);
    EXPECT_EQ(3u, images.size());
    // Top left tile with bitmap[0][0] and bitmap[1][1].
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[0][0]);
    EXPECT_TRUE(images[1]->paint_image() == discardable_image[1][0]);
    EXPECT_TRUE(images[2]->paint_image() == discardable_image[1][1]);
  }

  // Verify different raster scales
  for (float scale = 1.f; scale <= 5.f; scale += 0.5f) {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(130, 0, 128, 128),
                                              &images);
    DrawImage image(*images[0], scale, PaintImage::kDefaultFrameIndex,
                    DefaultColorSpace());
    EXPECT_EQ(1u, images.size());
    EXPECT_FLOAT_EQ(scale, image.scale().width());
    EXPECT_FLOAT_EQ(scale, image.scale().height());
  }
}

TEST(RecordingSourceTest, EmptyImages) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  // Tile sized iterators.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 128, 128),
                                              &images);
    EXPECT_TRUE(images.empty());
  }
  // Shifted tile sized iterators.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(140, 140, 128, 128),
                                              &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256),
                                              &images);
    EXPECT_TRUE(images.empty());
  }
}

TEST(RecordingSourceTest, NoDiscardableImages) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);

  PaintFlags simple_flags;
  simple_flags.setColor(SkColorSetARGB(255, 12, 23, 34));

  auto non_discardable_image =
      CreateNonDiscardablePaintImage(gfx::Size(128, 128));
  recording_source->add_draw_rect_with_flags(gfx::Rect(0, 0, 256, 256),
                                             simple_flags);
  recording_source->add_draw_rect_with_flags(gfx::Rect(128, 128, 512, 512),
                                             simple_flags);
  recording_source->add_draw_rect_with_flags(gfx::Rect(512, 0, 256, 256),
                                             simple_flags);
  recording_source->add_draw_rect_with_flags(gfx::Rect(0, 512, 256, 256),
                                             simple_flags);
  recording_source->add_draw_image(non_discardable_image, gfx::Point(128, 0));
  recording_source->add_draw_image(non_discardable_image, gfx::Point(0, 128));
  recording_source->add_draw_image(non_discardable_image, gfx::Point(150, 150));
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  // Tile sized iterators.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 128, 128),
                                              &images);
    EXPECT_TRUE(images.empty());
  }
  // Shifted tile sized iterators.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(140, 140, 128, 128),
                                              &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256),
                                              &images);
    EXPECT_TRUE(images.empty());
  }
}

TEST(RecordingSourceTest, DiscardableImages) {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);

  PaintImage discardable_image[2][2];
  discardable_image[0][0] = CreateDiscardablePaintImage(gfx::Size(32, 32));
  discardable_image[1][0] = CreateDiscardablePaintImage(gfx::Size(32, 32));
  discardable_image[1][1] = CreateDiscardablePaintImage(gfx::Size(32, 32));

  // Discardable images are found in the following cells:
  // |---|---|
  // | x |   |
  // |---|---|
  // | x | x |
  // |---|---|
  recording_source->add_draw_image(discardable_image[0][0], gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[1][0], gfx::Point(0, 130));
  recording_source->add_draw_image(discardable_image[1][1],
                                   gfx::Point(140, 140));
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  // Tile sized iterators. These should find only one image.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 128, 128),
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[0][0]);
  }

  // Shifted tile sized iterators. These should find only one image.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(140, 140, 128, 128),
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[1][1]);
  }

  // Ensure there's no discardable images in the empty cell
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(140, 0, 128, 128),
                                              &images);
    EXPECT_TRUE(images.empty());
  }

  // Layer sized iterators. These should find all 3 images.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256),
                                              &images);
    EXPECT_EQ(3u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[0][0]);
    EXPECT_TRUE(images[1]->paint_image() == discardable_image[1][0]);
    EXPECT_TRUE(images[2]->paint_image() == discardable_image[1][1]);
  }
}

TEST(RecordingSourceTest, DiscardableImagesBaseNonDiscardable) {
  gfx::Rect recorded_viewport(0, 0, 512, 512);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);
  PaintImage non_discardable_image =
      CreateNonDiscardablePaintImage(gfx::Size(512, 512));

  PaintImage discardable_image[2][2];
  discardable_image[0][0] = CreateDiscardablePaintImage(gfx::Size(128, 128));
  discardable_image[0][1] = CreateDiscardablePaintImage(gfx::Size(128, 128));
  discardable_image[1][1] = CreateDiscardablePaintImage(gfx::Size(128, 128));

  // One large non-discardable image covers the whole grid.
  // Discardable images are found in the following cells:
  // |---|---|
  // | x | x |
  // |---|---|
  // |   | x |
  // |---|---|
  recording_source->add_draw_image(non_discardable_image, gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[0][0], gfx::Point(0, 0));
  recording_source->add_draw_image(discardable_image[0][1], gfx::Point(260, 0));
  recording_source->add_draw_image(discardable_image[1][1],
                                   gfx::Point(260, 260));
  recording_source->Rerecord();

  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  // Tile sized iterators. These should find only one image.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 256, 256),
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[0][0]);
  }
  // Shifted tile sized iterators. These should find only one image.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(260, 260, 256, 256),
                                              &images);
    EXPECT_EQ(1u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[1][1]);
  }
  // Ensure there's no discardable images in the empty cell
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 256, 256, 256),
                                              &images);
    EXPECT_TRUE(images.empty());
  }
  // Layer sized iterators. These should find three images.
  {
    std::vector<const DrawImage*> images;
    raster_source->GetDiscardableImagesInRect(gfx::Rect(0, 0, 512, 512),
                                              &images);
    EXPECT_EQ(3u, images.size());
    EXPECT_TRUE(images[0]->paint_image() == discardable_image[0][0]);
    EXPECT_TRUE(images[1]->paint_image() == discardable_image[0][1]);
    EXPECT_TRUE(images[2]->paint_image() == discardable_image[1][1]);
  }
}

TEST(RecordingSourceTest, AnalyzeIsSolid) {
  gfx::Size layer_bounds(400, 400);
  const std::vector<float> recording_scales = {1.f,   1.25f, 1.33f, 1.5f, 1.6f,
                                               1.66f, 2.f,   2.25f, 2.5f};
  for (float recording_scale : recording_scales) {
    std::unique_ptr<FakeRecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(layer_bounds);
    recording_source->SetRecordingScaleFactor(recording_scale);

    PaintFlags solid_flags;
    SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
    solid_flags.setColor(solid_color);

    SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
    PaintFlags non_solid_flags;
    non_solid_flags.setColor(non_solid_color);

    recording_source->add_draw_rect_with_flags(
        gfx::ScaleToEnclosingRect(gfx::Rect(layer_bounds), recording_scale),
        solid_flags);
    recording_source->Rerecord();

    scoped_refptr<RasterSource> raster = recording_source->CreateRasterSource();

    EXPECT_TRUE(raster->IsSolidColor())
        << " recording scale: " << recording_scale;
    EXPECT_EQ(raster->GetSolidColor(), solid_color);

    for (int y = 0; y < layer_bounds.height(); y += 50) {
      for (int x = 0; x < layer_bounds.width(); x += 50) {
        recording_source->reset_draws();
        recording_source->add_draw_rect_with_flags(
            gfx::ScaleToEnclosingRect(gfx::Rect(layer_bounds), recording_scale),
            solid_flags);
        recording_source->add_draw_rect_with_flags(
            gfx::Rect(std::round(x * recording_scale),
                      std::round(y * recording_scale), 1, 1),
            non_solid_flags);
        recording_source->Rerecord();
        raster = recording_source->CreateRasterSource();
        EXPECT_FALSE(raster->IsSolidColor())
            << " recording scale: " << recording_scale << " pixel at: (" << x
            << ", " << y << ") was not solid.";
      }
    }
  }
}

}  // namespace
}  // namespace cc
