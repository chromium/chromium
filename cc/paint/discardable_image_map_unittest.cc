// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/discardable_image_map.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ref_counted.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "cc/base/region.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/paint_image_matchers.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "skia/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {
namespace {
using ::testing::_;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::FloatNear;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using Rects = absl::InlinedVector<gfx::Rect, 1>;

struct PositionScaleDrawImage {
  PaintImage image;
  gfx::Rect image_rect;
  SkSize scale;

  friend void PrintTo(const PositionScaleDrawImage& img, std::ostream* os) {
    *os << "image: <paint image> image_rect: " << img.image_rect.ToString()
        << " scale: " << gfx::SkSizeToSizeF(img.scale).ToString();
  }
};

PaintRecord CreateRecording(const PaintImage& discardable_image,
                            const gfx::Rect& visible_rect) {
  PaintOpBuffer buffer;
  buffer.push<DrawImageOp>(discardable_image, 0.f, 0.f);
  return buffer.ReleaseAsRecord();
}

}  // namespace

class DiscardableImageMapTest : public testing::Test {
 public:
  std::vector<PositionScaleDrawImage> GetDiscardableImagesInRect(
      const DiscardableImageMap& image_map,
      const gfx::Rect& rect) {
    // Choose a not-SRGB-and-not-invalid target color space to verify that it
    // is passed correctly to the resulting DrawImages.
    const TargetColorParams target_color_params(
        gfx::ColorSpace::CreateXYZD50());
    std::vector<const DrawImage*> draw_image_ptrs =
        image_map.GetDiscardableImagesInRect(rect);
    std::vector<DrawImage> draw_images;
    for (const auto* image : draw_image_ptrs)
      draw_images.push_back(DrawImage(
          *image, 1.f, PaintImage::kDefaultFrameIndex, target_color_params));
    std::vector<PositionScaleDrawImage> position_draw_images;
    std::vector<const DrawImage*> results;
    image_map.images_rtree_->Search(rect, &results);
    for (const DrawImage* image : results) {
      auto image_id = image->paint_image().stable_id();
      position_draw_images.push_back(PositionScaleDrawImage{
          image->paint_image(),
          ImageRectsToRegion(image_map.GetRectsForImage(image_id)).bounds(),
          image->scale()});
    }

    EXPECT_EQ(draw_images.size(), position_draw_images.size());
    for (size_t i = 0; i < draw_images.size(); ++i) {
      EXPECT_TRUE(draw_images[i].paint_image().IsSameForTesting(
          position_draw_images[i].image));
      EXPECT_EQ(draw_images[i].target_color_space(),
                target_color_params.color_space);
    }
    return position_draw_images;
  }

  // Note that the image rtree outsets the images by 1, see the comment in
  // DiscardableImageMap::Generator::AddImage.
  std::vector<gfx::Rect> InsetImageRects(
      const std::vector<PositionScaleDrawImage>& images) {
    std::vector<gfx::Rect> result;
    for (auto& image : images) {
      result.push_back(image.image_rect);
      result.back().Inset(1);
    }
    return result;
  }
};

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectTest) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  // Discardable pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  PaintImage discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] =
            CreateDiscardablePaintImage(gfx::Size(500, 500));
        content_layer_client.add_draw_image(
            discardable_image[y][x], gfx::Point(x * 512 + 6, y * 512 + 6));
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          *image_map, gfx::Rect(x * 512, y * 512, 500, 500));
      std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image[y][x]))
            << x << " " << y;
        EXPECT_EQ(gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500),
                  inset_rects[0]);
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }

  // Capture 4 pixel refs.
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(512, 512, 2048, 2048));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(4u, images.size());

  EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image[1][2]));
  EXPECT_EQ(gfx::Rect(2 * 512 + 6, 512 + 6, 500, 500), inset_rects[0]);

  EXPECT_TRUE(images[1].image.IsSameForTesting(discardable_image[2][1]));
  EXPECT_EQ(gfx::Rect(512 + 6, 2 * 512 + 6, 500, 500), inset_rects[1]);

  EXPECT_TRUE(images[2].image.IsSameForTesting(discardable_image[2][3]));
  EXPECT_EQ(gfx::Rect(3 * 512 + 6, 2 * 512 + 6, 500, 500), inset_rects[2]);

  EXPECT_TRUE(images[3].image.IsSameForTesting(discardable_image[3][2]));
  EXPECT_EQ(gfx::Rect(2 * 512 + 6, 3 * 512 + 6, 500, 500), inset_rects[3]);
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectNonZeroLayer) {
  gfx::Rect visible_rect(1024, 0, 2048, 2048);
  // Make sure visible rect fits into the layer size.
  gfx::Size layer_size(visible_rect.right(), visible_rect.bottom());
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(layer_size);

  // Discardable pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  PaintImage discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] =
            CreateDiscardablePaintImage(gfx::Size(500, 500));
        content_layer_client.add_draw_image(
            discardable_image[y][x],
            gfx::Point(1024 + x * 512 + 6, y * 512 + 6));
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          *image_map, gfx::Rect(1024 + x * 512, y * 512, 500, 500));
      std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image[y][x]))
            << x << " " << y;
        EXPECT_EQ(gfx::Rect(1024 + x * 512 + 6, y * 512 + 6, 500, 500),
                  inset_rects[0]);
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }
  // Capture 4 pixel refs.
  {
    std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
        *image_map, gfx::Rect(1024 + 512, 512, 2048, 2048));
    std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
    EXPECT_EQ(4u, images.size());

    EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image[1][2]));
    EXPECT_EQ(gfx::Rect(1024 + 2 * 512 + 6, 512 + 6, 500, 500), inset_rects[0]);

    EXPECT_TRUE(images[1].image.IsSameForTesting(discardable_image[2][1]));
    EXPECT_EQ(gfx::Rect(1024 + 512 + 6, 2 * 512 + 6, 500, 500), inset_rects[1]);

    EXPECT_TRUE(images[2].image.IsSameForTesting(discardable_image[2][3]));
    EXPECT_EQ(gfx::Rect(1024 + 3 * 512 + 6, 2 * 512 + 6, 500, 500),
              inset_rects[2]);

    EXPECT_TRUE(images[3].image.IsSameForTesting(discardable_image[3][2]));
    EXPECT_EQ(gfx::Rect(1024 + 2 * 512 + 6, 3 * 512 + 6, 500, 500),
              inset_rects[3]);
  }

  // Non intersecting rects
  {
    std::vector<PositionScaleDrawImage> images =
        GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 0, 1000, 1000));
    EXPECT_EQ(0u, images.size());
  }
  {
    std::vector<PositionScaleDrawImage> images =
        GetDiscardableImagesInRect(*image_map, gfx::Rect(3500, 0, 1000, 1000));
    EXPECT_EQ(0u, images.size());
  }
  {
    std::vector<PositionScaleDrawImage> images =
        GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 1100, 1000, 1000));
    EXPECT_EQ(0u, images.size());
  }
  {
    std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
        *image_map, gfx::Rect(3500, 1100, 1000, 1000));
    EXPECT_EQ(0u, images.size());
  }

  // Image not present in the list.
  {
    PaintImage image = CreateDiscardablePaintImage(gfx::Size(500, 500));
    EXPECT_EQ(image_map->GetRectsForImage(image.stable_id()).size(), 0u);
  }
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectOnePixelQuery) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  // Discardable pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  PaintImage discardable_image[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] =
            CreateDiscardablePaintImage(gfx::Size(500, 500));
        content_layer_client.add_draw_image(
            discardable_image[y][x], gfx::Point(x * 512 + 6, y * 512 + 6));
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<PositionScaleDrawImage> images = GetDiscardableImagesInRect(
          *image_map, gfx::Rect(x * 512 + 256, y * 512 + 256, 1, 1));
      std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
      if ((x + y) & 1) {
        EXPECT_EQ(1u, images.size()) << x << " " << y;
        EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image[y][x]))
            << x << " " << y;
        EXPECT_EQ(gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500),
                  inset_rects[0]);
      } else {
        EXPECT_EQ(0u, images.size()) << x << " " << y;
      }
    }
  }
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectMassiveImage) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  PaintImage discardable_image =
      CreateDiscardablePaintImage(gfx::Size(1 << 25, 1 << 25), nullptr,
                                  false /* allocate_encoded_memory */);
  content_layer_client.add_draw_image(discardable_image, gfx::Point(0, 0));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 0, 1, 1));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image));
  EXPECT_EQ(gfx::Rect(0, 0, 2048, 2048), inset_rects[0]);
}

