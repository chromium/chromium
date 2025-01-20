// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // To get M_PI on Windows.

#include "chrome/browser/glic/border_view.h"

#include <math.h>

#include "chrome/browser/glic/glic_keyed_service_factory.h"
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
namespace {
constexpr static int kBorderWidthMin = 1;
constexpr static int kBorderWidthMax = 5;

constexpr static base::TimeDelta kAnimationDuration = base::Seconds(2);

// Maps `progress` in [0, 1] to its radian value in [0, M_PI/2].
float GetProgressInRadian(base::TimeDelta since_first_frame) {
  return (since_first_frame / kAnimationDuration) * (M_PI / 2);
}

SkV4 SkRGBA4fToSkV4(SkRGBA4f<kPremul_SkAlphaType> color) {
  return SkV4{color.fR, color.fG, color.fB, color.fA};
}

}  // namespace

class BorderView::BorderViewUpdater {
 public:
  explicit BorderViewUpdater(Browser* browser, BorderView* border_view)
      : border_view_(border_view), browser_(browser) {
    auto* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());

    // Subscribe to changes in the focus tab.
    focus_change_subscription_ = glic_service->AddFocusedTabChangedCallback(
        base::BindRepeating(&BorderView::BorderViewUpdater::OnFocusedTabChanged,
                            base::Unretained(this)));

    // Subscribe to changes in the context access indicator status.
    indicator_change_subscription_ =
        glic_service->AddContextAccessIndicatorStatusChangedCallback(
            base::BindRepeating(
                &BorderView::BorderViewUpdater::OnIndicatorStatusChanged,
                base::Unretained(this)));
  }
  BorderViewUpdater(const BorderViewUpdater&) = delete;
  BorderViewUpdater& operator=(const BorderViewUpdater&) = delete;
  ~BorderViewUpdater() = default;

  // Called when the focused tab changes.
  void OnFocusedTabChanged(const content::WebContents* contents) {
    if (contents) {
      focused_tab_ = const_cast<content::WebContents*>(contents)->GetWeakPtr();
    } else {
      focused_tab_.reset();
    }
    UpdateBorderView();
  }

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled) {
    if (context_access_indicator_enabled_ == enabled) {
      return;
    }
    context_access_indicator_enabled_ = enabled;
    UpdateBorderView();
  }

 private:
  // Updates the BorderView UI effect given the current state of the focused tab
  // and context access indicator flag.
  void UpdateBorderView() {
    border_view_->CancelAnimation();

    if (!context_access_indicator_enabled_ || !focused_tab_) {
      return;
    }
    auto* const model = browser_->GetTabStripModel();
    CHECK(model);
    const int index = model->GetIndexOfWebContents(focused_tab_.get());
    if (index == TabStripModel::kNoTab) {
      return;
    }
    auto* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());
    CHECK(service);
    if (!service->window_controller().HasWindow()) {
      return;
    }
    border_view_->StartAnimation();
  }

  // Back pointer to the owner. Guaranteed to outlive `this`.
  const raw_ptr<BorderView> border_view_;

  // Owned by `BrowserView`. Outlives all the children of the `BrowserView`.
  const raw_ptr<BrowserWindowInterface> browser_;

  // Tracked states and their subscriptions.
  base::WeakPtr<const content::WebContents> focused_tab_;
  base::CallbackListSubscription focus_change_subscription_;
  bool context_access_indicator_enabled_ = false;
  base::CallbackListSubscription indicator_change_subscription_;
};

BorderView::BorderView(Browser* browser)
    : updater_(std::make_unique<BorderViewUpdater>(browser, this)) {
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser->GetProfile());
  // Post-initialization updates. Don't do the update in the updater's ctor
  // because at that time BorderView isn't fully initialized, which can lead to
  // undefined behavior.
  //
  // Fetch the latest context access indicator status from service. We can't
  // assume the WebApp always updates the status on the service (thus the new
  // subscribers not getting the latest value).
  updater_->OnIndicatorStatusChanged(
      glic_service->is_context_access_indicator_enabled());
}

BorderView::~BorderView() = default;

void BorderView::OnPaint(gfx::Canvas* canvas) {
  if (!compositor_) {
    return;
  }

  float border_width =
      (kBorderWidthMin + ((kBorderWidthMax - kBorderWidthMin) * progress_));
  SkColor4f border_color =
      SkColor4f::FromColor(GetColorProvider()->GetColor(ui::kColorSysPrimary));
  border_color.fA = progress_;

  const std::string_view kDrawRect(R"(
      const float4 transparent = vec4(0);
      uniform float2 u_top_left;
      uniform float2 u_btm_right;
      uniform float4 u_border_color;
      vec4 main(float2 coord) {
        if (all(greaterThanEqual(coord, u_top_left)) &&
            all(lessThan(coord, u_btm_right))) {
          return transparent;
        } else {
          return u_border_color;
        }
      }
    )");

  std::vector<cc::PaintShader::Float2Uniform> float2_uniforms = {
      {.name = SkString("u_top_left"),
       .value = SkV2{bounds().origin().x() + border_width,
                     bounds().origin().y() + border_width}},
      {.name = SkString("u_btm_right"),
       .value = SkV2{bounds().bottom_right().x() - border_width,
                     bounds().bottom_right().y() - border_width}}};
  std::vector<cc::PaintShader::Float4Uniform> float4_uniforms = {
      {.name = SkString("u_border_color"),
       .value = SkRGBA4fToSkV4(border_color.premul())}};

  views::View::OnPaint(canvas);
  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeSkSLCommand(
      kDrawRect, /*float_uniforms=*/{}, std::move(float2_uniforms),
      std::move(float4_uniforms)));
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
