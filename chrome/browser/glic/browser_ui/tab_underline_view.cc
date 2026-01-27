// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/tab_underline_view.h"

#include <vector>

#include "base/debug/crash_logging.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view_controller.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
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

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabUnderlineView,
                                      kGlicTabUnderlineElementId);

TabUnderlineView::Factory* TabUnderlineView::Factory::factory_ = nullptr;

std::unique_ptr<TabUnderlineView> TabUnderlineView::Factory::Create(
    std::unique_ptr<TabUnderlineViewController> controller,
    BrowserWindowInterface* browser_window_interface,
    tabs::TabHandle tab_handle) {
  if (factory_) [[unlikely]] {
    return factory_->CreateUnderlineView(std::move(controller),
                                         browser_window_interface, tab_handle);
  }
  return base::WrapUnique(new TabUnderlineView(std::move(controller),
                                               browser_window_interface,
                                               tab_handle, /*tester=*/nullptr));
}

TabUnderlineView::TabUnderlineView(
    std::unique_ptr<TabUnderlineViewController> controller,
    BrowserWindowInterface* browser_window_interface,
    tabs::TabHandle tab_handle,
    std::unique_ptr<Tester> tester)
    : AnimatedEffectView(browser_window_interface->GetProfile(),
                         std::move(tester)),
      controller_(std::move(controller)),
      tab_handle_(tab_handle) {
  SetProperty(views::kElementIdentifierKey, kGlicTabUnderlineElementId);

  // Needed so that expectations of visibility that
  // inform underline updates are correct on first show.
  SetVisible(false);

  // `glic_tab_underline_view_` should never receive input events.
  SetCanProcessEventsWithinSubtree(false);

  // Register for active tab changes.
  active_tab_subscription_ =
      browser_window_interface->RegisterActiveTabDidChange(base::BindRepeating(
          &TabUnderlineView::OnActiveTabChanged, base::Unretained(this)));

  // Post-initialization updates. Don't do the update in the controller's ctor
  // because at that time TabUnderlineView isn't fully initialized, which
  // can lead to undefined behavior.
  controller_->Initialize(this, browser_window_interface);
}

TabUnderlineView::~TabUnderlineView() = default;

tabs::TabInterface* TabUnderlineView::GetTabInterface() {
  return tab_handle_.Get();
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

void TabUnderlineView::OnThemeChanged() {
  View::OnThemeChanged();
  colors_ = GetEffectColors();
  SchedulePaint();
}

void TabUnderlineView::AddedToWidget() {
  View::AddedToWidget();
  controller_->OnViewAddedToWidget();
}

std::vector<SkColor> TabUnderlineView::GetEffectColors() {
  // Overwrite colors used for shader effect to follow Chrome theming instead of
  // kGlicParameterizedShader feature values.
  const ui::ColorProvider* color_provider = GetColorProvider();
  std::vector<SkColor> colors;

  // Different sets of colors are used for underlines on active vs inactive tabs
  // if a custom theme is being used.
  if (color_provider && GetTabInterface()) {
    bool is_tab_active = GetTabInterface()->IsActivated();
    colors = {
        color_provider->GetColor(is_tab_active
                                     ? kColorGlicActiveTabUnderlineGradient1
                                     : kColorGlicInactiveTabUnderlineGradient1),
        color_provider->GetColor(is_tab_active
                                     ? kColorGlicActiveTabUnderlineGradient2
                                     : kColorGlicInactiveTabUnderlineGradient2),
        color_provider->GetColor(
            is_tab_active ? kColorGlicActiveTabUnderlineGradient3
                          : kColorGlicInactiveTabUnderlineGradient3)};
  } else {
    // If there is no ColorProvider available, fall back to
    // -gem-sys-color--brand-blue.
    colors = std::vector<SkColor>(3, SkColorSetARGB(255, 49, 134, 255));
  }
  return colors;
}

int TabUnderlineView::ComputeDimension() {
  int bounds_dim = (orientation_ == Orientation::kHorizontal) ? size().width()
                                                              : size().height();
  // At the smallest tab sizes, favicons can be clipped and so a shorter
  // underline is required.
  if (bounds_dim < kMinimumTabWidthThreshold) {
    return kMinUnderlineWidth;
  }

  int insets_dim = (orientation_ == Orientation::kHorizontal)
                       ? parent()->GetInsets().width()
                       : parent()->GetInsets().height();

  // Underline should use either the width of the tab's contents bounds or the
  // width of the favicon, whichever is greater.
  int dimension = bounds_dim - insets_dim;
  if (dimension < gfx::kFaviconSize) {
    return kSmallUnderlineWidth;
  }

  return dimension;
}

void TabUnderlineView::SetOrientation(Orientation orientation) {
  orientation_ = orientation;
}

void TabUnderlineView::DrawEffect(gfx::Canvas* canvas,
                                  const cc::PaintFlags& flags) {
  int dimension = ComputeDimension();

  gfx::Rect effect_bounds;

  if (orientation_ == Orientation::kHorizontal) {
    int underline_x = (size().width() - dimension + 1) / 2;
    gfx::Point origin(underline_x, size().height() - kEffectThickness);
    gfx::Size size(dimension, kEffectThickness);
    effect_bounds = gfx::Rect(origin, size);
  } else {
    // Vertical orientation: Draw on the left.
    int underline_y = (size().height() - dimension + 1) / 2;
    gfx::Point origin(kEffectThickness, underline_y);
    gfx::Size size(kEffectThickness, dimension);
    effect_bounds = gfx::Rect(origin, size);
  }

  cc::PaintFlags new_flags(flags);
  const int kNumDefaultColors = 3;
  // At small sizes, paint the underline as a solid color instead of a gradient.
  // We also draw a solid color if we've got no shader and fewer than 3 colors.
  if (dimension < gfx::kFaviconSize * 2 ||
      (!new_flags.getShader() && colors_.size() < kNumDefaultColors)) {
    new_flags.setShader(nullptr);
    CHECK(!colors_.empty());
    new_flags.setColor(colors_[0]);
  } else if (!new_flags.getShader()) {
    SetDefaultColors(new_flags, gfx::RectF(effect_bounds));
  }

  canvas->DrawRoundRect(gfx::RectF(effect_bounds), kCornerRadius, new_flags);
}

void TabUnderlineView::OnActiveTabChanged(
    BrowserWindowInterface* browser_window_interface) {
  colors_ = GetEffectColors();
  SchedulePaint();
}

BEGIN_METADATA(TabUnderlineView)
END_METADATA

}  // namespace glic