TEST_F(DiscardableImageMapTest, PaintDestroyedWhileImageIsDrawn) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  PaintImage discardable_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  PaintRecord record = CreateRecording(discardable_image, visible_rect);

  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;
  PaintFlags paint;
  display_list->StartPaint();
  display_list->push<SaveLayerOp>(gfx::RectToSkRect(visible_rect), paint);
  display_list->push<DrawRecordOp>(std::move(record));
  display_list->push<RestoreOp>();
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 0, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image));
}

// Check if SkNoDrawCanvas does not crash for large layers.
TEST_F(DiscardableImageMapTest, RestoreSavedBigLayers) {
  PaintFlags flags;
  SkRect rect =
      SkRect::MakeWH(static_cast<float>(INT_MAX), static_cast<float>(INT_MAX));
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;
  display_list->StartPaint();
  display_list->push<DrawRectOp>(rect, flags);
  display_list->EndPaintOfUnpaired(gfx::Rect(INT_MAX, INT_MAX));
  display_list->Finalize();
  display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
}

// Test if SaveLayer and Restore work together.
// 1. Move cursor to (25, 25) draw a black rect of size 25x25.
// 2. save layer, move the cursor by (100, 100) or to point (125, 125), draw a
// red rect of size 25x25.
// 3. Restore layer, so the cursor moved back to (25, 25), move cursor by (100,
// 0) or at the point (125, 25), draw a yellow rect of size 25x25.
//  (25, 25)
//  +---+
//  |   |
//  +---+
//  (25, 125) (125, 125)
//  +---+     +---+
//  |   |     |   |
//  +---+     +---+
TEST_F(DiscardableImageMapTest, RestoreSavedTransformedLayers) {
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;
  PaintFlags paint;
  gfx::Rect visible_rect(200, 200);
  display_list->StartPaint();

  PaintImage discardable_image1 =
      CreateDiscardablePaintImage(gfx::Size(25, 25));
  PaintImage discardable_image2 =
      CreateDiscardablePaintImage(gfx::Size(25, 25));
  PaintImage discardable_image3 =
      CreateDiscardablePaintImage(gfx::Size(25, 25));
  display_list->push<TranslateOp>(25.0f, 25.0f);
  display_list->push<DrawImageOp>(discardable_image1, 0.f, 0.f);
  display_list->push<SaveLayerOp>(paint);
  display_list->push<TranslateOp>(100.0f, 100.0f);
  display_list->push<DrawImageOp>(discardable_image2, 0.f, 0.f);
  display_list->push<RestoreOp>();
  display_list->push<TranslateOp>(0.0f, 100.0f);
  display_list->push<DrawImageOp>(discardable_image3, 0.f, 0.f);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ(3u, images.size());
  EXPECT_EQ(gfx::Rect(25, 25, 25, 25), InsetImageRects(images)[0]);
  EXPECT_EQ(gfx::Rect(125, 125, 25, 25), InsetImageRects(images)[1]);
  EXPECT_EQ(gfx::Rect(25, 125, 25, 25), InsetImageRects(images)[2]);
}

