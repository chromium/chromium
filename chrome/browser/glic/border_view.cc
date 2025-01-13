// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // To get M_PI on Windows.

#include "chrome/browser/glic/border_view.h"

#include <math.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view_class_properties.h"

namespace glic {

// Note: |           |           |
//       |<-- 5px -->|<-- 5px -->|
//       |  outside  |  visible  |
//
// So only half of the full width are inside the visible viewport.
constexpr static int kBorderWidthMin = 2;
constexpr static int kBorderWidthMax = 10;

constexpr static base::TimeDelta kAnimationDuration = base::Seconds(2);

// Maps `progress` in [0, 1] to its radian value in [0, M_PI/2].
float GetProgressInRadian(base::TimeDelta since_first_frame) {
  return (since_first_frame / kAnimationDuration) * (M_PI / 2);
}

// static
BorderView* BorderView::FindBorderForWebContents(
    const content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser || !browser->window()) {
    // We might not have a browser or browser window in unittests.
    return nullptr;
  }
  return browser->GetBrowserView().glic_border();
}

// static.
void BorderView::CancelAnimation(BrowserWindowInterface* browser_interface) {
  BorderView* glic_border =
      static_cast<Browser*>(browser_interface)->GetBrowserView().glic_border();
  CHECK(glic_border);
  glic_border->CancelAnimation();
}

BorderView::BorderView() = default;

BorderView::~BorderView() = default;

void BorderView::OnPaint(gfx::Canvas* canvas) {
  if (!compositor_) {
    return;
  }

  int border_width =
      kBorderWidthMin + ((kBorderWidthMax - kBorderWidthMin) * progress_);

  views::View::OnPaint(canvas);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(GetColorProvider()->GetColor(ui::kColorSysPrimary));
  flags.setStrokeWidth(border_width);
  flags.setAlphaf(progress_);
  canvas->DrawRect(gfx::RectF(bounds()), flags);
}

void BorderView::OnAnimationStep(base::TimeTicks timestamp) {
  if (first_frame_time_.is_null()) {
    first_frame_time_ = timestamp;
  }

  base::TimeDelta since_first_frame = timestamp - first_frame_time_;
  float progress_in_radian = GetProgressInRadian(since_first_frame);

  if (progress_in_radian > M_PI / 2) {
    progress_ = 1;
    return;
  }

  progress_ = sin(progress_in_radian);

  SchedulePaint();
}

void BorderView::OnCompositingShuttingDown(ui::Compositor* compositor) {
  CancelAnimation();
}

void BorderView::StartAnimation() {
  if (compositor_) {
    // The user can click on the glic icon after the window is shown. The
    // animation is already playing at that time.
    return;
  }

  if (!parent()) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetVisible(true);

  ui::Compositor* compositor = layer()->GetCompositor();
  if (!compositor) {
    base::debug::DumpWithoutCrashing();
    return;
  }
  compositor_ = compositor;
  compositor_->AddAnimationObserver(this);
}

void BorderView::CancelAnimation() {
  if (!compositor_) {
    return;
  }

  compositor_->RemoveAnimationObserver(this);
  compositor_ = nullptr;
  progress_ = 0.f;
  first_frame_time_ = base::TimeTicks();

  // `DestroyLayer()` schedules another paint to repaint the affected area by
  // the destroyed layer.
  DestroyLayer();
  SetVisible(false);
}

BEGIN_METADATA(BorderView)
END_METADATA

}  // namespace glic
