// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/display_item_list.h"

#include <stddef.h>

#include <vector>

#include "base/logging.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/render_surface_filters.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_skcanvas.h"
#include "skia/ext/font_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {

namespace {

bool CompareN32Pixels(void* actual_pixels,
                      void* expected_pixels,
                      int width,
                      int height) {
  if (memcmp(actual_pixels, expected_pixels, 4 * width * height) == 0)
    return true;

  SkImageInfo actual_info = SkImageInfo::MakeN32Premul(width, height);
  SkBitmap actual_bitmap;
  actual_bitmap.installPixels(actual_info, actual_pixels,
                              actual_info.minRowBytes());

  SkImageInfo expected_info = SkImageInfo::MakeN32Premul(width, height);
  SkBitmap expected_bitmap;
  expected_bitmap.installPixels(expected_info, expected_pixels,
                                expected_info.minRowBytes());

  std::string gen_bmp_data_url = GetPNGDataUrl(actual_bitmap);
  std::string ref_bmp_data_url = GetPNGDataUrl(expected_bitmap);

  LOG(ERROR) << "Pixels do not match!";
  LOG(ERROR) << "Actual: " << gen_bmp_data_url;
  LOG(ERROR) << "Expected: " << ref_bmp_data_url;
  return false;
}

}  // namespace

class DisplayItemListTest : public testing::Test {
 protected:
  std::unique_ptr<base::Value> ToBaseValue(const DisplayItemList* list,
                                           bool include_items) {
    base::trace_event::TracedValueJSON value;
    list->AddToValue(&value, include_items);
    return value.ToBaseValue();
  }
};

#define EXPECT_TRACED_RECT(x, y, width, height, rect_list) \
  do {                                                     \
    ASSERT_EQ(4u, rect_list->size());                      \
    EXPECT_EQ(x, (*rect_list)[0].GetIfDouble());           \
    EXPECT_EQ(y, (*rect_list)[1].GetIfDouble());           \
    EXPECT_EQ(width, (*rect_list)[2].GetIfDouble());       \
    EXPECT_EQ(height, (*rect_list)[3].GetIfDouble());      \
  } while (false)

// AddToValue should not crash if there are different numbers of visual_rect
// are paint_op
TEST_F(DisplayItemListTest, TraceEmptyVisualRect) {
  PaintFlags red_paint;
  red_paint.setColor(SK_ColorRED);
  auto list = base::MakeRefCounted<DisplayItemList>();

  gfx::Point offset(8, 9);

  list->StartPaint();
  list->push<DrawRectOp>(SkRect::MakeEmpty(), red_paint);
  // The rect is empty to cause rtree generation to skip it.
  list->EndPaintOfUnpaired(gfx::Rect(offset, gfx::Size(0, 10)));
  list->StartPaint();
  list->push<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 10), red_paint);
  // This rect is not empty.
  list->EndPaintOfUnpaired(gfx::Rect(offset, gfx::Size(10, 10)));
  list->Finalize();

  // Pass: we don't crash
  std::unique_ptr<base::Value> root = ToBaseValue(list.get(), true);

  const base::Value::Dict* root_dict = root->GetIfDict();
  ASSERT_NE(nullptr, root_dict);
  const base::Value::Dict* params_dict = root_dict->FindDict("params");
  ASSERT_NE(nullptr, params_dict);
  const base::Value::List* items = params_dict->FindList("items");
  ASSERT_NE(nullptr, items);
  ASSERT_EQ(2u, items->size());

  const base::Value::Dict* item_dict;
  const base::Value::List* visual_rect;
  const std::string* name;

  item_dict = ((*items)[0]).GetIfDict();
  ASSERT_NE(nullptr, item_dict);
  visual_rect = item_dict->FindList("visual_rect");
  ASSERT_NE(nullptr, visual_rect);
  EXPECT_TRACED_RECT(0, 0, 0, 0, visual_rect);
  name = item_dict->FindString("name");
  ASSERT_NE(nullptr, name);
  EXPECT_EQ("DrawRectOp", *name);

  item_dict = ((*items)[1]).GetIfDict();
  ASSERT_NE(nullptr, item_dict);
  visual_rect = item_dict->FindList("visual_rect");
  ASSERT_NE(nullptr, visual_rect);
  EXPECT_TRACED_RECT(8, 9, 10, 10, visual_rect);
  name = item_dict->FindString("name");
  ASSERT_NE(nullptr, name);
  EXPECT_EQ("DrawRectOp", *name);
}

