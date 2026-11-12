// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_image_annotator.h"

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace ttc {
namespace {

// A 2x viewport of 400x300 DIPs, filled with a flat blue.
constexpr float kScaleFactor = 2.0f;
constexpr SkColor kPageColor = SkColorSetRGB(0x20, 0x60, 0xE0);

SkBitmap MakeViewport() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(800, 600, /*isOpaque=*/true);
  bitmap.eraseColor(kPageColor);
  return bitmap;
}

SkBitmap Decode(const VisualContent& content) {
  EXPECT_EQ(content.mime_type, "image/jpeg");
  SkBitmap bitmap = gfx::JPEGCodec::Decode(content.image_data);
  EXPECT_FALSE(bitmap.isNull());
  return bitmap;
}

// JPEG is lossy, and the marks are antialiased, so classify colors loosely.
bool IsReddish(SkColor c) {
  return SkColorGetR(c) > 180 && SkColorGetG(c) < 100 && SkColorGetB(c) < 100;
}
bool IsWhitish(SkColor c) {
  return SkColorGetR(c) > 200 && SkColorGetG(c) > 200 && SkColorGetB(c) > 200;
}
// The halo is a thin white ring over the page; antialiasing and JPEG leave it
// a pale blue, lighter than the page and not red.
bool IsLightened(SkColor c) {
  return SkColorGetR(c) > 100 && SkColorGetG(c) > 130;
}
bool IsPageColor(SkColor c) {
  return SkColorGetR(c) < 80 && SkColorGetG(c) > 60 && SkColorGetG(c) < 140 &&
         SkColorGetB(c) > 180;
}

int CountReddish(const SkBitmap& image, const gfx::Rect& rect) {
  int count = 0;
  for (int y = rect.y(); y < rect.bottom(); ++y) {
    for (int x = rect.x(); x < rect.right(); ++x) {
      count += IsReddish(image.getColor(x, y));
    }
  }
  return count;
}

TEST(PointerImageAnnotatorTest, CropsToDevicePixelsAndMarksThePoint) {
  // Crop (50, 40)-(250, 140) DIP, i.e. 400x200 px; the point at (150, 90)
  // DIP is its center, (200, 100) px.
  const gfx::PointF point(150, 90);
  std::optional<VisualContent> content =
      AnnotatePointerImage(MakeViewport(), gfx::Rect(50, 40, 200, 100),
                           base::span_from_ref(point), kScaleFactor);
  ASSERT_TRUE(content);
  const SkBitmap image = Decode(*content);
  EXPECT_EQ(image.width(), 400);
  EXPECT_EQ(image.height(), 200);

  // The crosshair's arms are red out to 24 DIP = 48 px from the point; the
  // ring is at 14 DIP = 28 px with a white halo just outside it, visible
  // between the arms.
  EXPECT_TRUE(IsReddish(image.getColor(200, 100)));
  EXPECT_TRUE(IsReddish(image.getColor(240, 100)));
  EXPECT_TRUE(IsReddish(image.getColor(200, 60)));
  EXPECT_TRUE(IsLightened(image.getColor(200 + 22, 100 + 22)));
  // Away from the mark the page shows through unchanged.
  EXPECT_TRUE(IsPageColor(image.getColor(20, 20)));
  EXPECT_TRUE(IsPageColor(image.getColor(380, 180)));
  // A single point gets no badge: below the ring is still page.
  EXPECT_TRUE(IsPageColor(image.getColor(200, 100 + 58)));
}

// Several points get numbered badges. Runs on the thread pool, as the tool
// will.
TEST(PointerImageAnnotatorTest, SeveralPointsGetBadgesOffTheUiThread) {
  base::test::TaskEnvironment task_environment;
  const std::vector<gfx::PointF> points = {{100, 80}, {300, 200}};
  base::test::TestFuture<std::optional<VisualContent>> future;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AnnotatePointerImage, MakeViewport(),
                     gfx::Rect(0, 0, 400, 300), points, kScaleFactor),
      future.GetCallback());
  ASSERT_TRUE(future.Get());
  const SkBitmap image = Decode(*future.Get());
  EXPECT_EQ(image.width(), 800);
  EXPECT_EQ(image.height(), 600);

  // Each point is marked and has a white badge 18 DIP below it, 22 DIP tall,
  // with its number in red in the middle.
  for (const gfx::PointF& point : points) {
    const int x = point.x() * kScaleFactor;
    const int y = point.y() * kScaleFactor;
    EXPECT_TRUE(IsReddish(image.getColor(x, y))) << point.ToString();
    EXPECT_TRUE(IsWhitish(image.getColor(x - 15, y + 40))) << point.ToString();
    EXPECT_TRUE(IsWhitish(image.getColor(x + 15, y + 40))) << point.ToString();
    EXPECT_GT(CountReddish(image, gfx::Rect(x - 15, y + 36, 30, 44)), 0)
        << point.ToString();
  }
}