TEST_F(DiscardableImageMapTest, NullPaintOnSaveLayer) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  PaintImage discardable_image = CreateDiscardablePaintImage(gfx::Size(10, 10));
  PaintRecord record = CreateRecording(discardable_image, visible_rect);

  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;
  display_list->StartPaint();
  display_list->push<SaveLayerOp>(gfx::RectToSkRect(visible_rect),
                                  PaintFlags());
  display_list->push<DrawRecordOp>(std::move(record));
  display_list->push<RestoreOp>();
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 0, 1, 1));
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image));
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectMaxImage) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  int dimension = std::numeric_limits<int>::max();
  sk_sp<SkColorSpace> no_color_space;
  PaintImage discardable_image = CreateDiscardablePaintImage(
      gfx::Size(dimension, dimension), no_color_space,
      false /* allocate_encoded_memory */);
  content_layer_client.add_draw_image(discardable_image, gfx::Point(42, 42));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(42, 42, 1, 1));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image));
  EXPECT_EQ(gfx::Rect(42, 42, 2006, 2006), inset_rects[0]);
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectMaxImageMaxLayer) {
  // At large values of integer x, x != static_cast<int>(static_cast<float>(x)).
  // So, make sure the dimension can be converted back and forth for the
  // purposes of the unittest. Also, at near max int values, Skia seems to skip
  // some draw calls, so we subtract 64 since we only care about "really large"
  // values, not necessarily max int values.
  int dimension = static_cast<int>(
      static_cast<float>(std::numeric_limits<int>::max() - 64));
  gfx::Rect visible_rect(dimension, dimension);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  sk_sp<SkColorSpace> no_color_space;
  PaintImage discardable_image1 = CreateDiscardablePaintImage(
      gfx::Size(dimension, dimension), no_color_space,
      false /* allocate_encoded_memory */);
  PaintImage discardable_image2 = CreateDiscardablePaintImage(
      gfx::Size(dimension, dimension), no_color_space,
      false /* allocate_encoded_memory */);
  PaintImage discardable_image3 = CreateDiscardablePaintImage(
      gfx::Size(dimension, dimension), no_color_space,
      false /* allocate_encoded_memory */);

  content_layer_client.add_draw_image(discardable_image1, gfx::Point(0, 0));
  content_layer_client.add_draw_image(discardable_image2, gfx::Point(10000, 0));
  content_layer_client.add_draw_image(discardable_image3,
                                      gfx::Point(-10000, 500));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 0, 1, 1));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), inset_rects[0]);

  images = GetDiscardableImagesInRect(*image_map, gfx::Rect(10000, 0, 1, 1));
  inset_rects = InsetImageRects(images);
  EXPECT_EQ(2u, images.size());
  EXPECT_EQ(gfx::Rect(10000, 0, dimension - 10000, dimension), inset_rects[1]);
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), inset_rects[0]);

  // Since we adjust negative offsets before using ToEnclosingRect, the expected
  // width will be converted to float, which means that we lose some precision.
  // The expected value is whatever the value is converted to float and then
  // back to int.
  int expected10k = static_cast<int>(static_cast<float>(dimension - 10000));
  images = GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 500, 1, 1));
  inset_rects = InsetImageRects(images);
  EXPECT_EQ(2u, images.size());
  EXPECT_EQ(gfx::Rect(0, 500, expected10k, dimension - 500), inset_rects[1]);
  EXPECT_EQ(gfx::Rect(0, 0, dimension, dimension), inset_rects[0]);
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesRectInBounds) {
  gfx::Rect visible_rect(1000, 1000);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  PaintImage discardable_image1 =
      CreateDiscardablePaintImage(gfx::Size(100, 100));
  PaintImage discardable_image2 =
      CreateDiscardablePaintImage(gfx::Size(100, 100));
  PaintImage long_discardable_image =
      CreateDiscardablePaintImage(gfx::Size(10000, 100));

  content_layer_client.add_draw_image(discardable_image1, gfx::Point(-10, -11));
  content_layer_client.add_draw_image(discardable_image2, gfx::Point(950, 951));
  content_layer_client.add_draw_image(long_discardable_image,
                                      gfx::Point(-100, 500));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 0, 1, 1));
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);

  ASSERT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 0, 90, 89), inset_rects[0]);

  images = GetDiscardableImagesInRect(*image_map, gfx::Rect(999, 999, 1, 1));
  inset_rects = InsetImageRects(images);
  ASSERT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(950, 951, 50, 49), inset_rects[0]);

  images = GetDiscardableImagesInRect(*image_map, gfx::Rect(0, 500, 1, 1));
  inset_rects = InsetImageRects(images);
  ASSERT_EQ(1u, images.size());
  EXPECT_EQ(gfx::Rect(0, 500, 1000, 100), inset_rects[0]);
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInShader) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  // Discardable pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  PaintImage discardable_image[4][4];

  // Skia doesn't allow shader instantiation with non-invertible local
  // transforms, so we can't let the scale drop all the way to 0.
  static constexpr float kMinScale = 0.1f;

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        discardable_image[y][x] =
            PaintImageBuilder::WithDefault()
                .set_id(y * 4 + x)
                .set_paint_image_generator(
                    CreatePaintImageGenerator(gfx::Size(500, 500)))
                .TakePaintImage();
        SkMatrix scale = SkMatrix::Scale(std::max(x * 0.5f, kMinScale),
                                         std::max(y * 0.5f, kMinScale));
        PaintFlags flags;
        flags.setShader(PaintShader::MakeImage(discardable_image[y][x],
                                               SkTileMode::kClamp,
                                               SkTileMode::kClamp, &scale));
        content_layer_client.add_draw_rect(
            gfx::Rect(x * 512 + 6, y * 512 + 6, 500, 500), flags);
      }
    }
  }

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::vector<const DrawImage*> draw_images =
          image_map->GetDiscardableImagesInRect(
              gfx::Rect(x * 512, y * 512, 500, 500));
      if ((x + y) & 1) {
        EXPECT_EQ(1u, draw_images.size()) << x << " " << y;
        EXPECT_TRUE(draw_images[0]->paint_image().IsSameForTesting(
            discardable_image[y][x]))
            << x << " " << y;
        EXPECT_EQ(std::max(x * 0.5f, kMinScale),
                  draw_images[0]->scale().fWidth);
        EXPECT_EQ(std::max(y * 0.5f, kMinScale),
                  draw_images[0]->scale().fHeight);
      } else {
        EXPECT_EQ(0u, draw_images.size()) << x << " " << y;
      }
    }
  }

  // Capture 4 pixel refs.
  std::vector<const DrawImage*> draw_images =
      image_map->GetDiscardableImagesInRect(gfx::Rect(512, 512, 2048, 2048));
  EXPECT_EQ(4u, draw_images.size());
  EXPECT_TRUE(
      draw_images[0]->paint_image().IsSameForTesting(discardable_image[1][2]));
  EXPECT_TRUE(
      draw_images[1]->paint_image().IsSameForTesting(discardable_image[2][1]));
  EXPECT_TRUE(
      draw_images[2]->paint_image().IsSameForTesting(discardable_image[2][3]));
  EXPECT_TRUE(
      draw_images[3]->paint_image().IsSameForTesting(discardable_image[3][2]));
}

