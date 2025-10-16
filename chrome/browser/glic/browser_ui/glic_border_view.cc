// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_border_view.h"

#include <math.h>

#include "base/debug/crash_logging.h"
#include "chrome/browser/actor/ui/actor_border_view_controller.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"

namespace glic {
namespace {

// The amount of time for the border emphasis to go from 0 the max.
constexpr static base::TimeDelta kEmphasisRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the border emphasis to go from max to 0.
constexpr static base::TimeDelta kEmphasisRampDownDuration =
    base::Milliseconds(1000);
// The amount of time for the border to stay emphasized.
constexpr static base::TimeDelta kEmphasisDuration = base::Milliseconds(1500);

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

gfx::Insets GetContentsBorderInsets(BrowserView& browser_view,
                                    content::WebContents* web_contents) {
  gfx::Insets insets_for_contents_border;
  auto* const contents_border =
      browser_view.GetContentsContainerViewFor(web_contents)
          ->capture_contents_border_widget();
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
    Browser* browser,
    ContentsWebView* contents_web_view) {
  if (factory_) [[unlikely]] {
    return factory_->CreateBorderView(browser, contents_web_view);
  }
  return base::WrapUnique(new GlicBorderView(browser, contents_web_view,
                                             /*tester=*/nullptr));
}

class GlicBorderView::BorderViewUpdater : public views::ViewObserver {
 public:
  explicit BorderViewUpdater(GlicBorderView* border_view,
                             ContentsWebView* contents_web_view)
      : border_view_(border_view), contents_web_view_(contents_web_view) {
    auto* glic_service = border_view->GetGlicService();

    // Subscribe to glow updates from the actor border controller.
    if (features::kGlicActorUiBorderGlow.Get()) {
      actor_border_view_controller_subscription_ =
          ActorBorderViewController::From(border_view_->browser_)
              ->AddOnActorBorderGlowUpdatedCallback(base::BindRepeating(
                  &GlicBorderView::BorderViewUpdater::OnActorBorderGlowUpdated,
                  base::Unretained(this)));
    }

    // Observe the contents web view for when it is deleting.
    contents_web_view_observation_.Observe(contents_web_view_);

    // Subscribe to changes in the focus tab.
    focus_change_subscription_ =
        glic_service->sharing_manager().AddFocusedTabChangedCallback(
            base::BindRepeating(
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
  ~BorderViewUpdater() override = default;

  ContentsWebView* contents_web_view() { return contents_web_view_; }

  // Called when the focused tab changes with the focused tab data object.
  void OnFocusedTabChanged(const FocusedTabData& focused_tab_data) {
    tabs::TabInterface* tab = focused_tab_data.focus();
    auto* previous_focus = glic_focused_contents_in_current_view_.get();
    if (tab && IsTabInCurrentView(tab->GetContents())) {
      glic_focused_contents_in_current_view_ = tab->GetContents()->GetWeakPtr();
    } else {
      glic_focused_contents_in_current_view_.reset();
    }

    auto* current_focus = glic_focused_contents_in_current_view_.get();
    bool focus_changed = previous_focus != current_focus;

    bool tab_switch = previous_focus &&
                      glic_focused_contents_in_current_view_ && focus_changed;
    bool window_gained_focus =
        !previous_focus && glic_focused_contents_in_current_view_;
    bool window_lost_focus =
        previous_focus && !glic_focused_contents_in_current_view_;

    if (tab_switch) {
      MaybeRunBorderViewUpdate(
          UpdateBorderReason::kFocusedTabChanged_NoFocusChange);
    } else if (window_gained_focus) {
      MaybeRunBorderViewUpdate(
          UpdateBorderReason::kFocusedTabChanged_GainFocus);
    } else if (window_lost_focus) {
      MaybeRunBorderViewUpdate(
          UpdateBorderReason::kFocusedTabChanged_LostFocus);
    }
  }

  // Called when the actor component changes the border glow status.
  void OnActorBorderGlowUpdated(tabs::TabInterface* tab, bool enabled) {
    if (!IsTabInCurrentView(tab->GetContents())) {
      return;
    }

    if (actor_border_glow_enabled_ == enabled) {
      return;
    }
    actor_border_glow_enabled_ = enabled;

    if (actor_border_glow_enabled_) {
      // Force the border to show, regardless of other states. This gives the
      // actor priority over other signals.
      border_view_->StopShowing();
      // If the standalone border glow param is enabled, don't actually just
      // suppress the glic_border_view from showing, as it is controlled by a
      // different component.
      if (!features::kGlicActorUiStandaloneBorderGlow.Get()) {
        border_view_->Show();
      }
    } else {
      // Revert to the last known state based on other signals like tab focus
      // or context access.
      if (last_mutating_update_reason_.has_value()) {
        UpdateBorderView(*last_mutating_update_reason_);
      } else {
        // No known state from before. We just ramp down.
        if (border_view_->IsShowing()) {
          border_view_->StartRampingDown();
        }
      }
    }
  }

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled) {
    if (context_access_indicator_enabled_ == enabled) {
      return;
    }
    context_access_indicator_enabled_ = enabled;
    MaybeRunBorderViewUpdate(
        context_access_indicator_enabled_
            ? UpdateBorderReason::kContextAccessIndicatorOn
            : UpdateBorderReason::kContextAccessIndicatorOff);
  }

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override {
    contents_web_view_observation_.Reset();
    indicator_change_subscription_ = {};
    focus_change_subscription_ = {};
    actor_border_view_controller_subscription_ = {};
    contents_web_view_ = nullptr;
  }

 private:
  // Updates the BorderView UI effect given the current state of the focused tab
  // and context access indicator flag.
  enum class UpdateBorderReason {
    kContextAccessIndicatorOn = 0,
    kContextAccessIndicatorOff,

    // Tab focus changes in the same contents view.
    kFocusedTabChanged_NoFocusChange,

    // Focus changes across different contents view.
    kFocusedTabChanged_GainFocus,
    kFocusedTabChanged_LostFocus,
  };

  // This function is a gateway for all non actor border updates. It respects
  // the actor_border_glow_enabled_ flag, which can suppress or override regular
  // updates. It also keeps track of the last reason for an update.
  void MaybeRunBorderViewUpdate(UpdateBorderReason reason) {
    // We only want to override the latest reason if it's one that would result
    // in showing vs hiding the border. `kFocusedTabChanged_NoFocusChange` only
    // replays an animation, it does not change the state.
    if (reason != UpdateBorderReason::kFocusedTabChanged_NoFocusChange) {
      last_mutating_update_reason_ = reason;
    }

    if (!actor_border_glow_enabled_) {
      UpdateBorderView(reason);
    }
  }

  void UpdateBorderView(UpdateBorderReason reason) {
    AddReasonForDebugging(reason);
    auto reasons_string = UpdateReasonsToString();
    SCOPED_CRASH_KEY_STRING1024("crbug-398319435", "update_reasons",
                                reasons_string);
    SCOPED_CRASH_KEY_BOOL("crbug-398319435", "access_indicator",
                          context_access_indicator_enabled_);
    SCOPED_CRASH_KEY_BOOL("crbug-398319435", "glic_focused_contents",
                          !!glic_focused_contents_in_current_view_);
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
          border_view_->ResetAnimationCycle();
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
    return border_view_->GetGlicService()->IsWindowShowing();
  }

  bool IsTabInCurrentView(const content::WebContents* tab) const {
    return contents_web_view_->web_contents() == tab;
  }

  bool ShouldShowBorderAnimation() {
    if (!glic_focused_contents_in_current_view_) {
      return false;
    }

    // For multi-instance we rely on the sharing manager signal for everything
    // else.
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return true;
    }

    // Remaining single instance checks.
    if (!context_access_indicator_enabled_) {
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
  raw_ptr<GlicBorderView> border_view_;

  // Pointer to the associated contents web view and associated view
  // observation for view deletion.
  raw_ptr<ContentsWebView> contents_web_view_;
  base::ScopedObservation<views::View, views::ViewObserver>
      contents_web_view_observation_{this};

  // Tracked states and their subscriptions.
  base::WeakPtr<content::WebContents> glic_focused_contents_in_current_view_;
  base::CallbackListSubscription focus_change_subscription_;
  bool context_access_indicator_enabled_ = false;
  base::CallbackListSubscription indicator_change_subscription_;

  // When true, the actor framework has requested the border to glow. This
  // overrides other signals.
  bool actor_border_glow_enabled_ = false;

  // Subscription to the actor border controller for glow updates.
  base::CallbackListSubscription actor_border_view_controller_subscription_;

  static constexpr size_t kNumReasonsToKeep = 10u;
  std::list<std::string> border_update_reasons_;

  // Stores the last mutating reason for a border update, so the state can be
  // restored when the actor glow is disabled.
  std::optional<UpdateBorderReason> last_mutating_update_reason_;
};

GlicBorderView::GlicBorderView(Browser* browser,
                               ContentsWebView* contents_web_view,
                               std::unique_ptr<Tester> tester)
    : GlicAnimatedEffectView(browser, std::move(tester)),
      updater_(std::make_unique<BorderViewUpdater>(this, contents_web_view)) {
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

void GlicBorderView::PopulateShaderUniforms(
    std::vector<cc::PaintShader::FloatUniform>& float_uniforms,
    std::vector<cc::PaintShader::Float2Uniform>& float2_uniforms,
    std::vector<cc::PaintShader::Float4Uniform>& float4_uniforms,
    std::vector<cc::PaintShader::IntUniform>& int_uniforms) const {
  CHECK(GetInsets().IsEmpty());
  const auto u_resolution = GetLocalBounds();

  // The BrowserView's contents_border_widget() is in its own Widget tree so we
  // need the special treatment.
  gfx::Insets uniform_insets =
      GetContentsBorderInsets(browser_->GetBrowserView(),
                              updater_->contents_web_view()->web_contents());
  // Check the contents's border widget insets is uniform.
  CHECK_EQ(uniform_insets.left(), uniform_insets.top());
  CHECK_EQ(uniform_insets.left(), uniform_insets.right());
  CHECK_EQ(uniform_insets.left(), uniform_insets.bottom());

  gfx::RoundedCornersF corner_radius = GetContentBorderRadius();

  float_uniforms.push_back(
      {.name = SkString("u_time"), .value = GetEffectTime()});
  float_uniforms.push_back(
      {.name = SkString("u_emphasis"), .value = emphasis_});
  float_uniforms.push_back(
      {.name = SkString("u_insets"),
       .value = static_cast<float>(uniform_insets.left())});
  float_uniforms.push_back(
      {.name = SkString("u_progress"), .value = progress_});

  float2_uniforms.push_back(
      // TODO(https://crbug.com/406026829): Ideally `u_resolution` should be a
      // vec4(x, y, w, h) and does not assume the origin is (0, 0). This way we
      // can eliminate `u_insets` and void the shader-internal origin-padding.
      {.name = SkString("u_resolution"),
       .value = SkV2{static_cast<float>(u_resolution.width()),
                     static_cast<float>(u_resolution.height())}});
  int_uniforms.push_back(
      {.name = SkString("u_dark"),
       .value = theme_service_->BrowserUsesDarkColors() ? 1 : 0});

  float4_uniforms.push_back(
      {.name = SkString("u_corner_radius"),
       .value = SkV4{corner_radius.upper_left(), corner_radius.upper_right(),
                     corner_radius.lower_right(), corner_radius.lower_left()}});
}

void GlicBorderView::DrawEffect(gfx::Canvas* canvas,
                                const cc::PaintFlags& flags) {
  auto bounds = GetLocalBounds();
  gfx::Insets uniform_insets =
      GetContentsBorderInsets(browser_->GetBrowserView(),
                              updater_->contents_web_view()->web_contents());
  bounds.Inset(uniform_insets);

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

bool GlicBorderView::IsCycleDone(base::TimeTicks timestamp) {
  base::TimeDelta emphasis_since_first_frame = timestamp - first_cycle_frame_;
  emphasis_ = GetEmphasis(emphasis_since_first_frame);
  return emphasis_ == 0.f && !emphasis_since_first_frame.is_zero();
}

void GlicBorderView::SetRoundedCorners(const gfx::RoundedCornersF& radii) {
  if (corner_radius_ == radii) {
    return;
  }

  corner_radius_ = radii;

  if (IsShowing()) {
    layer()->SetRoundedCornerRadius(radii);
    layer()->SetIsFastRoundedCorner(true);
    SchedulePaint();
  }
}

float GlicBorderView::GetEmphasis(base::TimeDelta delta) const {
  if (skip_animation_cycle_) {
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

base::TimeDelta GlicBorderView::GetTotalDuration() const {
  base::TimeDelta total_duration =
      kEmphasisRampUpDuration + kEmphasisRampDownDuration + kEmphasisDuration;
  return total_duration;
}

gfx::RoundedCornersF GlicBorderView::GetContentBorderRadius() const {
  if (!corner_radius_.IsEmpty()) {
    return corner_radius_;
  }

#if BUILDFLAG(IS_MAC)
  if (!browser_->GetBrowserView().IsFullscreen()) {
    return gfx::RoundedCornersF(0.0f, 0.0f, 12.0f, 12.0f);
  }
#endif

  return gfx::RoundedCornersF();
}

BEGIN_METADATA(GlicBorderView)
END_METADATA

}  // namespace glic
