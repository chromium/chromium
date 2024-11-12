// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_focus_indicator.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace ash {
namespace {

constexpr int kQuickInsertFocusIndicatorWidth = 3;

constexpr SkScalar kQuickInsertFocusIndicatorRadius =
    SkIntToScalar(kQuickInsertFocusIndicatorWidth);
constexpr SkScalar kQuickInsertFocusIndicatorRadii[8] = {
    0,
    0,  // top-left
    kQuickInsertFocusIndicatorRadius,
    kQuickInsertFocusIndicatorRadius,  // top-right
    kQuickInsertFocusIndicatorRadius,
    kQuickInsertFocusIndicatorRadius,  // bottom-right
    0,
    0};  // bottom-left

}  // namespace

void PaintPickerFocusIndicator(gfx::Canvas* canvas,
                               const gfx::Point& origin,
                               int height,
                               SkColor color) {
  SkPath path;
  const gfx::Rect focus_indicator_bounds(
      origin, gfx::Size(kQuickInsertFocusIndicatorWidth, height));
  path.addRoundRect(gfx::RectToSkRect(focus_indicator_bounds),
                    kQuickInsertFocusIndicatorRadii);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color);
  canvas->DrawPath(path, flags);
}

}  // namespace ash