TEST_F(DiscardableImageMapTest, ClipsImageRects) {
  gfx::Rect visible_rect(500, 500);

  PaintImage discardable_image =
      CreateDiscardablePaintImage(gfx::Size(500, 500));
  PaintRecord record = CreateRecording(discardable_image, visible_rect);

  scoped_refptr<DisplayItemList> display_list = new DisplayItemList;

  display_list->StartPaint();
  display_list->push<ClipRectOp>(gfx::RectToSkRect(gfx::Rect(250, 250)),
                                 SkClipOp::kIntersect, false);
  display_list->push<DrawRecordOp>(std::move(record));
  display_list->EndPaintOfUnpaired(gfx::Rect(250, 250));

  display_list->Finalize();

  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, visible_rect);
  std::vector<gfx::Rect> inset_rects = InsetImageRects(images);
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(images[0].image.IsSameForTesting(discardable_image));
  EXPECT_EQ(gfx::Rect(250, 250), inset_rects[0]);
}

TEST_F(DiscardableImageMapTest, GathersDiscardableImagesFromNestedOps) {
  // This |discardable_image| is in a PaintOpBuffer that gets added to
  // the root buffer.
  PaintOpBuffer internal_buffer;
  PaintImage discardable_image =
      CreateDiscardablePaintImage(gfx::Size(100, 100));
  internal_buffer.push<DrawImageOp>(discardable_image, 0.f, 0.f);

  // This |discardable_image2| is in a DisplayItemList that gets added
  // to the root buffer.
  PaintImage discardable_image2 =
      CreateDiscardablePaintImage(gfx::Size(100, 100));

  PaintOpBuffer buffer2;
  buffer2.push<DrawImageOp>(discardable_image2, 100.f, 100.f);

  PaintOpBuffer root_buffer;
  root_buffer.push<DrawRecordOp>(internal_buffer.ReleaseAsRecord());
  root_buffer.push<DrawRecordOp>(buffer2.ReleaseAsRecord());
  scoped_refptr<DiscardableImageMap> image_map = DiscardableImageMap::Generate(
      root_buffer, gfx::Rect(200, 200), ScrollOffsetMap());

  std::vector<const DrawImage*> images =
      image_map->GetDiscardableImagesInRect(gfx::Rect(0, 0, 5, 95));
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(discardable_image.IsSameForTesting(images[0]->paint_image()));

  images = image_map->GetDiscardableImagesInRect(gfx::Rect(105, 105, 5, 95));
  EXPECT_EQ(1u, images.size());
  EXPECT_TRUE(discardable_image2.IsSameForTesting(images[0]->paint_image()));
}

TEST_F(DiscardableImageMapTest, GathersAnimatedImages) {
  gfx::Rect visible_rect(1000, 1000);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(2)),
      FrameMetadata(true, base::Milliseconds(3))};

  gfx::Size image_size(100, 100);
  PaintImage static_image = CreateDiscardablePaintImage(image_size);
  PaintImage animated_loop_none =
      CreateAnimatedImage(image_size, frames, kAnimationNone);
  PaintImage animation_loop_infinite =
      CreateAnimatedImage(image_size, frames, 1u);

  content_layer_client.add_draw_image(static_image, gfx::Point(0, 0));
  content_layer_client.add_draw_image(animated_loop_none, gfx::Point(100, 100));
  content_layer_client.add_draw_image(animation_loop_infinite,
                                      gfx::Point(200, 200));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  const auto& animated_images_metadata = image_map->animated_images_metadata();

  ASSERT_EQ(animated_images_metadata.size(), 1u);
  EXPECT_EQ(animated_images_metadata[0].paint_image_id,
            animation_loop_infinite.stable_id());
  EXPECT_EQ(animated_images_metadata[0].completion_state,
            animation_loop_infinite.completion_state());
  EXPECT_EQ(animated_images_metadata[0].frames,
            animation_loop_infinite.GetFrameMetadata());
  EXPECT_EQ(animated_images_metadata[0].repetition_count,
            animation_loop_infinite.repetition_count());

  std::vector<const DrawImage*> images =
      image_map->GetDiscardableImagesInRect(visible_rect);
  ASSERT_EQ(images.size(), 3u);
  EXPECT_TRUE(images[0]->paint_image().IsSameForTesting(static_image));
  EXPECT_DCHECK_DEATH(images[0]->frame_index());
  EXPECT_TRUE(images[1]->paint_image().IsSameForTesting(animated_loop_none));
  EXPECT_DCHECK_DEATH(images[1]->frame_index());
  EXPECT_TRUE(
      images[2]->paint_image().IsSameForTesting(animation_loop_infinite));
  EXPECT_DCHECK_DEATH(images[2]->frame_index());
}

