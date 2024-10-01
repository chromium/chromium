// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_region_overlay_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/scanner/scanner_text.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace ash {

namespace {

TEST(CaptureRegionOverlayControllerTest, PaintsDetectedTextRegions) {
  // Initialize an entirely green 100 x 100 canvas.
  constexpr gfx::Rect kCanvasBounds(0, 0, 100, 100);
  gfx::Canvas canvas(kCanvasBounds.size(), /*image_scale=*/1.0f,
                     /*is_opaque=*/false);
  canvas.DrawColor(SK_ColorGREEN);
  // Set and paint a detected text region which:
  // - is centered at (40, 50) on the canvas,
  // - has size 40 x 8 before rotation, then
  // - is rotated clockwise by 45 degrees.
  CaptureRegionOverlayController capture_region_overlay_controller;
  ScannerText text(u"text content");
  ScannerText::Paragraph& paragraph = text.AppendParagraph();
  constexpr gfx::Point kTextRegionCenter(40, 50);
  paragraph.AppendLine(gfx::Range(0, 12), ScannerText::CenterRotatedBox{
                                              .center = kTextRegionCenter,
                                              .size = gfx::Size(40, 8),
                                              .rotation = 45});
  capture_region_overlay_controller.OnTextDetected(std::move(text));
  capture_region_overlay_controller.PaintCaptureRegionOverlay(
      canvas, /*region=*/kCanvasBounds);

  // Check that points outside the detected text region remain green.
  // Above the text region.
  EXPECT_EQ(canvas.GetBitmap().getColor(kTextRegionCenter.x(),
                                        kTextRegionCenter.y() - 10),
            SK_ColorGREEN);
  // Below the text region.
  EXPECT_EQ(canvas.GetBitmap().getColor(kTextRegionCenter.x(),
                                        kTextRegionCenter.y() + 10),
            SK_ColorGREEN);
  // Left of the text region.
  EXPECT_EQ(canvas.GetBitmap().getColor(kTextRegionCenter.x() - 10,
                                        kTextRegionCenter.y()),
            SK_ColorGREEN);
  // Right of the text region.
  EXPECT_EQ(canvas.GetBitmap().getColor(kTextRegionCenter.x() + 10,
                                        kTextRegionCenter.y()),
            SK_ColorGREEN);
  // Check that points inside the detected text region have been painted a
  // different color.
  // Center of text region.
  EXPECT_NE(
      canvas.GetBitmap().getColor(kTextRegionCenter.x(), kTextRegionCenter.y()),
      SK_ColorGREEN);
  // Top left part of text region.
  EXPECT_NE(canvas.GetBitmap().getColor(kTextRegionCenter.x() - 10,
                                        kTextRegionCenter.y() - 10),
            SK_ColorGREEN);
  // Bottom right part of text region.
  EXPECT_NE(canvas.GetBitmap().getColor(kTextRegionCenter.x() + 10,
                                        kTextRegionCenter.y() + 10),
            SK_ColorGREEN);
}

}  // namespace

}  // namespace ash
