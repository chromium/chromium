// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_border_view.h"

#include <math.h>

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/glic/browser_ui/theme_util.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/gpu_data_manager.h"
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
// The amount of time for the opacity to go from 1 to 0 in a fast ramp up.
constexpr static base::TimeDelta kFastOpacityRampUpDuration =
    base::Milliseconds(200);
// The amount of time for the opacity to go from 1 to 0.
constexpr static base::TimeDelta kOpacityRampDownDuration =
    base::Milliseconds(200);
// The amount of time for the border emphasis to go from 0 the max.
constexpr static base::TimeDelta kEmphasisRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the border emphasis to go from max to 0.
constexpr static base::TimeDelta kEmphasisRampDownDuration =
    base::Milliseconds(1000);
// The amount of time for the border to stay emphasized.
constexpr static base::TimeDelta kEmphasisDuration = base::Milliseconds(1500);
// Time since creation will roll over after this time to prevent growing
// indefinitely.
constexpr static base::TimeDelta kMaxTime = base::Hours(1);

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

int64_t TimeTicksToMicroseconds(base::TimeTicks tick) {
  return (tick - base::TimeTicks()).InMicroseconds();
}

gfx::Insets GetContentsBorderInsets(BrowserView& browser_view) {
  gfx::Insets insets_for_contents_border;
  auto* contents_border = browser_view.contents_border_widget();
  if (contents_border && contents_border->IsVisible()) {
    auto* contents_border_view = contents_border->GetContentsView();
    if (contents_border_view && contents_border_view->GetBorder()) {
      insets_for_contents_border =
          contents_border_view->GetBorder()->GetInsets();
    }
  }
  return insets_for_contents_border;
}
}  // namespace

GlicBorderView::Factory* GlicBorderView::Factory::factory_ = nullptr;

std::unique_ptr<GlicBorderView> GlicBorderView::Factory::Create(
    Browser* browser) {
  if (factory_) [[unlikely]] {
    return factory_->CreateBorderView(browser);
  }
  return base::WrapUnique(new GlicBorderView(browser, /*tester=*/nullptr));
}

class GlicBorderView::BorderViewUpdater {
 public:
  explicit BorderViewUpdater(Browser* browser, GlicBorderView* border_view)
      : border_view_(border_view), browser_(browser) {
    auto* glic_service =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());

    // Subscribe to changes in the focus tab.
    focus_change_subscription_ =
        glic_service->AddFocusedTabChangedCallback(base::BindRepeating(
            &GlicBorderView::BorderViewUpdater::OnFocusedTabChanged,
            base::Unretained(this)));

