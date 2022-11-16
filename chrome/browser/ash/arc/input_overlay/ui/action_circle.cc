// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_circle.h"

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"

namespace arc::input_overlay {
namespace {
constexpr SkColor kViewColor = SK_ColorTRANSPARENT;
constexpr SkColor kEditDefaultColor = SkColorSetA(SK_ColorWHITE, 0x80);
constexpr SkColor kEditedColor = gfx::kGoogleRed300;

constexpr int kViewStroke = 1;
constexpr int kEditDefaultStroke = 2;
constexpr int kEditedStroke = 3;
}  // namespace

class ActionCircle::CircleBackground : public views::Background {
 public:
  explicit CircleBackground(SkColor color, int stroke) {
    SetNativeControlColor(color);
    stroke_ = stroke;
  }
  ~CircleBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    int radius = view->bounds().width() / 2;
    gfx::PointF center(radius, radius);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(stroke_);
    flags.setColor(get_color());
    canvas->DrawCircle(center, radius - stroke_, flags);
  }

 private:
  int stroke_;
};

ActionCircle::ActionCircle(int radius) : views::View() {
  SetSize(gfx::Size(radius * 2, radius * 2));
}

ActionCircle::~ActionCircle() = default;

void ActionCircle::SetDisplayMode(DisplayMode mode) {
  if (current_mode_ == mode)
    return;

  switch (mode) {
    case DisplayMode::kView:
      SetBackground(
          std::make_unique<CircleBackground>(kViewColor, kViewStroke));
      break;
    case DisplayMode::kEdit:
      SetBackground(std::make_unique<CircleBackground>(kEditDefaultColor,
                                                       kEditDefaultStroke));
      break;
    case DisplayMode::kEditedSuccess:
    case DisplayMode::kEditedUnbound:
      SetBackground(
          std::make_unique<CircleBackground>(kEditedColor, kEditedStroke));
      break;
    default:
      NOTREACHED();
      break;
  }

  current_mode_ = mode;
}

}  // namespace arc::input_overlay
