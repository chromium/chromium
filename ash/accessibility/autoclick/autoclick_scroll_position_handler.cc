// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/autoclick/autoclick_scroll_position_handler.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AutoclickScrollPositionView : public views::View {
 public:
  AutoclickScrollPositionView();
  AutoclickScrollPositionView(const AutoclickScrollPositionView&) = delete;
  AutoclickScrollPositionView& operator=(const AutoclickScrollPositionView&) =
      delete;
  ~AutoclickScrollPositionView() override = default;

 protected:
  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;
};

AutoclickScrollPositionView::AutoclickScrollPositionView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(std::make_unique<views::ImageView>())
      ->SetImage(
          gfx::CreateVectorIcon(kAutoclickScrollIcon, 24, SK_ColorWHITE));
}

void AutoclickScrollPositionView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(gfx::kGoogleGrey600);
  canvas->DrawCircle(GetLocalBounds().CenterPoint(), width() / 2, flags);
}

// static
constexpr base::TimeDelta AutoclickScrollPositionHandler::kOpaqueTime;
constexpr base::TimeDelta AutoclickScrollPositionHandler::kFadeTime;

AutoclickScrollPositionHandler::AutoclickScrollPositionHandler(
    std::unique_ptr<views::Widget> widget)
    : widget_(std::move(widget)),
      timer_(FROM_HERE,
             kOpaqueTime,
             static_cast<gfx::Animation*>(&animation_),
             &gfx::Animation::Start) {
  widget_->SetContentsView(std::make_unique<AutoclickScrollPositionView>());
}

AutoclickScrollPositionHandler::~AutoclickScrollPositionHandler() = default;

gfx::NativeView AutoclickScrollPositionHandler::GetNativeView() {
  return widget_->GetNativeView();
}

void AutoclickScrollPositionHandler::SetScrollPointCenterInScreen(
    const gfx::Point& scroll_point_center) {
  constexpr int kBackgroundSizeDips = 32;
  gfx::Rect bounds(gfx::Size(kBackgroundSizeDips, kBackgroundSizeDips));
  bounds.set_origin(scroll_point_center -
                    bounds.CenterPoint().OffsetFromOrigin());
  widget_->SetBounds(bounds);
  widget_->Show();
  widget_->SetOpacity(1.0f);

  timer_.Reset();
}

void AutoclickScrollPositionHandler::AnimationProgressed(
    const gfx::Animation* animation) {
  constexpr float kSteadyStateOpacity = 0.5f;
  widget_->SetOpacity(gfx::Tween::FloatValueBetween(
      animation_.GetCurrentValue(), 1.0f, kSteadyStateOpacity));
}

}  // namespace ash