    // Subscribe to changes in the context access indicator status.
    indicator_change_subscription_ =
        glic_service->AddContextAccessIndicatorStatusChangedCallback(
            base::BindRepeating(
                &GlicBorderView::BorderViewUpdater::OnIndicatorStatusChanged,
                base::Unretained(this)));
  }
  BorderViewUpdater(const BorderViewUpdater&) = delete;
  BorderViewUpdater& operator=(const BorderViewUpdater&) = delete;
  ~BorderViewUpdater() = default;

  // Called when the focused tab changes with the focused tab data object.
  void OnFocusedTabChanged(FocusedTabData focused_tab_data) {
    content::WebContents* contents = focused_tab_data.focus();
    auto* previous_focus = glic_focused_contents_in_current_window_.get();
    if (contents && IsTabInCurrentWindow(contents)) {
      glic_focused_contents_in_current_window_ = contents->GetWeakPtr();
    } else {
      glic_focused_contents_in_current_window_.reset();
    }

    auto* current_focus = glic_focused_contents_in_current_window_.get();
    bool focus_changed = previous_focus != current_focus;

    bool tab_switch = previous_focus &&
                      glic_focused_contents_in_current_window_ && focus_changed;
    bool window_gained_focus =
        !previous_focus && glic_focused_contents_in_current_window_;
    bool window_lost_focus =
        previous_focus && !glic_focused_contents_in_current_window_;

    if (tab_switch) {
      UpdateBorderView(UpdateBorderReason::kFocusedTabChanged_NoFocusChange);
    } else if (window_gained_focus) {
      UpdateBorderView(UpdateBorderReason::kFocusedTabChanged_GainFocus);
    } else if (window_lost_focus) {
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
    AddReasonForDebugging(reason);
    auto reasons_string = UpdateReasonsToString();
    SCOPED_CRASH_KEY_STRING1024("crbug-398319435", "update_reasons",
                                reasons_string);
    SCOPED_CRASH_KEY_BOOL("crbug-398319435", "access_indicator",
                          context_access_indicator_enabled_);
    SCOPED_CRASH_KEY_BOOL("crbug-398319435", "glic_focused_contents",
                          !!glic_focused_contents_in_current_window_);
    SCOPED_CRASH_KEY_BOOL("crbug-398319435", "is_glic_window_showing",
                          IsGlicWindowShowing());

    switch (reason) {
      case UpdateBorderReason::kContextAccessIndicatorOn: {
        // Off to On. Throw away everything we have and start the animation from
        // the beginning.
        border_view_->StopShowing();
        if (ShouldShowBorderAnimation()) {
          border_view_->Show();
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
      // This happens when the user has changed the focus from this chrome
      // window to a different chrome window or a different app.
      case UpdateBorderReason::kFocusedTabChanged_GainFocus: {
        border_view_->StopShowing();
        if (ShouldShowBorderAnimation()) {
          border_view_->Show();
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
    return border_view_->GetGlicService()->window_controller().IsShowing();
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

  std::string UpdateReasonToString(UpdateBorderReason reason) {
    switch (reason) {
      case UpdateBorderReason::kContextAccessIndicatorOn:
        return "IndicatorOn";
      case UpdateBorderReason::kContextAccessIndicatorOff:
        return "IndicatorOff";
      case UpdateBorderReason::kFocusedTabChanged_NoFocusChange:
        return "TabFocusChange";
      case UpdateBorderReason::kFocusedTabChanged_GainFocus:
        return "WindowGainFocus";
      case UpdateBorderReason::kFocusedTabChanged_LostFocus:
        return "WindowLostFocus";
    }
    NOTREACHED();
  }

  void AddReasonForDebugging(UpdateBorderReason reason) {
    border_update_reasons_.push_back(UpdateReasonToString(reason));
    if (border_update_reasons_.size() > kNumReasonsToKeep) {
      border_update_reasons_.pop_front();
    }
  }

  std::string UpdateReasonsToString() const {
    std::ostringstream oss;
    for (const auto& r : border_update_reasons_) {
      oss << r << ",";
    }
    return oss.str();
  }

  // Back pointer to the owner. Guaranteed to outlive `this`.
  const raw_ptr<GlicBorderView> border_view_;

  // Owned by `BrowserView`. Outlives all the children of the `BrowserView`.
  const raw_ptr<BrowserWindowInterface> browser_;

  // Tracked states and their subscriptions.
  base::WeakPtr<content::WebContents> glic_focused_contents_in_current_window_;
  base::CallbackListSubscription focus_change_subscription_;
  bool context_access_indicator_enabled_ = false;
  base::CallbackListSubscription indicator_change_subscription_;

  static constexpr size_t kNumReasonsToKeep = 10u;
  std::list<std::string> border_update_reasons_;
};

GlicBorderView::GlicBorderView(Browser* browser, std::unique_ptr<Tester> tester)
    : updater_(std::make_unique<BorderViewUpdater>(browser, this)),
      creation_time_(base::TimeTicks::Now()),
      tester_(std::move(tester)),
      theme_service_(ThemeServiceFactory::GetForProfile(browser->GetProfile())),
      browser_(browser) {
  auto* gpu_data_manager = content::GpuDataManager::GetInstance();
  has_hardware_acceleration_ =
      gpu_data_manager->IsGpuRasterizationForUIEnabled();

  // Upon GPU crashing, the hardware acceleration status might change. This
  // will observe GPU changes to keep hardware acceleration status updated.
  gpu_data_manager_observer_.Observe(gpu_data_manager);

  shader_ =
      ForceSimplifiedShader()
          ? ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                IDR_GLIC_SIMPLIFIED_BORDER_SHADER)
          : ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                IDR_GLIC_BORDER_SHADER);
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

GlicBorderView::~GlicBorderView() = default;

void GlicBorderView::OnPaint(gfx::Canvas* canvas) {
  if (!compositor_) {
    return;
  }
  // We shouldn't have any border.
  CHECK(GetInsets().IsEmpty());
  auto bounds = GetLocalBounds();
  const auto u_resolution = GetLocalBounds();

  // The BrowserView's contents_border_widget() is in its own Widget tree so we
  // need the special treatment.
  gfx::Insets uniform_insets =
      GetContentsBorderInsets(browser_->GetBrowserView());
  // Check the contents's border widget insets is uniform.
  CHECK_EQ(uniform_insets.left(), uniform_insets.top());
  CHECK_EQ(uniform_insets.left(), uniform_insets.right());
  CHECK_EQ(uniform_insets.left(), uniform_insets.bottom());
  bounds.Inset(uniform_insets);

  float corner_radius = 0.0f;
#if BUILDFLAG(IS_MAC)
  if (!browser_->window()->IsFullscreen()) {
    corner_radius = 12.0f;
  }
#endif
  std::vector<cc::PaintShader::FloatUniform> float_uniforms = {
      {.name = SkString("u_time"), .value = GetEffectTime()},
      {.name = SkString("u_emphasis"), .value = emphasis_},
      {.name = SkString("u_corner_radius"), .value = corner_radius},
      {.name = SkString("u_insets"),
       .value = static_cast<float>(uniform_insets.left())},
      {.name = SkString("u_progress"), .value = progress_}};
  std::vector<cc::PaintShader::Float2Uniform> float2_uniforms = {
      // TODO(https://crbug.com/406026829): Ideally `u_resolution` should be a
      // vec4(x, y, w, h) and does not assume the origin is (0, 0). This way we
      // can eliminate `u_insets` and void the shader-internal origin-padding.
      {.name = SkString("u_resolution"),
       .value = SkV2{static_cast<float>(u_resolution.width()),
                     static_cast<float>(u_resolution.height())}}};
  std::vector<cc::PaintShader::IntUniform> int_uniforms = {
      {.name = SkString("u_dark"),
       .value = UseDarkMode(theme_service_) ? 1 : 0}};

  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  auto shader = cc::PaintShader::MakeSkSLCommand(
      shader_, std::move(float_uniforms), std::move(float2_uniforms),
      /*float4_uniforms=*/{}, std::move(int_uniforms), cached_paint_shader_);
  flags.setShader(shader);

  if (base::FeatureList::IsEnabled(features::kGlicUseShaderCache)) {
    cached_paint_shader_ = shader;
  }

  // TODO(liuwilliam): This will create a hard clip at the boundary. Figure out
  // a better way of the falloff.
  constexpr static int kMaxEffectWidth = 100;
  //
  // Four-patch method. This is superior to setting the clip rect on the
  // SkCanvas.
  //
  // ┌─────┬─────────────────────────────┬─────┐
  // │     │            top              │     │
  // │     ├─────────────────────────────┤     │
  // │     │                             │     │
  // │     │                             │     │
  // │     │                             │     │
  // │     │                             │     │
  // │     │                             │     │
  // │left │                             │right│
  // │     │                             │     │
  // │     │                             │     │
  // │     │                             │     │
  // │     │                             │     │
  // │     ├─────────────────────────────┤     │
  // │     │           bottom            │     │
  // └─────┴─────────────────────────────┴─────┘
  gfx::Rect left(bounds.origin(), gfx::Size(kMaxEffectWidth, bounds.height()));
  gfx::Rect right =
      left + gfx::Vector2d(bounds.size().width() - kMaxEffectWidth, 0);

  gfx::Point top_origin = bounds.origin() + gfx::Vector2d(kMaxEffectWidth, 0);
  gfx::Size top_size(bounds.size().width() - 2 * kMaxEffectWidth,
                     kMaxEffectWidth);
  gfx::Rect top(top_origin, top_size);
  gfx::Rect bottom =
      top + gfx::Vector2d(0, bounds.size().height() - kMaxEffectWidth);

  canvas->DrawRect(gfx::RectF(left), flags);
  canvas->DrawRect(gfx::RectF(right), flags);
  canvas->DrawRect(gfx::RectF(top), flags);
  canvas->DrawRect(gfx::RectF(bottom), flags);
}

void GlicBorderView::OnAnimationStep(base::TimeTicks timestamp) {
  if (tester_) [[unlikely]] {
    timestamp = tester_->GetTestTimestamp();
  }
  last_animation_step_time_ = timestamp;
  if (first_frame_time_.is_null()) {
    first_frame_time_ = timestamp;
  }
  if (first_emphasis_frame_.is_null()) {
    first_emphasis_frame_ = timestamp;

    // The time gaps when the border is in steady state cause discontinuous
    // border states when switching tabs. By keeping track of the total steady
    // time, we can have a continuous effect time. Each steady time interval is
    // added to the total at the very beginning of an upcoming emphasis
    // animation.
    // Note: the opacity ramp up / down is not part of the shader animation.
    if (!last_emphasis_frame_.is_null()) {
      total_steady_time_ += timestamp - last_emphasis_frame_;
      last_emphasis_frame_ = base::TimeTicks{};
    }
  }
  if (record_first_ramp_down_frame_) {
    record_first_ramp_down_frame_ = false;
    first_ramp_down_frame_ = timestamp;
  }

  base::TimeDelta emphasis_since_first_frame =
      timestamp - first_emphasis_frame_;
  emphasis_ = GetEmphasis(emphasis_since_first_frame);
  base::TimeDelta opacity_since_first_frame = timestamp - first_frame_time_;
  opacity_ = GetOpacity(timestamp);
  progress_ = GetEffectProgress(timestamp);

  // TODO(liuwilliam): Ideally this should be done in paint-related methods.
  // Consider moving it to LayerDelegate::OnPaintLayer().
  CHECK(layer());
  layer()->SetOpacity(opacity_);

  // Don't animate if the animations have exhausted and we haven't started
  // ramping down. We shouldn't be an observer for more than 60 seconds
  // (CompositorAnimationObserver::NotifyFailure()).
  bool emphasis_done =
      emphasis_ == 0.f && !emphasis_since_first_frame.is_zero();
  bool opacity_ramp_up_done =
      opacity_ == 1.f && !opacity_since_first_frame.is_zero();
  bool show_steady_state =
      emphasis_done && opacity_ramp_up_done && first_ramp_down_frame_.is_null();

  if (show_steady_state) {
    // If skipping the animation the class does not need to be an animation
    // observer.
    compositor_->RemoveAnimationObserver(this);
    if (last_emphasis_frame_.is_null()) {
      last_emphasis_frame_ = timestamp;
    }
    return;
  }

  bool opacity_ramp_down_done =
      opacity_ == 0.f && !first_ramp_down_frame_.is_null();
  if (opacity_ramp_down_done) {
    StopShowing();
    return;
  }

  SchedulePaint();
}

void GlicBorderView::OnCompositingShuttingDown(ui::Compositor* compositor) {
  StopShowing();
}

void GlicBorderView::OnGpuInfoUpdate() {
  auto* gpu_data_manager = content::GpuDataManager::GetInstance();
  bool has_hardware_acceleration =
      gpu_data_manager->IsGpuRasterizationForUIEnabled();

  if (has_hardware_acceleration_ != has_hardware_acceleration) {
    has_hardware_acceleration_ = has_hardware_acceleration;
    shader_ =
        ForceSimplifiedShader()
            ? ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_SIMPLIFIED_BORDER_SHADER)
            : ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_BORDER_SHADER);

    if (IsShowing()) {
      SchedulePaint();
    }
  }
}

bool GlicBorderView::IsShowing() const {
  // `compositor_` is set when the border starts to show and unset when the
  // border stops to show.
  return !!compositor_;
}

float GlicBorderView::GetEffectTimeForTesting() const {
  return GetEffectTime();
}

void GlicBorderView::Show() {
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

  skip_emphasis_animation_ =
      gfx::Animation::PrefersReducedMotion() || ForceSimplifiedShader();

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

void GlicBorderView::StopShowing() {
  if (!compositor_) {
    return;
  }

  compositor_->RemoveObserver(this);
  compositor_->RemoveAnimationObserver(this);
  compositor_ = nullptr;
  first_frame_time_ = base::TimeTicks{};
  first_emphasis_frame_ = base::TimeTicks{};
  last_emphasis_frame_ = base::TimeTicks{};
  first_ramp_down_frame_ = base::TimeTicks{};
  record_first_ramp_down_frame_ = false;
  total_steady_time_ = base::Milliseconds(0);
  opacity_ = 0.f;
  emphasis_ = 0.f;

  // `DestroyLayer()` schedules another paint to repaint the affected area by
  // the destroyed layer.
  DestroyLayer();
  SetVisible(false);
}

float GlicBorderView::GetEmphasis(base::TimeDelta delta) const {
  if (skip_emphasis_animation_) {
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

void GlicBorderView::ResetEmphasisAndReplay() {
  // TOOD(crbug.com/398319435): Remove once we know why this is called before
  // `Show()`.
  if (!compositor_) {
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "opacity", opacity_);
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "emphasis", emphasis_);
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "creation",
                            TimeTicksToMicroseconds(creation_time_));
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "first_frame",
                            TimeTicksToMicroseconds(first_frame_time_));
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "first_emphasis",
                            TimeTicksToMicroseconds(first_emphasis_frame_));
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "last_step",
                            TimeTicksToMicroseconds(last_animation_step_time_));
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "first_rampdown",
                            TimeTicksToMicroseconds(first_ramp_down_frame_));
    base::debug::DumpWithoutCrashing();

    // Gracefully handling the crash case in b/398319435 by
    // closing(minimizing) the glic window.
    // TODO(b/413442838): Add tests to reproduce the dump without crash and
    // validate the solution.
    GetGlicService()->window_controller().Close();
    return;
  }
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

