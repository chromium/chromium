// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using devtools::DockSide;

TEST(DevToolsContentsResizingStrategyTest, EmptyStrategy) {
  DevToolsContentsResizingStrategy strategy;
  gfx::Rect container_bounds(0, 0, 1000, 800);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 1000, 800), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 800), contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, UndockedExplicitNoneWithBounds) {
  // Strategy has DockSide::kNone and explicit bounds (e.g. Device Mode
  // emulation). Inspected page respects specified bounds (700x800).
  DevToolsContentsResizingStrategy strategy(DockSide::kNone,
                                            gfx::Rect(0, 0, 700, 800));
  gfx::Rect container_bounds(0, 0, 1000, 800);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 1000, 800), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 700, 800), contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, DockedRightNormalBounds) {
  // Container 1000x800, Inspected page 700x800, DevTools 300x800 on right.
  DevToolsContentsResizingStrategy strategy(DockSide::kRight,
                                            gfx::Rect(0, 0, 700, 800));
  gfx::Rect container_bounds(0, 0, 1000, 800);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 1000, 800), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 700, 800), contents_bounds);
  // DevTools receives remaining width 300px (1000 - 700).
}

TEST(DevToolsContentsResizingStrategyTest, DockedRightContainerGrows) {
  // Original Strategy: Inspected page 700x800 (when container was 1000x800).
  // User manually resizes window outwards to 1200x800.
  // Inspected page remains at 700x800 until JS sends new bounds; DevTools gets
  // remaining 500px (1200 - 700).
  DevToolsContentsResizingStrategy strategy(DockSide::kRight,
                                            gfx::Rect(0, 0, 700, 800));
  gfx::Rect container_bounds(0, 0, 1200, 800);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 1200, 800), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 700, 800), contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, DockedRightContainerShrinks) {
  // Original Strategy: Inspected page 700x800 (when container was 1000x800).
  // Container shrinks to 650x800.
  // DevTools should retain at least 250px width, so inspected page should be at
  // most 400px (650 - 250).
  DevToolsContentsResizingStrategy strategy(DockSide::kRight,
                                            gfx::Rect(0, 0, 700, 800));
  gfx::Rect container_bounds(0, 0, 650, 800);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 650, 800), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 400, 800), contents_bounds);
  // DevTools retains 250px (650 - 400), preventing it from collapsing to 0.
}

TEST(DevToolsContentsResizingStrategyTest, DockedBottomContainerShrinks) {
  // Original Strategy: Inspected page 1000x600 (when container was 1000x800).
  // Container height shrinks to 650px.
  // DevTools should retain at least 72px height, so inspected page height
  // should be at most 578px (650 - 72).
  DevToolsContentsResizingStrategy strategy(DockSide::kBottom,
                                            gfx::Rect(0, 0, 1000, 600));
  gfx::Rect container_bounds(0, 0, 1000, 650);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 1000, 650), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 578), contents_bounds);
  // DevTools retains 72px height (650 - 578).
}

TEST(DevToolsContentsResizingStrategyTest, DockedBottomHeightLessThanBounds) {
  // Original Strategy: Inspected page 1000x600 (when container was 1000x800).
  // Container height shrinks to 550px (< bounds.height() 600px).
  // DevTools should still retain at least 72px height, so inspected page
  // height should be clamped to 478px (550 - 72).
  DevToolsContentsResizingStrategy strategy(DockSide::kBottom,
                                            gfx::Rect(0, 0, 1000, 600));
  gfx::Rect container_bounds(0, 0, 1000, 550);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 1000, 550), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 478), contents_bounds);
  // DevTools retains 72px height (550 - 478).
}

TEST(DevToolsContentsResizingStrategyTest, DockedContainerShrinksBothAxes) {
  // Original Strategy: Inspected page 700x800 (when container was 1000x800).
  // Container shrinks both horizontally and vertically to 600x500.
  // DevTools right retains 250px width (600 - 250 = 350px page width),
  // and height scales to full container height 500px.
  DevToolsContentsResizingStrategy strategy(DockSide::kRight,
                                            gfx::Rect(0, 0, 700, 800));
  gfx::Rect container_bounds(0, 0, 600, 500);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 350, 500), contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, DockedRightVerySmallContainer) {
  // Container width (100px) is below minimum DevTools width (250px).
  // DevTools gets full container width (100px), inspected page width clamped to
  // 0.
  DevToolsContentsResizingStrategy strategy(DockSide::kRight,
                                            gfx::Rect(0, 0, 700, 800));
  gfx::Rect container_bounds(0, 0, 100, 800);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 100, 800), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 0, 800), contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, DockedBottomVerySmallContainer) {
  // Container height (50px) is below minimum DevTools height (72px).
  // DevTools gets full container height (50px), inspected page height clamped
  // to 0.
  DevToolsContentsResizingStrategy strategy(DockSide::kBottom,
                                            gfx::Rect(0, 0, 1000, 600));
  gfx::Rect container_bounds(0, 0, 1000, 50);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 1000, 50), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 0), contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, DockedBottomExplicitBounds) {
  // When DevTools is docked bottom, explicit bounds (e.g. Device Mode
  // emulation) are preserved.
  DevToolsContentsResizingStrategy strategy(DockSide::kBottom,
                                            gfx::Rect(0, 0, 700, 600));
  gfx::Rect container_bounds(0, 0, 1000, 800);
  gfx::Rect devtools_bounds;
  gfx::Rect contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy, container_bounds,
                                        &devtools_bounds, &contents_bounds);

  EXPECT_EQ(gfx::Rect(0, 0, 1000, 800), devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 700, 600), contents_bounds);
}
