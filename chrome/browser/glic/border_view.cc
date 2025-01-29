// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // To get M_PI on Windows.

#include "chrome/browser/glic/border_view.h"

#include <math.h>

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view_class_properties.h"

namespace glic {
namespace {
constexpr static float kBorderWidth = 5.0f;

// The amount of time for the opacity to go from 0 to 1.
constexpr static base::TimeDelta kOpacityRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the border empasis to go from 0 the max, and from max
// back to 0 (symmetical).
constexpr static base::TimeDelta kEmphasisRampDuration =
    base::Milliseconds(500);
// The amount of time for the border to stay emphasized.
constexpr static base::TimeDelta kEmphasisDuration = base::Milliseconds(1500);

SkV4 SkRGBA4fToSkV4(SkRGBA4f<kPremul_SkAlphaType> color) {
  return SkV4{color.fR, color.fG, color.fB, color.fA};
}

float ClampAndInterpolate(gfx::Tween::Type type,
                          float t,
                          float low,
                          float high) {
  float clamp_lo = std::min(low, high);
  float clamp_hi = std::max(low, high);
  float clamped = std::clamp(t, clamp_lo, clamp_hi);
  // Interpolate `clamped` within [low, high], using the function `type`.
  double calculated = gfx::Tween::CalculateValue(type, clamped);
  // Linear project `calculated` onto [low, high].
  return gfx::Tween::FloatValueBetween(calculated, low, high);
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
    auto* previous_focus = glic_focused_contents_in_current_window_.get();
    if (contents && IsTabInCurrentWindow(contents)) {
      glic_focused_contents_in_current_window_ =
          const_cast<content::WebContents*>(contents)->GetWeakPtr();
    } else if (!contents) {
      if (!IsGlicWindowShowing() || !context_access_indicator_enabled_) {
        // TODO(crbug.com/384517660): Focused tab is truly lost when glic window
        // isn't showing or the context sharing is disabled.
        glic_focused_contents_in_current_window_.reset();
      }
    } else {
      glic_focused_contents_in_current_window_.reset();
    }
    if (previous_focus && glic_focused_contents_in_current_window_ &&
        previous_focus != glic_focused_contents_in_current_window_.get()) {
      UpdateBorderView(UpdateBorderReason::kFocusedTabChanged_NoFocusChange);
    } else if (!previous_focus && glic_focused_contents_in_current_window_) {
      UpdateBorderView(UpdateBorderReason::kFocusedTabChanged_GainFocus);
    } else if (previous_focus && !glic_focused_contents_in_current_window_) {
      UpdateBorderView(UpdateBorderReason::kFocusedTabChanged_LostFocus);
    }
  }

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled) {
    if (context_access_indicator_enabled_ == enabled) {
      return;
    }
    context_access_indicator_enabled_ = enabled;
    UpdateBorderView(context_access_indicator_enabled_
                         ? UpdateBorderReason::kContextAccessIndicatorOn
                         : UpdateBorderReason::kContextAccessIndicatorOff);
  }

 private:
  // Updates the BorderView UI effect given the current state of the focused tab
  // and context access indicator flag.
  enum class UpdateBorderReason {
    kContextAccessIndicatorOn = 0,
    kContextAccessIndicatorOff,

    // Tab focus changes in the same window.
    kFocusedTabChanged_NoFocusChange,
    // Focus changes across different application windows.
    kFocusedTabChanged_GainFocus,
    kFocusedTabChanged_LostFocus,
  };
  void UpdateBorderView(UpdateBorderReason reason) {
    switch (reason) {
      case UpdateBorderReason::kContextAccessIndicatorOn: {
        // Off to On. Throw away everything we have and start the animation from
        // the beginning.
        border_view_->CancelAnimation();
        if (ShouldShowBorderAnimation()) {
          border_view_->StartAnimation();
        }
        break;
      }
      case UpdateBorderReason::kContextAccessIndicatorOff: {
        // TODO(baranerf): Implement this path by ramping the opacity down.
        border_view_->CancelAnimation();
        break;
      }
      case UpdateBorderReason::kFocusedTabChanged_NoFocusChange: {
        if (ShouldShowBorderAnimation()) {
          border_view_->ResetEmphasisAndReplay();
        }
        break;
      }
      // It's hard to know if the user has changed the focus from this chrome
      // window to a different chrome window or a different app. For now, just
      // cancel the animation and restart from t0 for the cross-window focus
      // change.
      // TODO(crbug.com/392641313): Confirm with UX if the user will ever notice
      // the animation restart at all, in the cross-window focus change case.
      case UpdateBorderReason::kFocusedTabChanged_GainFocus: {
        border_view_->CancelAnimation();
        if (ShouldShowBorderAnimation()) {
          border_view_->StartAnimation();
        }
        break;
      }
      case UpdateBorderReason::kFocusedTabChanged_LostFocus: {
        // TODO(baranerf): Implement this path by ramping the opacity down.
        border_view_->CancelAnimation();
        break;
      }
    }
  }