float GlicBorderView::GetOpacity(base::TimeTicks timestamp) {
  auto ramp_up_duration = skip_emphasis_animation_ ? kFastOpacityRampUpDuration
                                                   : kOpacityRampUpDuration;
  if (!first_ramp_down_frame_.is_null()) {
    // The ramp up opacity could be any value between 0-1 during the ramp up
    // time. Thus, the ramping down opacity must be deducted from the value of
    // ramp up opacity at the time of `first_ramp_down_frame_`.
    base::TimeDelta delta = first_ramp_down_frame_ - first_frame_time_;
    float ramp_up_opacity =
        std::clamp(static_cast<float>(delta.InMillisecondsF() /
                                      ramp_up_duration.InMillisecondsF()),
                   0.0f, 1.0f);

    base::TimeDelta time_since_first_ramp_down_frame =
        timestamp - first_ramp_down_frame_;
    float ramp_down_opacity =
        static_cast<float>(time_since_first_ramp_down_frame.InMillisecondsF() /
                           kOpacityRampDownDuration.InMillisecondsF());
    ramp_down_opacity_ =
        std::clamp(ramp_up_opacity - ramp_down_opacity, 0.0f, 1.0f);
    return ramp_down_opacity_;
  } else {
    base::TimeDelta time_since_first_frame = timestamp - first_frame_time_;
    return std::clamp(
        static_cast<float>(ramp_down_opacity_ +
                           (time_since_first_frame.InMillisecondsF() /
                            ramp_up_duration.InMillisecondsF())),
        0.0f, 1.0f);
  }
}

