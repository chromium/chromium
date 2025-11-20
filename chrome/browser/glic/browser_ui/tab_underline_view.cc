// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/tab_underline_view.h"

#include "base/debug/crash_logging.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view_controller.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/view_class_properties.h"

namespace glic {
namespace {

// The total duration of the underline's animation cycle.
constexpr static base::TimeDelta kCycleDuration = base::Milliseconds(3000);

// The width to use for the underline when tabs reach a small size.
constexpr static int kSmallUnderlineWidth = gfx::kFaviconSize;

// The width to use for the underline at the smallest tab sizes when tab
// contents begin to be clipped.
constexpr static int kMinUnderlineWidth = kSmallUnderlineWidth - 4;

// The threshold for tab width at which `kMinUnderlineWidth` should be used.
constexpr static int kMinimumTabWidthThreshold = 42;

// The height of the underline effect.
constexpr static int kEffectHeight = 2;

// The radius to use for rounded corners of the underline effect.
constexpr static float kCornerRadius = kEffectHeight / 2.0f;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabUnderlineView,
                                      kGlicTabUnderlineElementId);

TabUnderlineView::Factory* TabUnderlineView::Factory::factory_ = nullptr;

std::unique_ptr<TabUnderlineView> TabUnderlineView::Factory::Create(
    std::unique_ptr<TabUnderlineViewController> controller,
    Browser* browser,
    Tab* tab) {
  if (factory_) [[unlikely]] {
    return factory_->CreateUnderlineView(std::move(controller), browser, tab);
  }
  return base::WrapUnique(new TabUnderlineView(std::move(controller), browser,
                                               tab, /*tester=*/nullptr));
}

TabUnderlineView::TabUnderlineView(
    std::unique_ptr<TabUnderlineViewController> controller,
    Browser* browser,
    Tab* tab,
    std::unique_ptr<Tester> tester)
    : AnimatedEffectView(browser, std::move(tester)),
      controller_(std::move(controller)),
      tab_(tab) {
  SetProperty(views::kElementIdentifierKey, kGlicTabUnderlineElementId);

  // Post-initialization updates. Don't do the update in the controller's ctor
  // because at that time TabUnderlineView isn't fully initialized, which
  // can lead to undefined behavior.
  controller_->Initialize(this, browser);
}

TabUnderlineView::~TabUnderlineView() = default;

base::WeakPtr<tabs::TabInterface> TabUnderlineView::GetTabInterface() {
  return tab_ ? tab_->data().tab_interface : nullptr;
}

bool TabUnderlineView::IsCycleDone(base::TimeTicks timestamp) {
  return progress_ == 1.f;
}

base::TimeDelta TabUnderlineView::GetTotalDuration() const {
  return kCycleDuration;
}

void TabUnderlineView::PopulateShaderUniforms(
    std::vector<cc::PaintShader::FloatUniform>& float_uniforms,
    std::vector<cc::PaintShader::Float2Uniform>& float2_uniforms,
    std::vector<cc::PaintShader::Float4Uniform>& float4_uniforms,
    std::vector<cc::PaintShader::IntUniform>& int_uniforms) const {
  const auto u_resolution = GetLocalBounds();
  // Insets aren't relevant to the tab underline effect, but are defined in the
  // uniforms of the ContextSharingBorderView shader.
  gfx::Insets uniform_insets = gfx::Insets();

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

  float4_uniforms.push_back({.name = SkString("u_corner_radius"),
                             .value = SkV4{kCornerRadius, kCornerRadius,
                                           kCornerRadius, kCornerRadius}});
}

int TabUnderlineView::ComputeWidth() {
  // At the smallest tab sizes, favicons can be clipped and so a shorter
  // underline is required.
  if (size().width() < kMinimumTabWidthThreshold) {
    return kMinUnderlineWidth;
  }

  // Underline should use either the width of the tab's contents bounds or the
  // width of the favicon, whichever is greater.
  int underline_width = size().width() - tab_->GetInsets().width();
  if (underline_width < gfx::kFaviconSize) {
    return kSmallUnderlineWidth;
  }

  return underline_width;
}

void TabUnderlineView::DrawEffect(gfx::Canvas* canvas,
                                  const cc::PaintFlags& flags) {
  int underline_width = ComputeWidth();
  int underline_x = (size().width() - underline_width + 1) / 2;

  // Draw the underline in the bottom `kEffectHeight` area of the given bounds
  // below the tab contents.
  gfx::Point origin(underline_x, size().height() - kEffectHeight);
  gfx::Size size(underline_width, kEffectHeight);
  gfx::Rect effect_bounds(origin, size);

  cc::PaintFlags new_flags(flags);
  const int kNumDefaultColors = 3;
  // At small sizes, paint the underline as a solid color instead of a gradient.
  // We also draw a solid color if we've got no shader and fewer than 3 colors.
  if (underline_width < gfx::kFaviconSize * 2 ||
      (!new_flags.getShader() && colors_.size() < kNumDefaultColors)) {
    new_flags.setShader(nullptr);
    // `colors_` is not populated if the kGlicParameterizedShader feature is not
    // enabled.
    if (!colors_.empty()) {
      new_flags.setColor(colors_[0]);  // -gem-sys-color--brand-blue #3186FF
    } else {
      // Use -gem-sys-color--brand-blue as fallback color.
      const SkColor fallback_color = SkColorSetARGB(255, 49, 134, 255);
      new_flags.setColor(fallback_color);
    }
  } else if (!new_flags.getShader()) {
    SetDefaultColors(new_flags, gfx::RectF(effect_bounds));
  }

  canvas->DrawRoundRect(gfx::RectF(effect_bounds), kCornerRadius, new_flags);
}

BEGIN_METADATA(TabUnderlineView)
END_METADATA

}  // namespace glic