  bool IsGlicWindowShowing() const {
    auto* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());
    CHECK(service);
    return service->window_controller().IsShowing();
  }

  bool IsTabInCurrentWindow(const content::WebContents* tab) const {
    auto* model = browser_->GetTabStripModel();
    CHECK(model);
    int index = model->GetIndexOfWebContents(tab);
    return index != TabStripModel::kNoTab;
  }

  bool ShouldShowBorderAnimation() {
    if (!context_access_indicator_enabled_ ||
        !glic_focused_contents_in_current_window_) {
      return false;
    }
    return IsGlicWindowShowing();
  }

  // Back pointer to the owner. Guaranteed to outlive `this`.
  const raw_ptr<BorderView> border_view_;

  // Owned by `BrowserView`. Outlives all the children of the `BrowserView`.
  const raw_ptr<BrowserWindowInterface> browser_;

  // Tracked states and their subscriptions.
  //
  // NOTE: this tab remains valid if the user focuses on a non-Chrome window
  // with glic window shown, meaning the animation will be playing in the
  // non-focused Chrome window.
  base::WeakPtr<const content::WebContents>
      glic_focused_contents_in_current_window_;
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

  SkColor4f border_color =
      SkColor4f::FromColor(GetColorProvider()->GetColor(ui::kColorSysOutline));

  // TODO(crbug.com/392656256): Include the rounded bottom corners for Mac.
  const std::string_view kDrawRect(R"(
      const float4 transparent = vec4(0);
      const float max_extent = 25.0;
      uniform float u_emphasis;
      uniform float u_opacity;
      uniform float2 u_top_left;
      uniform float2 u_btm_right;
      uniform float4 u_border_color;

      float BorderDistance(float2 coord) {
        float left = coord.x - u_top_left.x;
        float top = coord.y - u_top_left.y;
        float right = u_btm_right.x - coord.x;
        float bottom = u_btm_right.y - coord.y;

        float extent = max_extent * u_emphasis;
        if (top < extent && left < extent) {
          return extent - distance(coord,
              vec2(u_top_left.x + extent,
                   u_top_left.y + extent));
        } else if (top < extent && right < extent) {
          return extent - distance(coord,
              vec2(u_btm_right.x - extent,
                   u_top_left.y + extent));
        } else if (bottom < extent && left < extent) {
          return extent - distance(coord,
              vec2(u_top_left.x + extent,
                   u_btm_right.y - extent));
        } else if (bottom < extent && right < extent) {
          return extent - distance(coord,
              vec2(u_btm_right.x - extent,
                   u_btm_right.y - extent));
        }
        return min(min(min(left, top), right), bottom);
      }

      vec4 main(float2 coord) {
        // Apply the opacity.
        float4 adjusted_color = u_border_color * u_opacity;
        adjusted_color.w = u_opacity;

        if (all(greaterThanEqual(coord, u_top_left)) &&
            all(lessThan(coord, u_btm_right))) {
          if (u_emphasis <= 0.0) {
            return transparent;
          } else {
            float extent = max_extent * u_emphasis;
            float delta = BorderDistance(coord);
            float opacity = 1.0 - min(max(delta / extent, 0.0), 1.0);
            opacity *= opacity;
            return adjusted_color * opacity;
          }
        } else {
          return adjusted_color;
        }
      }
    )");

  std::vector<cc::PaintShader::FloatUniform> float_uniforms = {
      {.name = SkString("u_emphasis"), .value = SkScalar(emphasis_)},
      {.name = SkString("u_opacity"), .value = SkScalar(opacity_)}};
  std::vector<cc::PaintShader::Float2Uniform> float2_uniforms = {
      {.name = SkString("u_top_left"),
       .value = SkV2{bounds().origin().x() + kBorderWidth,
                     bounds().origin().y() + kBorderWidth}},
      {.name = SkString("u_btm_right"),
       .value = SkV2{bounds().bottom_right().x() - kBorderWidth,
                     bounds().bottom_right().y() - kBorderWidth}}};
  std::vector<cc::PaintShader::Float4Uniform> float4_uniforms = {
      {.name = SkString("u_border_color"),
       .value = SkRGBA4fToSkV4(border_color.premul())}};

  views::View::OnPaint(canvas);
  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeSkSLCommand(
      kDrawRect, std::move(float_uniforms), std::move(float2_uniforms),
      std::move(float4_uniforms), {}));
  canvas->DrawRect(gfx::RectF(bounds()), flags);
}