TEST_F(DiscardableImageMapTest, GathersPaintWorklets) {
  gfx::Rect visible_rect(1000, 1000);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  gfx::Size image_size(100, 100);
  PaintImage static_image = CreateDiscardablePaintImage(image_size);
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(image_size));
  PaintImage paint_worklet_image = CreatePaintWorkletPaintImage(input);

  content_layer_client.add_draw_image(static_image, gfx::Point(0, 0));
  content_layer_client.add_draw_image(paint_worklet_image,
                                      gfx::Point(100, 100));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  const auto& paint_worklet_inputs = image_map->paint_worklet_inputs();
  ASSERT_EQ(paint_worklet_inputs.size(), 1u);
  EXPECT_EQ(paint_worklet_inputs[0].first, input);

  // PaintWorklets are not considered discardable images.
  std::vector<PositionScaleDrawImage> images =
      GetDiscardableImagesInRect(*image_map, visible_rect);
  ASSERT_EQ(images.size(), 1u);
  EXPECT_TRUE(images[0].image.IsSameForTesting(static_image));
}

TEST_F(DiscardableImageMapTest, CapturesImagesInPaintRecordShaders) {
  // Create the record to use in the shader.
  PaintOpBuffer shader_buffer;
  shader_buffer.push<ScaleOp>(2.0f, 2.0f);

  PaintImage static_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  shader_buffer.push<DrawImageOp>(static_image, 0.f, 0.f);

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(1)),
      FrameMetadata(true, base::Milliseconds(1))};
  PaintImage animated_image = CreateAnimatedImage(gfx::Size(100, 100), frames);
  shader_buffer.push<DrawImageOp>(animated_image, 0.f, 0.f);

  gfx::Rect visible_rect(500, 500);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList();
  display_list->StartPaint();
  display_list->push<ScaleOp>(2.0f, 2.0f);
  PaintFlags flags;
  SkRect tile = SkRect::MakeWH(100, 100);
  flags.setShader(PaintShader::MakePaintRecord(shader_buffer.ReleaseAsRecord(),
                                               tile, SkTileMode::kClamp,
                                               SkTileMode::kClamp, nullptr));
  display_list->push<DrawRectOp>(SkRect::MakeWH(200, 200), flags);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  EXPECT_EQ(flags.getShader()->image_analysis_state(),
            ImageAnalysisState::kNoAnalysis);
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  EXPECT_EQ(flags.getShader()->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);

  // The image rect is set to the rect for the DrawRectOp, and only animated
  // images in a shader are tracked.
  std::vector<PositionScaleDrawImage> draw_images =
      GetDiscardableImagesInRect(*image_map, visible_rect);
  std::vector<gfx::Rect> inset_rects = InsetImageRects(draw_images);
  ASSERT_EQ(draw_images.size(), 1u);
  EXPECT_TRUE(draw_images[0].image.IsSameForTesting(animated_image));
  // The position of the image is the position of the DrawRectOp that uses the
  // shader.
  EXPECT_EQ(gfx::Rect(400, 400), inset_rects[0]);
  // The scale of the image includes the scale at which the shader record is
  // rasterized.
  EXPECT_EQ(SkSize::Make(4.f, 4.f), draw_images[0].scale);
}

TEST_F(DiscardableImageMapTest, CapturesImagesInPaintFilters) {
  // Create the record to use in the filter.
  PaintOpBuffer filter_buffer;

  PaintImage static_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  filter_buffer.push<DrawImageOp>(static_image, 0.f, 0.f);

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(1)),
      FrameMetadata(true, base::Milliseconds(1))};
  PaintImage animated_image = CreateAnimatedImage(gfx::Size(100, 100), frames);
  filter_buffer.push<DrawImageOp>(animated_image, 0.f, 0.f);

  gfx::Rect visible_rect(500, 500);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList();
  display_list->StartPaint();
  PaintFlags flags;
  flags.setImageFilter(sk_make_sp<RecordPaintFilter>(
      filter_buffer.ReleaseAsRecord(), SkRect::MakeWH(150.f, 150.f)));
  display_list->push<DrawRectOp>(SkRect::MakeWH(200, 200), flags);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  EXPECT_EQ(flags.getImageFilter()->image_analysis_state(),
            ImageAnalysisState::kNoAnalysis);
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  EXPECT_EQ(flags.getImageFilter()->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);

  // The image rect is set to the rect for the DrawRectOp, and only animated
  // images in a filter are tracked.
  std::vector<PositionScaleDrawImage> draw_images =
      GetDiscardableImagesInRect(*image_map, visible_rect);
  std::vector<gfx::Rect> inset_rects = InsetImageRects(draw_images);
  ASSERT_EQ(draw_images.size(), 1u);
  EXPECT_TRUE(draw_images[0].image.IsSameForTesting(animated_image));
  // The position of the image is the position of the DrawRectOp that uses the
  // filter. Since the bounds of the filter does not depend on the source/input,
  // the resulting bounds is that of the RecordPaintFilter.
  EXPECT_EQ(gfx::Rect(150, 150), inset_rects[0]);
  // Images in a filter are decoded at the original size.
  EXPECT_EQ(SkSize::Make(1.f, 1.f), draw_images[0].scale);
}