TEST_F(DisplayItemListTest, SingleUnpairedRange) {
  gfx::Rect layer_rect(100, 100);
  PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
  PaintFlags red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  auto list = base::MakeRefCounted<DisplayItemList>();

  gfx::Point offset(8, 9);

  list->StartPaint();
  list->push<SaveOp>();
  list->push<TranslateOp>(static_cast<float>(offset.x()),
                          static_cast<float>(offset.y()));
  list->push<DrawRectOp>(SkRect::MakeLTRB(0.f, 0.f, 60.f, 60.f), red_paint);
  list->push<DrawRectOp>(SkRect::MakeLTRB(50.f, 50.f, 75.f, 75.f), blue_flags);
  list->push<RestoreOp>();
  list->EndPaintOfUnpaired(gfx::Rect(offset, layer_rect.size()));
  list->Finalize();
  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkiaPaintCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(0.f + offset.x(), 0.f + offset.y(), 60.f + offset.x(),
                       60.f + offset.y()),
      red_paint);
  expected_canvas.drawRect(
      SkRect::MakeLTRB(50.f + offset.x(), 50.f + offset.y(), 75.f + offset.x(),
                       75.f + offset.y()),
      blue_flags);

  EXPECT_TRUE(CompareN32Pixels(pixels, expected_pixels, 100, 100));
}

TEST_F(DisplayItemListTest, EmptyUnpairedRangeDoesNotAddVisualRect) {
  gfx::Rect layer_rect(100, 100);
  auto list = base::MakeRefCounted<DisplayItemList>();

  {
    list->StartPaint();
    list->EndPaintOfUnpaired(layer_rect);
  }
  // No ops.
  EXPECT_EQ(0u, list->TotalOpCount());

  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<RestoreOp>();
    list->EndPaintOfUnpaired(layer_rect);
  }
  // Two ops.
  EXPECT_EQ(2u, list->TotalOpCount());
}

TEST_F(DisplayItemListTest, ClipPairedRange) {
  gfx::Rect layer_rect(100, 100);
  PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
  PaintFlags red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  auto list = base::MakeRefCounted<DisplayItemList>();

  gfx::Point first_offset(8, 9);
  gfx::Rect first_recording_rect(first_offset, layer_rect.size());

  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<TranslateOp>(static_cast<float>(first_offset.x()),
                            static_cast<float>(first_offset.y()));
    list->push<DrawRectOp>(SkRect::MakeWH(60, 60), red_paint);
    list->push<RestoreOp>();
    list->EndPaintOfUnpaired(first_recording_rect);
  }

  gfx::Rect clip_rect(60, 60, 10, 10);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_rect), SkClipOp::kIntersect,
                           true);
    list->EndPaintOfPairedBegin();
  }

  gfx::Point second_offset(2, 3);
  gfx::Rect second_recording_rect(second_offset, layer_rect.size());
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<TranslateOp>(static_cast<float>(second_offset.x()),
                            static_cast<float>(second_offset.y()));
    list->push<DrawRectOp>(SkRect::MakeLTRB(50.f, 50.f, 75.f, 75.f),
                           blue_flags);
    list->push<RestoreOp>();
    list->EndPaintOfUnpaired(second_recording_rect);
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkiaPaintCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(0.f + first_offset.x(), 0.f + first_offset.y(),
                       60.f + first_offset.x(), 60.f + first_offset.y()),
      red_paint);
  expected_canvas.clipRect(gfx::RectToSkRect(clip_rect));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(50.f + second_offset.x(), 50.f + second_offset.y(),
                       75.f + second_offset.x(), 75.f + second_offset.y()),
      blue_flags);

  EXPECT_TRUE(CompareN32Pixels(pixels, expected_pixels, 100, 100));
}

