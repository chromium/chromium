// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_tab_underline_view.h"

#include <math.h>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/glic/browser_ui/theme_util.h"
#include "chrome/browser/glic/glic_keyed_service.h"
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
#include "content/public/common/color_parser.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/view_class_properties.h"

namespace glic {
namespace {

// The amount of time for the opacity to go from 0 to 1.
constexpr static base::TimeDelta kOpacityRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the opacity to go from 0 to 1 in a fast ramp up.
constexpr static base::TimeDelta kFastOpacityRampUpDuration =
    base::Milliseconds(200);
// The amount of time for the opacity to go from 1 to 0.
constexpr static base::TimeDelta kOpacityRampDownDuration =
    base::Milliseconds(200);
// The amount of time for the underline emphasis to go from 0 the max.
constexpr static base::TimeDelta kEmphasisRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the underline emphasis to go from max to 0.
constexpr static base::TimeDelta kEmphasisRampDownDuration =
    base::Milliseconds(1000);
// The amount of time for the underline to stay emphasized.
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

std::vector<SkColor> GetParameterizedColors() {
  std::vector<SkColor> colors;
  if (base::FeatureList::IsEnabled(features::kGlicParameterizedShader)) {
    std::vector<std::string> unparsed_colors =
        base::SplitString(::features::kGlicParameterizedShaderColors.Get(), "#",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& unparsed : unparsed_colors) {
      SkColor result;
      if (!content::ParseHexColorString("#" + unparsed, &result)) {
        return std::vector<SkColor>();
      }
      colors.push_back(result);
    }
  }
  return colors;
}

std::vector<float> GetParameterizedFloats() {
  std::vector<float> floats;
  if (base::FeatureList::IsEnabled(features::kGlicParameterizedShader)) {
    std::vector<std::string> unparsed_floats =
        base::SplitString(::features::kGlicParameterizedShaderFloats.Get(), "#",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& unparsed : unparsed_floats) {
      double result;
      if (!base::StringToDouble(unparsed, &result)) {
        return std::vector<float>();
      }
      floats.push_back(static_cast<float>(result));
    }
  }
  return floats;
}

}  // namespace

GlicTabUnderlineView::GlicTabUnderlineView(Browser* browser, Tab* tab)
    : creation_time_(base::TimeTicks::Now()),
      colors_(GetParameterizedColors()),
      floats_(GetParameterizedFloats()),
      theme_service_(ThemeServiceFactory::GetForProfile(browser->GetProfile())),
      tab_(tab),
      browser_(browser) {
  auto* gpu_data_manager = content::GpuDataManager::GetInstance();
  has_hardware_acceleration_ =
      gpu_data_manager->IsGpuRasterizationForUIEnabled();

  // Upon GPU crashing, the hardware acceleration status might change. This
  // will observe GPU changes to keep hardware acceleration status updated.
  gpu_data_manager_observer_.Observe(gpu_data_manager);

  UpdateShader();
  CHECK(!shader_.empty()) << "Shader not initialized.";
}

GlicTabUnderlineView::~GlicTabUnderlineView() = default;

void GlicTabUnderlineView::OnPaint(gfx::Canvas* canvas) {
  if (!compositor_) {
    return;
  }
  auto bounds = GetLocalBounds();
  const auto u_resolution = GetLocalBounds();
  // Insets aren't relevant to the tab underline effect, but are defined in the
  // uniforms of the GlicBorderView shader.
  gfx::Insets uniform_insets = gfx::Insets();

  float corner_radius = 0.0f;
#if BUILDFLAG(IS_MAC)
  if (!browser_->window()->IsFullscreen()) {
    corner_radius = 12.0f;
  }
#endif
  // TODO(crbug.com/433136181): shader logic is borrowed from GlicBorderView,
  // but emphasis can be fixed to 0 for the underline and related handling can
  // be removed entirely.
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

  std::vector<cc::PaintShader::Float4Uniform> float4_uniforms;
  if (base::FeatureList::IsEnabled(features::kGlicParameterizedShader)) {
    for (int i = 0; i < static_cast<int>(colors_.size()); ++i) {
      float4_uniforms.push_back(
          {.name = SkString(absl::StrFormat("u_color%d", i + 1)),
           .value =
               SkV4{static_cast<float>(SkColorGetR(colors_[i]) / 255.0),
                    static_cast<float>(SkColorGetG(colors_[i]) / 255.0),
                    static_cast<float>(SkColorGetB(colors_[i]) / 255.0), 1.f}});
    }
    for (int i = 0; i < static_cast<int>(floats_.size()); ++i) {
      float_uniforms.push_back(
          {.name = SkString(absl::StrFormat("u_float%d", i + 1)),
           .value = floats_[i]});
    }
  }

  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  auto shader = cc::PaintShader::MakeSkSLCommand(
      shader_, std::move(float_uniforms), std::move(float2_uniforms),
      std::move(float4_uniforms), std::move(int_uniforms),
      cached_paint_shader_);

  flags.setShader(shader);

  if (base::FeatureList::IsEnabled(features::kGlicUseShaderCache)) {
    cached_paint_shader_ = shader;
  }

  constexpr static int kMaxEffectWidth = 2;
  gfx::Point origin =
      bounds.origin() +
      gfx::Vector2d(0, bounds.size().height() - kMaxEffectWidth);
  gfx::Size size(bounds.size().width(), kMaxEffectWidth);
  gfx::Rect effect_bounds(origin, size);
  canvas->DrawRect(gfx::RectF(effect_bounds), flags);
}

void GlicTabUnderlineView::OnAnimationStep(base::TimeTicks timestamp) {
  last_animation_step_time_ = timestamp;
  if (first_frame_time_.is_null()) {
    first_frame_time_ = timestamp;
  }
  if (first_emphasis_frame_.is_null()) {
    first_emphasis_frame_ = timestamp;

    // The time gaps when the underline is in steady state cause discontinuous
    // underline states when switching tabs. By keeping track of the total
    // steady time, we can have a continuous effect time. Each steady time
    // interval is added to the total at the very beginning of an upcoming
    // emphasis animation. Note: the opacity ramp up / down is not part of the
    // shader animation.
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

void GlicTabUnderlineView::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  StopShowing();
}

void GlicTabUnderlineView::OnGpuInfoUpdate() {
  auto* gpu_data_manager = content::GpuDataManager::GetInstance();
  bool has_hardware_acceleration =
      gpu_data_manager->IsGpuRasterizationForUIEnabled();

  if (has_hardware_acceleration_ != has_hardware_acceleration) {
    has_hardware_acceleration_ = has_hardware_acceleration;
    UpdateShader();

    if (IsShowing()) {
      SchedulePaint();
    }
  }
}

bool GlicTabUnderlineView::IsShowing() const {
  // `compositor_` is set when the underline starts to show and unset when the
  // underline stops to show.
  return !!compositor_;
}

void GlicTabUnderlineView::Show() {
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
  compositor_animation_observation_.Observe(compositor_.get());
  compositor_observation_.Observe(compositor_.get());
}

void GlicTabUnderlineView::StopShowing() {
  if (!compositor_) {
    return;
  }

  compositor_observation_.Reset();
  compositor_animation_observation_.Reset();
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

float GlicTabUnderlineView::GetEmphasis(base::TimeDelta delta) const {
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

void GlicTabUnderlineView::ResetEmphasisAndReplay() {
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

    // Gracefully handling the crash case in crbug.com/398319435 by
    // closing(minimizing) the glic window.
    // TODO(crbug.com/413442838): Add tests to reproduce the dump without crash
    // and validate the solution.
    GetGlicService()->window_controller().Close();
    return;
  }
  CHECK(compositor_->HasObserver(this));
  if (!compositor_->HasAnimationObserver(this)) {
    compositor_->AddAnimationObserver(this);
  }
  first_emphasis_frame_ = base::TimeTicks{};
  SchedulePaint();
}

float GlicTabUnderlineView::GetOpacity(base::TimeTicks timestamp) {
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

void GlicTabUnderlineView::StartRampingDown() {
  CHECK(compositor_);

  // From now on the opacity will be decreased until it reaches 0.
  record_first_ramp_down_frame_ = true;

  if (!compositor_->HasAnimationObserver(this)) {
    compositor_->AddAnimationObserver(this);
  }
}

float GlicTabUnderlineView::GetEffectTime() const {
  if (last_animation_step_time_.is_null()) {
    return 0;
  }

  // Returns a constant duration so the underline states don't jump around when
  // switching tabs.
  if (skip_emphasis_animation_) {
    auto time_since_creation = (first_frame_time_ - creation_time_) % kMaxTime;
    return time_since_creation.InSecondsF();
  }

  auto time_since_creation =
      ((last_animation_step_time_ - creation_time_) - total_steady_time_) %
      kMaxTime;
  return time_since_creation.InSecondsF();
}

float GlicTabUnderlineView::GetEffectProgress(base::TimeTicks timestamp) const {
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

bool GlicTabUnderlineView::ForceSimplifiedShader() const {
  return base::FeatureList::IsEnabled(features::kGlicForceSimplifiedBorder) ||
         !has_hardware_acceleration_;
}

GlicKeyedService* GlicTabUnderlineView::GetGlicService() const {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());
  CHECK(service);
  return service;
}

void GlicTabUnderlineView::UpdateShader() {
  if (base::FeatureList::IsEnabled(features::kGlicParameterizedShader) &&
      !colors_.empty() && !floats_.empty()) {
    shader_ =
        ForceSimplifiedShader()
            ? ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_SIMPLIFIED_PARAMETERIZED_BORDER_SHADER)
            : ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_PARAMETERIZED_BORDER_SHADER);
  } else {
    shader_ =
        ForceSimplifiedShader()
            ? ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_SIMPLIFIED_BORDER_SHADER)
            : ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_BORDER_SHADER);
  }
}

BEGIN_METADATA(GlicTabUnderlineView)
END_METADATA

}  // namespace glic