TEST_F(DiscardableImageMapTest, CapturesImagesInSaveLayers) {
  PaintFlags flags;
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  flags.setShader(PaintShader::MakeImage(image, SkTileMode::kClamp,
                                         SkTileMode::kClamp, nullptr));

  gfx::Rect visible_rect(500, 500);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList();
  display_list->StartPaint();
  display_list->push<SaveLayerOp>(flags);
  display_list->push<DrawColorOp>(SkColors::kBlue, SkBlendMode::kSrc);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();

  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  std::vector<PositionScaleDrawImage> draw_images =
      GetDiscardableImagesInRect(*image_map, visible_rect);
  std::vector<gfx::Rect> inset_rects = InsetImageRects(draw_images);
  ASSERT_EQ(draw_images.size(), 1u);
  EXPECT_TRUE(draw_images[0].image.IsSameForTesting(image));
  EXPECT_EQ(gfx::Rect(500, 500), inset_rects[0]);
  EXPECT_EQ(SkSize::Make(1.f, 1.f), draw_images[0].scale);
}

TEST_F(DiscardableImageMapTest, EmbeddedShaderWithAnimatedImages) {
  // Create the record with animated image to use in the shader.
  SkRect tile = SkRect::MakeWH(100, 100);
  PaintOpBuffer shader_buffer;
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(1)),
      FrameMetadata(true, base::Milliseconds(1))};
  PaintImage animated_image = CreateAnimatedImage(gfx::Size(100, 100), frames);
  shader_buffer.push<DrawImageOp>(animated_image, 0.f, 0.f);
  auto shader_with_image = PaintShader::MakePaintRecord(
      shader_buffer.ReleaseAsRecord(), tile, SkTileMode::kClamp,
      SkTileMode::kClamp, nullptr);

  // Create a second shader which uses the shader above.
  PaintOpBuffer second_shader_buffer;
  PaintFlags flags;
  flags.setShader(shader_with_image);
  second_shader_buffer.push<DrawRectOp>(SkRect::MakeWH(200, 200), flags);
  auto shader_with_shader_with_image = PaintShader::MakePaintRecord(
      second_shader_buffer.ReleaseAsRecord(), tile, SkTileMode::kClamp,
      SkTileMode::kClamp, nullptr);

  gfx::Rect visible_rect(500, 500);
  scoped_refptr<DisplayItemList> display_list = new DisplayItemList();
  display_list->StartPaint();
  flags.setShader(shader_with_shader_with_image);
  display_list->push<DrawRectOp>(SkRect::MakeWH(200, 200), flags);
  display_list->EndPaintOfUnpaired(visible_rect);
  display_list->Finalize();
  display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  EXPECT_EQ(shader_with_image->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);
  EXPECT_EQ(shader_with_shader_with_image->image_analysis_state(),
            ImageAnalysisState::kAnimatedImages);
}

TEST_F(DiscardableImageMapTest, DecodingModeHintsBasic) {
  gfx::Rect visible_rect(100, 100);
  PaintImage unspecified_image =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(1)
          .set_decoding_mode(PaintImage::DecodingMode::kUnspecified)
          .TakePaintImage();
  PaintImage async_image =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(2)
          .set_decoding_mode(PaintImage::DecodingMode::kAsync)
          .TakePaintImage();
  PaintImage sync_image =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(3)
          .set_decoding_mode(PaintImage::DecodingMode::kSync)
          .TakePaintImage();

  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());
  content_layer_client.add_draw_image(unspecified_image, gfx::Point(0, 0));
  content_layer_client.add_draw_image(async_image, gfx::Point(10, 10));
  content_layer_client.add_draw_image(sync_image, gfx::Point(20, 20));
  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  auto decode_hints = image_map->TakeDecodingModeMap();
  EXPECT_EQ(decode_hints.size(), 3u);
  EXPECT_TRUE(base::Contains(decode_hints, 1));
  EXPECT_TRUE(base::Contains(decode_hints, 2));
  EXPECT_TRUE(base::Contains(decode_hints, 3));
  EXPECT_EQ(decode_hints[1], PaintImage::DecodingMode::kUnspecified);
  EXPECT_EQ(decode_hints[2], PaintImage::DecodingMode::kAsync);
  EXPECT_EQ(decode_hints[3], PaintImage::DecodingMode::kSync);

  decode_hints = image_map->TakeDecodingModeMap();
  EXPECT_EQ(decode_hints.size(), 0u);
}