TEST_F(DisplayItemListTest, TransformPairedRange) {
  gfx::Rect layer_rect(100, 100);
  PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
  PaintFlags red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  auto list = base::MakeRefCounted<DisplayItemList>();

  gfx::Point first_offset(8, 9);
  gfx::Rect first_recording_rect(first_offset, layer_rect.size());
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<TranslateOp>(static_cast<float>(first_offset.x()),
                            static_cast<float>(first_offset.y()));
    list->push<DrawRectOp>(SkRect::MakeWH(60, 60), red_paint);
    list->push<RestoreOp>();
    list->EndPaintOfUnpaired(first_recording_rect);
  }

  gfx::Transform transform;
  transform.Rotate(45.0);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ConcatOp>(gfx::TransformToSkM44(transform));
    list->EndPaintOfPairedBegin();
  }

  gfx::Point second_offset(2, 3);
  gfx::Rect second_recording_rect(second_offset, layer_rect.size());
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<TranslateOp>(static_cast<float>(second_offset.x()),
                            static_cast<float>(second_offset.y()));
    list->push<DrawRectOp>(SkRect::MakeLTRB(50.f, 50.f, 75.f, 75.f),
                           blue_flags);
    list->push<RestoreOp>();
    list->EndPaintOfUnpaired(second_recording_rect);
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkiaPaintCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(0.f + first_offset.x(), 0.f + first_offset.y(),
                       60.f + first_offset.x(), 60.f + first_offset.y()),
      red_paint);
  expected_canvas.setMatrix(gfx::TransformToSkM44(transform));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(50.f + second_offset.x(), 50.f + second_offset.y(),
                       75.f + second_offset.x(), 75.f + second_offset.y()),
      blue_flags);

  EXPECT_TRUE(CompareN32Pixels(pixels, expected_pixels, 100, 100));
}

TEST_F(DisplayItemListTest, FilterPairedRange) {
  gfx::Rect layer_rect(100, 100);
  FilterOperations filters;
  unsigned char pixels[4 * 100 * 100] = {0};
  auto list = base::MakeRefCounted<DisplayItemList>();

  sk_sp<SkSurface> source_surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(50, 50));
  SkCanvas* source_canvas = source_surface->getCanvas();
  source_canvas->clear(SkColorSetRGB(128, 128, 128));
  PaintImage source_image = PaintImageBuilder::WithDefault()
                                .set_id(PaintImage::GetNextId())
                                .set_image(source_surface->makeImageSnapshot(),
                                           PaintImage::GetNextContentId())
                                .TakePaintImage();

  // For most SkImageFilters, the |dst| bounds computed by computeFastBounds are
  // dependent on the provided |src| bounds. This means, for example, that
  // translating |src| results in a corresponding translation of |dst|. But this
  // is not the case for all SkImageFilters; for some of them (e.g.
  // SkImageSource), the computation of |dst| in computeFastBounds doesn't
  // involve |src| at all. Incorrectly assuming such a relationship (e.g. by
  // translating |dst| after it is computed by computeFastBounds, rather than
  // translating |src| before it provided to computedFastBounds) can cause
  // incorrect clipping of filter output. To test for this, we include an
  // SkImageSource filter in |filters|. Here, |src| is |filter_bounds|, defined
  // below.
  SkRect rect = SkRect::MakeWH(source_image.width(), source_image.height());
  sk_sp<PaintFilter> image_filter = sk_make_sp<ImagePaintFilter>(
      source_image, rect, rect, PaintFlags::FilterQuality::kHigh);
  filters.Append(FilterOperation::CreateReferenceFilter(image_filter));
  filters.Append(FilterOperation::CreateBrightnessFilter(0.5f));
  gfx::RectF filter_bounds(10.f, 10.f, 50.f, 50.f);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<TranslateOp>(filter_bounds.x(), filter_bounds.y());

    PaintFlags flags;
    flags.setImageFilter(RenderSurfaceFilters::BuildImageFilter(filters));

    SkRect layer_bounds = gfx::RectFToSkRect(filter_bounds);
    layer_bounds.offset(-filter_bounds.x(), -filter_bounds.y());
    list->push<SaveLayerOp>(layer_bounds, flags);
    list->push<TranslateOp>(-filter_bounds.x(), -filter_bounds.y());

    list->EndPaintOfPairedBegin();
  }

  // Include a rect drawing so that filter is actually applied to something.
  {
    list->StartPaint();

    PaintFlags red_flags;
    red_flags.setColor(SK_ColorRED);

    list->push<DrawRectOp>(
        SkRect::MakeLTRB(filter_bounds.x(), filter_bounds.y(),
                         filter_bounds.right(), filter_bounds.bottom()),
        red_flags);

    list->EndPaintOfUnpaired(ToEnclosingRect(filter_bounds));
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();  // For SaveLayerOp.
    list->push<RestoreOp>();  // For SaveOp.
    list->EndPaintOfPairedEnd();
  }

  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  PaintFlags paint;
  paint.setColor(SkColorSetRGB(64, 64, 64));
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkiaPaintCanvas expected_canvas(expected_bitmap);
  expected_canvas.drawRect(RectFToSkRect(filter_bounds), paint);

  EXPECT_TRUE(CompareN32Pixels(pixels, expected_pixels, 100, 100));
}