void GlicBorderView::StartRampingDown() {
  CHECK(compositor_);

  // From now on the opacity will be decreased until it reaches 0.
  record_first_ramp_down_frame_ = true;

  if (!compositor_->HasAnimationObserver(this)) {
    compositor_->AddAnimationObserver(this);
  }

  if (tester_) [[unlikely]] {
    tester_->RampDownStarted();
  }
}

float GlicBorderView::GetEffectTime() const {
  if (last_animation_step_time_.is_null()) {
    return 0;
  }

  // Returns a constant duration so the border states don't jump around when
  // switching tabs.
  if (skip_emphasis_animation_) {
    auto time_since_creation =
        (first_frame_time_ - GetCreationTime()) % kMaxTime;
    return time_since_creation.InSecondsF();
  }

  auto time_since_creation =
      ((last_animation_step_time_ - GetCreationTime()) - total_steady_time_) %
      kMaxTime;
  return time_since_creation.InSecondsF();
}

float GlicBorderView::GetEffectProgress(base::TimeTicks timestamp) const {
  if (skip_emphasis_animation_) {
    return 0.0;
  }
  base::TimeDelta time_since_first_frame = timestamp - first_emphasis_frame_;
  base::TimeDelta total_duration =
      kEmphasisRampUpDuration + kEmphasisRampDownDuration + kEmphasisDuration;
  return std::clamp(
      static_cast<float>(time_since_first_frame.InMillisecondsF() /
                         total_duration.InMillisecondsF()),
      0.0f, 1.0f);
}

base::TimeTicks GlicBorderView::GetCreationTime() const {
  if (tester_ && !tester_->GetTestCreationTime().is_null()) [[unlikely]] {
    return tester_->GetTestCreationTime();
  }
  return creation_time_;
}

bool GlicBorderView::ForceSimplifiedShader() const {
  return base::FeatureList::IsEnabled(features::kGlicForceSimplifiedBorder) ||
         !has_hardware_acceleration_;
}

GlicKeyedService* GlicBorderView::GetGlicService() const {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());
  CHECK(service);
  return service;
}

BEGIN_METADATA(GlicBorderView)
END_METADATA

}  // namespace glic
