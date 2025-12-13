// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/context_sharing_border_view.h"

#include <math.h>

#include "base/debug/crash_logging.h"
#include "chrome/browser/glic/browser_ui/context_sharing_border_view_controller.h"
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

constexpr static float kCornerRadius = 12.0f;

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

ContextSharingBorderView::Factory* ContextSharingBorderView::Factory::factory_ =
    nullptr;

std::unique_ptr<ContextSharingBorderView>
ContextSharingBorderView::Factory::Create(
    std::unique_ptr<ContextSharingBorderViewController> controller,
    Browser* browser,
    ContentsWebView* contents_web_view) {
  if (factory_) [[unlikely]] {
    return factory_->CreateBorderView(std::move(controller), browser,
                                      contents_web_view);
  }
  return base::WrapUnique(new ContextSharingBorderView(
      std::move(controller), browser, contents_web_view,
      /*tester=*/nullptr));
}

ContextSharingBorderView::ContextSharingBorderView(
    std::unique_ptr<ContextSharingBorderViewController> controller,
    Browser* browser,
    ContentsWebView* contents_web_view,
    std::unique_ptr<Tester> tester)
    : AnimatedEffectView(browser, std::move(tester)),
      controller_(std::move(controller)) {
  // Post-initialization updates. Don't do the update in the controller's ctor
  // because at that time BorderView isn't fully initialized, which can lead to
  // undefined behavior.
  controller_->Initialize(this, contents_web_view, browser);
}

ContextSharingBorderView::~ContextSharingBorderView() = default;

void ContextSharingBorderView::PopulateShaderUniforms(
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
                              controller_->contents_web_view()->web_contents());
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

void ContextSharingBorderView::DrawSimplifiedEffect(gfx::Canvas* canvas) const {
  const float kBorderWidth = 5.0f;
  gfx::RectF bounds(GetLocalBounds());
  // Ensure that the border does not spill out of the viewport (and taking the
  // floor ensures that the anti-aliased border properly hugs the edge of the
  // container).
  bounds.Inset(std::floor(kBorderWidth * 0.5f));
  auto content_border_insets = gfx::InsetsF(GetContentsBorderInsets(
      browser_->GetBrowserView(),
      controller_->contents_web_view()->web_contents()));
  bounds.Inset(content_border_insets);

  cc::PaintFlags border_flags;
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setStrokeWidth(kBorderWidth);
  border_flags.setAntiAlias(true);

  gfx::RoundedCornersF radii = GetContentBorderRadius();
  SkRRect rrect;
  SkVector corner_radii[4];
  float radius_adjustment = kBorderWidth * 0.5f;
  int index = 0;
  auto radii_span = base::span<SkVector>(corner_radii);
  for (float radius : {radii.upper_left(), radii.upper_right(),
                       radii.lower_right(), radii.lower_left()}) {
    if (content_border_insets.IsEmpty()) {
      radius = std::max(0.f, radius - radius_adjustment);
    } else {
      // Do not use a border radius if we're further inset.
      radius = 0.f;
    }
    // Assigning to corner_radii results an unsafe buffer access error. Instead,
    // we will assign via the span.
    radii_span[index] = {radius, radius};
    index++;
  }
  rrect.setRectRadii(gfx::RectFToSkRect(bounds), corner_radii);
  SetDefaultColors(border_flags, bounds);
  canvas->sk_canvas()->drawRRect(rrect, border_flags);
}

void ContextSharingBorderView::DrawEffect(gfx::Canvas* canvas,
                                          const cc::PaintFlags& flags) {
  auto bounds = GetLocalBounds();
  gfx::Insets uniform_insets =
      GetContentsBorderInsets(browser_->GetBrowserView(),
                              controller_->contents_web_view()->web_contents());
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

  if (!flags.getShader()) {
    DrawSimplifiedEffect(canvas);
    return;
  }

  canvas->DrawRect(gfx::RectF(left), flags);
  canvas->DrawRect(gfx::RectF(right), flags);
  canvas->DrawRect(gfx::RectF(top), flags);
  canvas->DrawRect(gfx::RectF(bottom), flags);
}

bool ContextSharingBorderView::IsCycleDone(base::TimeTicks timestamp) {
  base::TimeDelta emphasis_since_first_frame = timestamp - first_cycle_frame_;
  emphasis_ = GetEmphasis(emphasis_since_first_frame);
  return emphasis_ == 0.f && !emphasis_since_first_frame.is_zero();
}

void ContextSharingBorderView::SetRoundedCorners(
    const gfx::RoundedCornersF& radii) {
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

float ContextSharingBorderView::GetEmphasis(base::TimeDelta delta) const {
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

base::TimeDelta ContextSharingBorderView::GetTotalDuration() const {
  base::TimeDelta total_duration =
      kEmphasisRampUpDuration + kEmphasisRampDownDuration + kEmphasisDuration;
  return total_duration;
}

gfx::RoundedCornersF ContextSharingBorderView::GetContentBorderRadius() const {
  if (!corner_radius_.IsEmpty()) {
    return corner_radius_;
  }

  // If GlicMultiInstance is enabled, have all corners be rounded.
  // TODO(https://crbug.com/457452232): Update rounded corner radiuses for
  // different OS's.
  if (controller_->IsSidePanelOpen()) {
    return gfx::RoundedCornersF(kCornerRadius, kCornerRadius, kCornerRadius,
                                kCornerRadius);
  }

#if BUILDFLAG(IS_MAC)
  if (!browser_->GetBrowserView().IsFullscreen()) {
    return gfx::RoundedCornersF(0.0f, 0.0f, kCornerRadius, kCornerRadius);
  }
#endif

  return gfx::RoundedCornersF();
}

BEGIN_METADATA(ContextSharingBorderView)
END_METADATA

}  // namespace glic
