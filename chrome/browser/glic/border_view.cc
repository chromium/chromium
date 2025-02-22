// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // To get M_PI on Windows.

#include "chrome/browser/glic/border_view.h"

#include <math.h>

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/view_class_properties.h"

namespace glic {
namespace {

// The amount of time for the opacity to go from 0 to 1.
constexpr static base::TimeDelta kOpacityRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the opacity to go from 1 to 0.
constexpr static base::TimeDelta kOpacityRampDownDuration =
    base::Milliseconds(200);
// The amount of time for the border empasis to go from 0 the max.
constexpr static base::TimeDelta kEmphasisRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the border empasis to go from max to 0.
constexpr static base::TimeDelta kEmphasisRampDownDuration =
    base::Milliseconds(1000);
// The amount of time for the border to stay emphasized.
constexpr static base::TimeDelta kEmphasisDuration = base::Milliseconds(1500);
// Time since creation will roll over after this time to prevent growing
// indefinitely.
constexpr static base::TimeDelta kMaxTime = base::Days(1);

bool UseDarkMode(ThemeService* theme_service) {
  // Taken from lens_overlay_theme_utils.cc
  ThemeService::BrowserColorScheme color_scheme =
      theme_service->GetBrowserColorScheme();
  return color_scheme == ThemeService::BrowserColorScheme::kSystem
             ? ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
             : color_scheme == ThemeService::BrowserColorScheme::kDark;
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
        if (border_view_->compositor_) {
          border_view_->StartRampingDown();
        }
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
        if (border_view_->compositor_) {
          border_view_->StartRampingDown();
        }
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
  base::WeakPtr<const content::WebContents>
      glic_focused_contents_in_current_window_;
  base::CallbackListSubscription focus_change_subscription_;
  bool context_access_indicator_enabled_ = false;
  base::CallbackListSubscription indicator_change_subscription_;
};

BorderView::BorderView(Browser* browser)
    : updater_(std::make_unique<BorderViewUpdater>(browser, this)),
      shader_(ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_GLIC_BORDER_SHADER)),
      creation_time_(base::TimeTicks::Now()),
      theme_service_(ThemeServiceFactory::GetForProfile(browser->GetProfile())),
      browser_(browser) {
  CHECK(!shader_.empty()) << "Shader not initialized.";
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
  float corner_radius = 0.0f;
#if BUILDFLAG(IS_MAC)
  if (!browser_->window()->IsFullscreen()) {
    corner_radius = 12.0f;
  }
#endif
  std::vector<cc::PaintShader::FloatUniform> float_uniforms = {
      {.name = SkString("u_time"), .value = GetSecondsSinceCreation()},
      {.name = SkString("u_emphasis"), .value = SkScalar(emphasis_)},
      {.name = SkString("u_corner_radius"), .value = SkScalar(corner_radius)}};
  std::vector<cc::PaintShader::Float2Uniform> float2_uniforms = {
      {.name = SkString("u_resolution"),
       .value = SkV2{static_cast<float>(bounds().width()),
                     static_cast<float>(bounds().height())}}};
  std::vector<cc::PaintShader::IntUniform> int_uniforms = {
      {.name = SkString("u_dark"),
       .value = UseDarkMode(theme_service_) ? 1 : 0}};

  views::View::OnPaint(canvas);
  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeSkSLCommand(
      shader_, std::move(float_uniforms), std::move(float2_uniforms),
      /*float4_uniforms=*/{}, std::move(int_uniforms)));
  canvas->DrawRect(gfx::RectF(bounds()), flags);
}