TEST_F(DisplayItemListTest, BytesUsed) {
  const int kNumPaintOps = 1000;
  size_t memory_usage;

  auto list = base::MakeRefCounted<DisplayItemList>();

  gfx::Rect layer_rect(100, 100);
  PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);

  {
    list->StartPaint();
    for (int i = 0; i < kNumPaintOps; i++)
      list->push<DrawRectOp>(SkRect::MakeWH(1, 1), blue_flags);
    list->EndPaintOfUnpaired(layer_rect);
  }

  memory_usage = list->BytesUsed();
  EXPECT_GE(memory_usage, sizeof(DrawRectOp) * kNumPaintOps);
  EXPECT_LE(memory_usage, 2 * sizeof(DrawRectOp) * kNumPaintOps);
}

TEST_F(DisplayItemListTest, AsValueWithNoOps) {
  auto list = base::MakeRefCounted<DisplayItemList>();
  list->Finalize();

  // Pass |true| to ask for PaintOps even though there are none.
  std::unique_ptr<base::Value> root = ToBaseValue(list.get(), true);
  const base::Value::Dict* root_dict = root->GetIfDict();
  ASSERT_NE(nullptr, root_dict);
  // The traced value has a params dictionary as its root.
  {
    const base::Value::Dict* params_dict = root_dict->FindDict("params");
    ASSERT_NE(nullptr, params_dict);

    // The real contents of the traced value is in here.
    {
      const base::Value::List* params_list;

      // The layer_rect field is present by empty.
      params_list = params_dict->FindList("layer_rect");
      ASSERT_NE(nullptr, params_list);
      EXPECT_TRACED_RECT(0, 0, 0, 0, params_list);

      // The items list is there but empty.
      params_list = params_dict->FindList("items");
      ASSERT_NE(nullptr, params_list);
      EXPECT_EQ(0u, params_list->size());
    }
  }

  // Pass |false| to not include PaintOps.
  root = ToBaseValue(list.get(), false);
  root_dict = root->GetIfDict();
  ASSERT_NE(nullptr, root_dict);
  // The traced value has a params dictionary as its root.
  {
    const base::Value::Dict* params_dict = root_dict->FindDict("params");
    ASSERT_NE(nullptr, params_dict);

    // The real contents of the traced value is in here.
    {
      const base::Value::List* params_list;

      // The layer_rect field is present by empty.
      params_list = params_dict->FindList("layer_rect");
      ASSERT_NE(nullptr, params_list);
      EXPECT_TRACED_RECT(0, 0, 0, 0, params_list);

      // The items list is not there since we asked for no ops.
      params_list = params_dict->FindList("items");
      ASSERT_EQ(nullptr, params_list);
    }
  }
}