TEST_F(DiscardableImageMapTest, DecodingModeHintsDuplicates) {
  gfx::Rect visible_rect(100, 100);
  PaintImage unspecified_image1 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(1)
          .set_decoding_mode(PaintImage::DecodingMode::kUnspecified)
          .TakePaintImage();
  PaintImage async_image1 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(1)
          .set_decoding_mode(PaintImage::DecodingMode::kAsync)
          .TakePaintImage();

  PaintImage unspecified_image2 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(2)
          .set_decoding_mode(PaintImage::DecodingMode::kUnspecified)
          .TakePaintImage();
  PaintImage sync_image2 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(2)
          .set_decoding_mode(PaintImage::DecodingMode::kSync)
          .TakePaintImage();

  PaintImage async_image3 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(3)
          .set_decoding_mode(PaintImage::DecodingMode::kAsync)
          .TakePaintImage();
  PaintImage sync_image3 =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(10, 10)))
          .set_id(3)
          .set_decoding_mode(PaintImage::DecodingMode::kSync)
          .TakePaintImage();

  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());
  content_layer_client.add_draw_image(unspecified_image1, gfx::Point(0, 0));
  content_layer_client.add_draw_image(async_image1, gfx::Point(10, 10));
  content_layer_client.add_draw_image(unspecified_image2, gfx::Point(20, 20));
  content_layer_client.add_draw_image(sync_image2, gfx::Point(30, 30));
  content_layer_client.add_draw_image(async_image3, gfx::Point(40, 40));
  content_layer_client.add_draw_image(sync_image3, gfx::Point(50, 50));
  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  auto decode_hints = image_map->TakeDecodingModeMap();
  EXPECT_EQ(decode_hints.size(), 3u);
  EXPECT_TRUE(base::Contains(decode_hints, 1));
  EXPECT_TRUE(base::Contains(decode_hints, 2));
  EXPECT_TRUE(base::Contains(decode_hints, 3));
  // 1 was unspecified and async, so the result should be unspecified.
  EXPECT_EQ(decode_hints[1], PaintImage::DecodingMode::kUnspecified);
  // 2 was unspecified and sync, so the result should be sync.
  EXPECT_EQ(decode_hints[2], PaintImage::DecodingMode::kSync);
  // 3 was async and sync, so the result should be sync
  EXPECT_EQ(decode_hints[3], PaintImage::DecodingMode::kSync);

  decode_hints = image_map->TakeDecodingModeMap();
  EXPECT_EQ(decode_hints.size(), 0u);
}

TEST_F(DiscardableImageMapTest, TracksImageRegions) {
  gfx::Rect visible_rect(500, 500);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(1)),
      FrameMetadata(true, base::Milliseconds(1)),
  };
  auto image = CreateAnimatedImage(gfx::Size(100, 100), frames);
  content_layer_client.add_draw_image(image, gfx::Point(0, 0));
  content_layer_client.add_draw_image(image, gfx::Point(400, 400));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());

  std::vector<gfx::Rect> rects = {gfx::Rect(100, 100),
                                  gfx::Rect(400, 400, 100, 100)};
  Region expected_region;
  for (auto& rect : rects) {
    rect.Inset(-1);
    expected_region.Union(rect);
  }

  EXPECT_EQ(ImageRectsToRegion(image_map->GetRectsForImage(image.stable_id())),
            expected_region);
}

TEST_F(DiscardableImageMapTest, ImagesUnderDrawScrollingContentsOp) {
  PaintImage image1 = CreateDiscardablePaintImage(gfx::Size(100, 300));
  PaintImage image2 = CreateDiscardablePaintImage(gfx::Size(300, 200));

  auto scrolling_contents_list = base::MakeRefCounted<DisplayItemList>();
  scrolling_contents_list->StartPaint();
  scrolling_contents_list->push<DrawImageOp>(image1, 0.f, 0.f);
  scrolling_contents_list->push<DrawImageOp>(image2, 100.f, 350.f);
  scrolling_contents_list->EndPaintOfUnpaired(gfx::Rect(500, 700));
  scrolling_contents_list->Finalize();

  ElementId scroll_element_id1(123);
  ElementId scroll_element_id2(456);
  auto display_list = base::MakeRefCounted<DisplayItemList>();
  // Draw scrolling contents op under a clip.
  display_list->StartPaint();
  display_list->push<SaveOp>();
  display_list->push<ClipRectOp>(SkRect::MakeWH(200, 200), SkClipOp::kIntersect,
                                 false);
  display_list->EndPaintOfPairedBegin();
  display_list->PushDrawScrollingContentsOp(
      scroll_element_id1, scrolling_contents_list, gfx::Rect(200, 200));
  display_list->StartPaint();
  display_list->push<RestoreOp>();
  display_list->EndPaintOfPairedEnd();
  // Draw another scrolling contents op under a translate and a clip.
  display_list->StartPaint();
  display_list->push<SaveOp>();
  display_list->push<TranslateOp>(100.f, 300.f);
  display_list->push<ClipRectOp>(SkRect::MakeWH(200, 200), SkClipOp::kIntersect,
                                 false);
  display_list->EndPaintOfPairedBegin();
  display_list->PushDrawScrollingContentsOp(scroll_element_id2,
                                            scrolling_contents_list,
                                            gfx::Rect(100, 300, 200, 200));
  display_list->StartPaint();
  display_list->push<RestoreOp>();
  display_list->EndPaintOfPairedEnd();
  display_list->Finalize();

  ScrollOffsetMap scroll_offsets;
  scroll_offsets[scroll_element_id1] = gfx::PointF();
  scroll_offsets[scroll_element_id2] = gfx::PointF();

  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(scroll_offsets);
  // The image rect is the union of the two appearances of image1.
  EXPECT_THAT(GetDiscardableImagesInRect(*image_map, gfx::Rect(1000, 1000)),
              UnorderedElementsAre(
                  FieldsAre(ImageIsSame(image1), gfx::Rect(-1, -1, 202, 502),
                            SkSize::Make(1, 1)),
                  FieldsAre(ImageIsSame(image1), gfx::Rect(-1, -1, 202, 502),
                            SkSize::Make(1, 1))));

  // The first scroller scrolls to make both images invisible.
  scroll_offsets[scroll_element_id1] = gfx::PointF(200, 100);
  // The second scroller scrolls to make both images visible.
  scroll_offsets[scroll_element_id2] = gfx::PointF(0, 250);
  image_map = display_list->GenerateDiscardableImageMap(scroll_offsets);
  EXPECT_THAT(GetDiscardableImagesInRect(*image_map, gfx::Rect(1000, 1000)),
              UnorderedElementsAre(
                  FieldsAre(ImageIsSame(image1), gfx::Rect(99, 299, 102, 52),
                            SkSize::Make(1, 1)),
                  FieldsAre(ImageIsSame(image2), gfx::Rect(199, 399, 102, 102),
                            SkSize::Make(1, 1))));
}