TEST(PointerImageAnnotatorTest, CropIsClippedToTheViewport) {
  // A crop running off the bottom right is cut to the part inside; one
  // entirely outside, or an empty one, gives nothing.
  const gfx::PointF point(390, 290);
  std::optional<VisualContent> content =
      AnnotatePointerImage(MakeViewport(), gfx::Rect(300, 250, 200, 100),
                           base::span_from_ref(point), kScaleFactor);
  ASSERT_TRUE(content);
  const SkBitmap image = Decode(*content);
  EXPECT_EQ(image.width(), 200);
  EXPECT_EQ(image.height(), 100);
  EXPECT_TRUE(IsReddish(image.getColor(180, 80)));

  EXPECT_FALSE(AnnotatePointerImage(MakeViewport(),
                                    gfx::Rect(450, 350, 100, 100),
                                    base::span_from_ref(point), kScaleFactor));
  EXPECT_FALSE(AnnotatePointerImage(MakeViewport(), gfx::Rect(),
                                    base::span_from_ref(point), kScaleFactor));
  EXPECT_FALSE(AnnotatePointerImage(SkBitmap(), gfx::Rect(0, 0, 10, 10),
                                    base::span_from_ref(point), kScaleFactor));
}

// A badge that would run off the bottom of the image goes above its point
// instead, and a point that is not a number is left out.
TEST(PointerImageAnnotatorTest, BadgeStaysInsideTheImage) {
  const std::vector<gfx::PointF> points = {
      {100, 290}, {300, 100}, {std::nanf(""), 50}};
  std::optional<VisualContent> content = AnnotatePointerImage(
      MakeViewport(), gfx::Rect(0, 0, 400, 300), points, kScaleFactor);
  ASSERT_TRUE(content);
  const SkBitmap image = Decode(*content);

  // The first point is 10 DIP from the bottom. Its badge, 22 DIP tall and
  // 18 DIP away, is above it: 36 to 80 px up.
  const int x = 200;
  const int y = 580;
  EXPECT_TRUE(IsWhitish(image.getColor(x - 15, y - 76)));
  EXPECT_TRUE(IsWhitish(image.getColor(x + 15, y - 76)));
  EXPECT_GT(CountReddish(image, gfx::Rect(x - 15, y - 80, 30, 44)), 0);
}

TEST(PointerImageAnnotatorTest, BadScaleGivesNothing) {
  const gfx::PointF point(5, 5);
  for (float scale_factor :
       {0.0f, -1.0f, std::nanf(""), std::numeric_limits<float>::infinity()}) {
    EXPECT_FALSE(AnnotatePointerImage(MakeViewport(), gfx::Rect(0, 0, 10, 10),
                                      base::span_from_ref(point), scale_factor))
        << scale_factor;
  }
}

}  // namespace
}  // namespace ttc