TEST_F(DisplayItemListTest, AsValueWithOps) {
  gfx::Rect layer_rect = gfx::Rect(1, 2, 8, 9);
  auto list = base::MakeRefCounted<DisplayItemList>();
  gfx::Transform transform;
  transform.Translate(6.f, 7.f);

  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ConcatOp>(gfx::TransformToSkM44(transform));
    list->EndPaintOfPairedBegin();
  }

  gfx::Point offset(2, 3);
  gfx::Rect bounds(offset, layer_rect.size());
  {
    list->StartPaint();

    PaintFlags red_paint;
    red_paint.setColor(SK_ColorRED);

    list->push<SaveLayerOp>(red_paint);
    list->push<TranslateOp>(static_cast<float>(offset.x()),
                            static_cast<float>(offset.y()));
    list->push<DrawRectOp>(SkRect::MakeWH(4, 4), red_paint);
    list->push<RestoreOp>();

    list->EndPaintOfUnpaired(bounds);
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  list->Finalize();

  // Pass |true| to ask for PaintOps to be included.
  std::unique_ptr<base::Value> root = ToBaseValue(list.get(), true);
  const base::Value::Dict* root_dict = root->GetIfDict();
  ASSERT_NE(nullptr, root_dict);
  // The traced value has a params dictionary as its root.
  {
    const base::Value::Dict* params_dict = root_dict->FindDict("params");
    ASSERT_NE(nullptr, params_dict);

    // The real contents of the traced value is in here.
    {
      const base::Value::List* layer_rect_list =
          params_dict->FindList("layer_rect");
      // The layer_rect field is present and has the bounds of the rtree.
      ASSERT_NE(nullptr, layer_rect_list);
      EXPECT_TRACED_RECT(2, 3, 8, 9, layer_rect_list);

      // The items list has 3 things in it since we built 3 visual rects.
      const base::Value::List* items = params_dict->FindList("items");
      ASSERT_NE(nullptr, items);
      ASSERT_EQ(7u, items->size());

      const char* expected_names[] = {
          "SaveOp",     "ConcatOp",  "SaveLayerOp", "TranslateOp",
          "DrawRectOp", "RestoreOp", "RestoreOp"};
      bool expected_has_skp[] = {false, true, true, true, true, false, false};

      for (int i = 0; i < 7; ++i) {
        const base::Value& item_value = (*items)[i];
        ASSERT_TRUE(item_value.is_dict());
        const base::Value::Dict& item_dict = item_value.GetDict();

        const base::Value::List* visual_rect =
            item_dict.FindList("visual_rect");
        ASSERT_NE(nullptr, visual_rect);
        EXPECT_TRACED_RECT(2, 3, 8, 9, visual_rect);

        const std::string* name = item_dict.FindString("name");
        EXPECT_NE(nullptr, name);
        EXPECT_EQ(expected_names[i], *name);

        EXPECT_EQ(expected_has_skp[i],
                  item_dict.FindString("skp64") != nullptr);
      }
    }
  }

  // Pass |false| to not include PaintOps.
  root = ToBaseValue(list.get(), false);
  root_dict = root->GetIfDict();
  ASSERT_NE(nullptr, root_dict);
  // The traced value has a params dictionary as its root.
  {
    const base::Value::Dict* params_dict = root_dict->FindDict("params");
    ASSERT_NE(nullptr, params_dict);

    // The real contents of the traced value is in here.
    {
      const base::Value::List* params_list;
      // The layer_rect field is present and has the bounds of the rtree.
      params_list = params_dict->FindList("layer_rect");
      ASSERT_NE(nullptr, params_list);
      EXPECT_TRACED_RECT(2, 3, 8, 9, params_list);

      // The items list is not present since we asked for no ops.
      params_list = params_dict->FindList("items");
      ASSERT_EQ(nullptr, params_list);
    }
  }
}

TEST_F(DisplayItemListTest, SizeEmpty) {
  auto list = base::MakeRefCounted<DisplayItemList>();
  EXPECT_EQ(0u, list->TotalOpCount());
}

TEST_F(DisplayItemListTest, SizeOne) {
  auto list = base::MakeRefCounted<DisplayItemList>();
  gfx::Rect drawing_bounds(5, 6, 1, 1);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_bounds);
  }
  EXPECT_EQ(1u, list->TotalOpCount());
}

TEST_F(DisplayItemListTest, SizeMultiple) {
  auto list = base::MakeRefCounted<DisplayItemList>();
  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  EXPECT_EQ(3u, list->TotalOpCount());
}

TEST_F(DisplayItemListTest, AppendVisualRectSimple) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // One drawing: D.

  gfx::Rect drawing_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_bounds);
  }

  EXPECT_EQ(1u, list->TotalOpCount());
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
}

TEST_F(DisplayItemListTest, AppendVisualRectEmptyBlock) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // One block: B1, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(3u, list->TotalOpCount());
  EXPECT_EQ(gfx::Rect(), list->VisualRectForTesting(0));
  EXPECT_EQ(gfx::Rect(), list->VisualRectForTesting(1));
  EXPECT_EQ(gfx::Rect(), list->VisualRectForTesting(2));
}