void BorderView::OnAnimationStep(base::TimeTicks timestamp) {
  if (tester_) [[unlikely]] {
    timestamp = tester_->GetTestTimestamp();
  }
  last_animation_step_time_ = timestamp;
  if (first_frame_time_.is_null()) {
    first_frame_time_ = timestamp;
  }
  if (first_emphasis_frame_.is_null()) {
    first_emphasis_frame_ = timestamp;
  }
  if (record_first_ramp_down_frame_) {
    first_ramp_down_frame_ = timestamp;
    record_first_ramp_down_frame_ = false;
  }

  base::TimeDelta emphasis_since_first_frame =
      timestamp - first_emphasis_frame_;
  emphasis_ = GetEmphasis(emphasis_since_first_frame);
  base::TimeDelta opacity_since_first_frame = timestamp - first_frame_time_;
  opacity_ = GetOpacity(timestamp);

  layer()->SetOpacity(opacity_);

  // Don't animate if:
  // - `skip_animation_` is explicitly toggled, or
  // - The animations have exhausted and we haven't started ramping down.
  // We shouldn't be an observer for more than 60 seconds
  // (CompositorAnimationObserver::NotifyFailure()).
  bool emphasis_done =
      emphasis_ == 0.f && !emphasis_since_first_frame.is_zero();
  bool opacity_ramp_up_done =
      opacity_ == 1.f && !opacity_since_first_frame.is_zero();
  bool show_steady_state =
      skip_animation_ || (emphasis_done && opacity_ramp_up_done &&
                          first_ramp_down_frame_.is_null());

  if (show_steady_state) {
    // If skipping the animation the class does not need to be an animation
    // observer.
    compositor_->RemoveAnimationObserver(this);
    return;
  }

  bool opacity_ramp_down_done =
      opacity_ == 0.f && !first_ramp_down_frame_.is_null();
  if (opacity_ramp_down_done) {
    CancelAnimation();
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

  if (tester_) [[unlikely]] {
    tester_->AnimationStarted();
  }
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
  first_ramp_down_frame_ = base::TimeTicks{};
  opacity_ = 0.f;
  emphasis_ = 0.f;

  // `DestroyLayer()` schedules another paint to repaint the affected area by
  // the destroyed layer.
  DestroyLayer();
  SetVisible(false);
}

float BorderView::GetSecondsSinceCreationForTesting() const {
  return GetSecondsSinceCreation();
}

float BorderView::GetEmphasis(base::TimeDelta delta) const {
  if (skip_animation_) {
    return 0.f;
  }
  static constexpr base::TimeDelta kRampUpAndSteady =
      kEmphasisRampUpDuration + kEmphasisDuration;
  if (delta < kRampUpAndSteady) {
    auto target = static_cast<float>(delta / kEmphasisRampUpDuration);
    return ClampAndInterpolate(gfx::Tween::Type::EASE_OUT, target, 0, 1);
  }
  auto target = static_cast<float>((delta - kRampUpAndSteady) /
                                   kEmphasisRampDownDuration);
  return ClampAndInterpolate(gfx::Tween::Type::EASE_IN_OUT_2, target, 1, 0);
}

void BorderView::ResetEmphasisAndReplay() {
  CHECK(compositor_);
  CHECK(compositor_->HasObserver(this));
  if (!compositor_->HasAnimationObserver(this)) {
    compositor_->AddAnimationObserver(this);
  }
  first_emphasis_frame_ = base::TimeTicks{};
  SchedulePaint();
  if (tester_) [[unlikely]] {
    tester_->EmphasisRestarted();
  }
}

float BorderView::GetOpacity(base::TimeTicks timestamp) const {
  if (skip_animation_) {
    return 1.0f;
  }

  if (!first_ramp_down_frame_.is_null()) {
    // The ramp up opacity could be any value between 0-1 during the ramp up
    // time. Thus, the ramping down opacity must be deducted from the value of
    // ramp up opacity at the time of `first_ramp_down_frame_`.
    base::TimeDelta delta = first_ramp_down_frame_ - first_frame_time_;
    float ramp_up_opacity =
        std::clamp(static_cast<float>(delta.InMillisecondsF() /
                                      kOpacityRampUpDuration.InMillisecondsF()),
                   0.0f, 1.0f);

    base::TimeDelta time_since_first_ramp_down_frame =
        timestamp - first_ramp_down_frame_;
    float ramp_down_opacity =
        static_cast<float>(time_since_first_ramp_down_frame.InMillisecondsF() /
                           kOpacityRampDownDuration.InMillisecondsF());

    return std::clamp(ramp_up_opacity - ramp_down_opacity, 0.0f, 1.0f);
  } else {
    base::TimeDelta time_since_first_frame = timestamp - first_frame_time_;
    return std::clamp(
        static_cast<float>(time_since_first_frame.InMillisecondsF() /
                           kOpacityRampUpDuration.InMillisecondsF()),
        0.0f, 1.0f);
  }
}

void BorderView::StartRampingDown() {
  CHECK(compositor_);

  // From now on the opacity will be decreased until it reaches 0.
  record_first_ramp_down_frame_ = true;

  if (!compositor_->HasAnimationObserver(this)) {
    compositor_->AddAnimationObserver(this);
  }
}

float BorderView::GetSecondsSinceCreation() const {
  if (last_animation_step_time_.is_null()) {
    return 0;
  }
  auto time_since_creation =
      (last_animation_step_time_ - GetCreationTime()) % kMaxTime;
  return time_since_creation.InSecondsF();
}

base::TimeTicks BorderView::GetCreationTime() const {
  if (tester_ && !tester_->GetTestCreationTime().is_null()) [[unlikely]] {
    return tester_->GetTestCreationTime();
  }
  return creation_time_;
}

BEGIN_METADATA(BorderView)
END_METADATA

}  // namespace glic