void BorderView::OnAnimationStep(base::TimeTicks timestamp) {
  if (tester_) [[unlikely]] {
    timestamp = tester_->GetTestTimestamp();
  }
  if (first_frame_time_.is_null()) {
    first_frame_time_ = timestamp;
  }
  if (first_emphasis_frame_.is_null()) {
    first_emphasis_frame_ = timestamp;
  }

  base::TimeDelta emphasis_since_first_frame =
      timestamp - first_emphasis_frame_;
  emphasis_ = GetEmphasis(emphasis_since_first_frame);
  base::TimeDelta opacity_since_first_frame = timestamp - first_frame_time_;
  opacity_ = GetOpacity(opacity_since_first_frame);

  bool emphasis_done =
      emphasis_ == 0.f && !emphasis_since_first_frame.is_zero();
  bool opacity_done = opacity_ == 1.f && !opacity_since_first_frame.is_zero();

  // Don't animate if:
  // - `skip_animation_` is explicitly toggled.
  // - The animations have exhausted.
  // We shouldn't be an observer for more than 60 seconds
  // (CompositorAnimationObserver::NotifyFailure()).
  bool remove_animation_observer =
      skip_animation_ || (emphasis_done && opacity_done);
  if (remove_animation_observer) {
    // If skipping the animation the class does not need to be an animation
    // observer.
    compositor_->RemoveAnimationObserver(this);
    return;
  }

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

  skip_animation_ = gfx::Animation::PrefersReducedMotion();

  ui::Compositor* compositor = layer()->GetCompositor();
  if (!compositor) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  compositor_ = compositor;
  compositor_->AddAnimationObserver(this);
  compositor_->AddObserver(this);
}

void BorderView::CancelAnimation() {
  if (!compositor_) {
    return;
  }

  compositor_->RemoveObserver(this);
  compositor_->RemoveAnimationObserver(this);
  compositor_ = nullptr;
  first_frame_time_ = base::TimeTicks{};
  first_emphasis_frame_ = base::TimeTicks{};
  opacity_ = 0.f;
  emphasis_ = 0.f;

  // `DestroyLayer()` schedules another paint to repaint the affected area by
  // the destroyed layer.
  DestroyLayer();
  SetVisible(false);
}

float BorderView::GetEmphasis(base::TimeDelta delta) const {
  if (skip_animation_) {
    return 0.f;
  }
  static constexpr base::TimeDelta kRampUpAndSteady =
      kEmphasisRampDuration + kEmphasisDuration;
  if (delta < kRampUpAndSteady) {
    auto target = static_cast<float>(delta / kRampUpAndSteady);
    return ClampAndInterpolate(gfx::Tween::Type::EASE_OUT, target, 0, 1);
  }
  auto target =
      static_cast<float>((delta - kRampUpAndSteady) / kEmphasisRampDuration);
  return ClampAndInterpolate(gfx::Tween::Type::EASE_OUT, target, 1, 0);
}

void BorderView::ResetEmphasisAndReplay() {
  CHECK(compositor_);
  CHECK(compositor_->HasObserver(this));
  if (!compositor_->HasAnimationObserver(this)) {
    compositor_->AddAnimationObserver(this);
  }
  first_emphasis_frame_ = base::TimeTicks{};
  SchedulePaint();
}

float BorderView::GetOpacity(base::TimeDelta delta) const {
  if (skip_animation_) {
    return 1.0f;
  }
  if (delta < kOpacityRampUpDuration) {
    return static_cast<float>(delta.InMillisecondsF() /
                              kOpacityRampUpDuration.InMillisecondsF());
  }
  return 1.0f;
}

BEGIN_METADATA(BorderView)
END_METADATA

}  // namespace glic