TEST_F(DisplayItemListTest, AppendVisualRectEmptyBlockContainingEmptyBlock) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // Two nested blocks: B1, B2, E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }

  {
    list->StartPaint();
    list->push<SaveOp>();
    list->EndPaintOfPairedBegin();
  }
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(5u, list->TotalOpCount());
  EXPECT_EQ(gfx::Rect(), list->VisualRectForTesting(0));
  EXPECT_EQ(gfx::Rect(), list->VisualRectForTesting(1));
  EXPECT_EQ(gfx::Rect(), list->VisualRectForTesting(2));
  EXPECT_EQ(gfx::Rect(), list->VisualRectForTesting(3));
  EXPECT_EQ(gfx::Rect(), list->VisualRectForTesting(4));
}

TEST_F(DisplayItemListTest, AppendVisualRectBlockContainingDrawing) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // One block with one drawing: B1, Da, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_bounds(5, 6, 1, 1);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_bounds);
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(4u, list->TotalOpCount());
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(2));
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(3));
}

TEST_F(DisplayItemListTest, AppendVisualRectBlockContainingEscapedDrawing) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // One block with one drawing: B1, Da (escapes), E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_bounds(1, 2, 3, 4);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_bounds);
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(4u, list->TotalOpCount());
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(2));
  EXPECT_EQ(drawing_bounds, list->VisualRectForTesting(3));
}

TEST_F(DisplayItemListTest,
       AppendVisualRectDrawingFollowedByBlockContainingEscapedDrawing) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // One drawing followed by one block with one drawing: Da, B1, Db (escapes),
  // E1.

  gfx::Rect drawing_a_bounds(1, 2, 3, 4);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(13, 14, 1, 1);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(5u, list->TotalOpCount());
  EXPECT_EQ(drawing_a_bounds, list->VisualRectForTesting(0));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(1));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(2));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
}

TEST_F(DisplayItemListTest, AppendVisualRectTwoBlocksTwoDrawings) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // Multiple nested blocks with drawings amidst: B1, Da, B2, Db, E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_a_bounds(5, 6, 1, 1);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ConcatOp>(SkM44());
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(7, 8, 1, 1);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  // End transform.
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  // End clip.
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(8u, list->TotalOpCount());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_EQ(drawing_a_bounds, list->VisualRectForTesting(2));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(5));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(6));
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(7));
}

TEST_F(DisplayItemListTest,
       AppendVisualRectTwoBlocksTwoDrawingsInnerDrawingEscaped) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // Multiple nested blocks with drawings amidst: B1, Da, B2, Db (escapes), E2,
  // E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_a_bounds(5, 6, 1, 1);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ConcatOp>(SkM44());
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(1, 2, 3, 4);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  // End transform.
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  // End clip.
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(8u, list->TotalOpCount());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_EQ(drawing_a_bounds, list->VisualRectForTesting(2));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(5));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(6));
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(7));
}

TEST_F(DisplayItemListTest,
       AppendVisualRectTwoBlocksTwoDrawingsOuterDrawingEscaped) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // Multiple nested blocks with drawings amidst: B1, Da (escapes), B2, Db, E2,
  // E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_a_bounds(1, 2, 3, 4);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ConcatOp>(SkM44());
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(7, 8, 1, 1);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  // End transform.
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  // End clip.
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(8u, list->TotalOpCount());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_EQ(drawing_a_bounds, list->VisualRectForTesting(2));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(5));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(6));
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(7));
}

TEST_F(DisplayItemListTest,
       AppendVisualRectTwoBlocksTwoDrawingsBothDrawingsEscaped) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // Multiple nested blocks with drawings amidst:
  // B1, Da (escapes to the right), B2, Db (escapes to the left), E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds), SkClipOp::kIntersect,
                           false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_a_bounds(13, 14, 1, 1);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<ConcatOp>(SkM44());
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(1, 2, 3, 4);
  {
    list->StartPaint();
    list->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  // End transform.
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  // End clip.
  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(8u, list->TotalOpCount());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_EQ(drawing_a_bounds, list->VisualRectForTesting(2));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(5));
  EXPECT_EQ(drawing_b_bounds, list->VisualRectForTesting(6));
  EXPECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(7));
}

TEST_F(DisplayItemListTest, VisualRectForPairsEnclosingEmptyPainting) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  // Some paired operations have drawing effect (e.g. some image filters),
  // so we should not ignore visual rect for empty painting.

  gfx::Rect visual_rect(11, 22, 33, 44);
  {
    list->StartPaint();
    list->push<SaveOp>();
    list->push<TranslateOp>(10.f, 20.f);
    list->EndPaintOfPairedBegin();
  }

  {
    list->StartPaint();
    list->EndPaintOfUnpaired(visual_rect);
  }

  {
    list->StartPaint();
    list->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(3u, list->TotalOpCount());
  EXPECT_EQ(visual_rect, list->VisualRectForTesting(0));
  EXPECT_EQ(visual_rect, list->VisualRectForTesting(1));
  EXPECT_EQ(visual_rect, list->VisualRectForTesting(2));
}