#if BUILDFLAG(SKIA_SUPPORT_SKOTTIE)
TEST_F(DiscardableImageMapTest,
       GetDiscardableImagesInRectSkottieWithoutImages) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());
  content_layer_client.add_draw_skottie(FakeContentLayerClient::SkottieData(
      CreateSkottie(gfx::Size(2048, 2048), /*duration_secs=*/1.f),
      /*dst=*/gfx::Rect(2048, 2048), /*t=*/0.1f, SkottieFrameDataMap(),
      SkottieColorMap(), SkottieTextPropertyValueMap()));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  EXPECT_THAT(GetDiscardableImagesInRect(*image_map, gfx::Rect(2048, 2048)),
              IsEmpty());
}

TEST_F(DiscardableImageMapTest, GetDiscardableImagesInRectSkottieWithImages) {
  gfx::Rect visible_rect(2048, 2048);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());
  // Skottie animation only is rendered in the right half of the screen.
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);

  SkottieFrameDataMap images_in;
  PaintImage image_0 = CreateDiscardablePaintImage(
      gfx::Size(kLottieDataWith2AssetsWidth, kLottieDataWith2AssetsHeight));
  PaintImage image_1 = CreateDiscardablePaintImage(
      gfx::Size(kLottieDataWith2AssetsWidth, kLottieDataWith2AssetsHeight));
  images_in[HashSkottieResourceId("image_0")] = {
      .image = image_0, .quality = PaintFlags::FilterQuality::kHigh};
  images_in[HashSkottieResourceId("image_1")] = {
      .image = image_1, .quality = PaintFlags::FilterQuality::kHigh};
  content_layer_client.add_draw_skottie(FakeContentLayerClient::SkottieData(
      CreateSkottieFromString(kLottieDataWith2Assets),
      /*dst=*/gfx::Rect(1024, 0, 1024, 2048),
      /*t=*/0.1f, images_in, SkottieColorMap(), SkottieTextPropertyValueMap()));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  // Left Half of screen should return no images.
  EXPECT_THAT(GetDiscardableImagesInRect(*image_map, gfx::Rect(1023, 2048)),
              IsEmpty());
  // Right Half of screen should return 2 images.
  std::vector<PositionScaleDrawImage> images_out =
      GetDiscardableImagesInRect(*image_map, gfx::Rect(1024, 0, 1024, 2048));
  ASSERT_THAT(images_out, SizeIs(2));
  EXPECT_THAT(images_out, Contains(Field(&PositionScaleDrawImage::image,
                                         ImageIsSame(image_0))));
  EXPECT_THAT(images_out, Contains(Field(&PositionScaleDrawImage::image,
                                         ImageIsSame(image_1))));
}

TEST_F(DiscardableImageMapTest,
       GetDiscardableImagesInRectSkottieWithImagesScalesProperly) {
  gfx::Rect visible_rect(kLottieDataWith2AssetsWidth * 2,
                         kLottieDataWith2AssetsHeight * 3);
  FakeContentLayerClient content_layer_client;
  content_layer_client.set_bounds(visible_rect.size());
  scoped_refptr<SkottieWrapper> skottie =
      CreateSkottieFromString(kLottieDataWith2Assets);

  SkottieFrameDataMap images_in;
  PaintImage image_0 = CreateDiscardablePaintImage(
      gfx::Size(kLottieDataWith2AssetsWidth, kLottieDataWith2AssetsHeight));
  PaintImage image_1 = CreateDiscardablePaintImage(
      gfx::Size(kLottieDataWith2AssetsWidth, kLottieDataWith2AssetsHeight));
  images_in[HashSkottieResourceId("image_0")] = {
      .image = image_0, .quality = PaintFlags::FilterQuality::kHigh};
  images_in[HashSkottieResourceId("image_1")] = {
      .image = image_1, .quality = PaintFlags::FilterQuality::kHigh};
  content_layer_client.add_draw_skottie(FakeContentLayerClient::SkottieData(
      CreateSkottieFromString(kLottieDataWith2Assets),
      /*dst=*/visible_rect,
      /*t=*/0.1f, images_in, SkottieColorMap(), SkottieTextPropertyValueMap()));

  scoped_refptr<DisplayItemList> display_list =
      content_layer_client.PaintContentsToDisplayList();
  scoped_refptr<DiscardableImageMap> image_map =
      display_list->GenerateDiscardableImageMap(ScrollOffsetMap());
  std::vector<PositionScaleDrawImage> images_out =
      GetDiscardableImagesInRect(*image_map, visible_rect);
  ASSERT_THAT(images_out, SizeIs(2));
  for (const PositionScaleDrawImage& image_out : images_out) {
    static constexpr float kScaleTolerance = .01f;
    EXPECT_THAT(image_out.scale.width(), FloatNear(2.f, kScaleTolerance));
    // Even though the destination rect's height is 3x the animation frame's
    // height, the image should not get stretched.
    EXPECT_THAT(image_out.scale.height(), FloatNear(2.f, kScaleTolerance));
  }
}

#endif  // BUILDFLAG(SKIA_SUPPORT_SKOTTIE)

}  // namespace cc