TEST_F(DisplayItemListTest, TotalOpCount) {
  auto list = base::MakeRefCounted<DisplayItemList>();
  auto sub_list = base::MakeRefCounted<DisplayItemList>();

  sub_list->StartPaint();
  sub_list->push<SaveOp>();
  sub_list->push<TranslateOp>(10.f, 20.f);
  sub_list->push<DrawRectOp>(SkRect::MakeWH(10, 20), PaintFlags());
  sub_list->push<RestoreOp>();
  sub_list->EndPaintOfUnpaired(gfx::Rect());
  EXPECT_EQ(4u, sub_list->TotalOpCount());

  list->StartPaint();
  list->push<SaveOp>();
  list->push<TranslateOp>(10.f, 20.f);
  list->push<DrawRecordOp>(sub_list->FinalizeAndReleaseAsRecordForTesting());
  list->push<RestoreOp>();
  list->EndPaintOfUnpaired(gfx::Rect());
  EXPECT_EQ(8u, list->TotalOpCount());
}

TEST_F(DisplayItemListTest, AreaOfDrawText) {
  auto list = base::MakeRefCounted<DisplayItemList>();

  SkFont font = skia::DefaultFont();
  auto text_blob1 = SkTextBlob::MakeFromString("ABCD", font);
  gfx::Size text_blob1_size(ceilf(text_blob1->bounds().width()),
                            ceilf(text_blob1->bounds().height()));
  auto text_blob1_area = text_blob1_size.width() * text_blob1_size.height();
  auto text_blob2 = SkTextBlob::MakeFromString("EFG", font);
  gfx::Size text_blob2_size(ceilf(text_blob2->bounds().width()),
                            ceilf(text_blob2->bounds().height()));
  auto text_blob2_area = text_blob2_size.width() * text_blob2_size.height();

  PaintOpBuffer sub_buffer;
  sub_buffer.push<DrawTextBlobOp>(text_blob1, 0.0f, 0.0f, PaintFlags());
  auto record = sub_buffer.ReleaseAsRecord();

  list->StartPaint();
  list->push<SaveOp>();
  list->push<TranslateOp>(100.0f, 100.0f);
  list->push<DrawRecordOp>(record);
  list->push<RestoreOp>();
  list->EndPaintOfUnpaired(gfx::Rect(gfx::Point(100, 100), text_blob1_size));

  list->StartPaint();
  list->push<SaveOp>();
  list->push<TranslateOp>(100.0f, 400.0f);
  list->push<DrawRecordOp>(record);
  list->push<RestoreOp>();
  list->EndPaintOfUnpaired(gfx::Rect(gfx::Point(100, 400), text_blob1_size));

  list->StartPaint();
  list->push<DrawTextBlobOp>(text_blob2, 10.0f, 20.0f, PaintFlags());
  list->EndPaintOfUnpaired(gfx::Rect(text_blob2_size));

  list->StartPaint();
  list->push<DrawTextBlobOp>(text_blob2, 400.0f, 100.0f, PaintFlags());
  list->EndPaintOfUnpaired(gfx::Rect(gfx::Point(400, 100), text_blob2_size));

  list->StartPaint();
  list->push<DrawRectOp>(SkRect::MakeXYWH(400, 100, 100, 100), PaintFlags());
  list->EndPaintOfUnpaired(gfx::Rect(400, 100, 100, 100));

  list->Finalize();
  // This includes the DrawTextBlobOp in the first DrawRecordOp the the first
  // direct DrawTextBlobOp.
  EXPECT_EQ(static_cast<int>(text_blob1_area + text_blob2_area),
            static_cast<int>(list->AreaOfDrawText(gfx::Rect(200, 200))));
  // This includes all DrawTextBlobOps.
  EXPECT_EQ(static_cast<int>(text_blob1_area * 2 + text_blob2_area * 2),
            static_cast<int>(list->AreaOfDrawText(gfx::Rect(500, 500))));
}

}  // namespace cc
