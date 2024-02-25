// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_curtain_view.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "ash/system/power/power_button_menu_view_util.h"
#include "base/check_deref.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

using views::Builder;
using views::FlexLayoutView;
using views::ImageView;
using views::LayoutOrientation;

constexpr gfx::Size kEnterpriseIconSize(20, 20);

// Bottom margin for the enterprise icon.
constexpr int kIconBottomMargin = 30;

// Preferred width of the dialog.
constexpr int kWidth = 400;

// Inner padding for the widget.
constexpr int kHorizontalPadding = 25;
constexpr int kVerticalPadding = 20;

// Bottom margin for the widget title.
constexpr int kTitleBottomMargin = 35;

// Line height for the widget content text.
constexpr int kContentLineHeight = 12;

views::FlexSpecification FullFlex() {
  return views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                  views::MaximumFlexSizeRule::kUnbounded)
      .WithWeight(1);
}

gfx::ImageSkia EnterpriseIcon(const ui::ColorProvider& color_provider) {
  return gfx::CreateVectorIcon(
      chromeos::kEnterpriseIcon,
      color_provider.GetColor(kColorAshIconColorPrimary));
}

std::u16string TitleText() {
  return l10n_util::GetStringUTF16(IDS_ASH_CURTAIN_POWER_WIDGET_TITLE);
}

std::u16string MessageText() {
  return l10n_util::GetStringUTF16(IDS_ASH_CURTAIN_POWER_WIDGET_DESCRIPTION);
}

}  // namespace

PowerButtonMenuCurtainView::PowerButtonMenuCurtainView() {
  SetPaintToLayer();
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPowerButtonMenuCornerRadius, kPowerButtonMenuBorderType));
  SetBackground(
      views::CreateThemedSolidBackground(kPowerButtonMenuBackgroundColorId));

  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kPowerButtonMenuCornerRadius));
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  Initialize();

  // Create a system shadow for current view.
  shadow_ = ash::SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, ash::SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(kPowerButtonMenuCornerRadius);
}

PowerButtonMenuCurtainView::~PowerButtonMenuCurtainView() = default;

void PowerButtonMenuCurtainView::ScheduleShowHideAnimation(bool show) {
  // Set initial state.
  SetVisible(true);

  // Calculate transform of menu view and shadow bounds.
  gfx::Transform transform;
  if (show) {
    transform.Translate(0, kPowerButtonMenuTransformDistanceDp);
  }

  SetLayerAnimation(layer(), this, show, transform);
  SetLayerAnimation(shadow_->GetLayer(), nullptr, show, transform);
}

void PowerButtonMenuCurtainView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const ui::ColorProvider& color_provider = CHECK_DEREF(GetColorProvider());
  enterprise_icon().SetImage(EnterpriseIcon(color_provider));
  title_text().SetEnabledColor(
      color_provider.GetColor(kColorAshIconColorPrimary));
  description_text().SetEnabledColor(
      color_provider.GetColor(kColorAshIconColorPrimary));
}

void PowerButtonMenuCurtainView::Initialize() {
  Builder<FlexLayoutView>(this)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .AddChildren(
          Builder<FlexLayoutView>()
              .SetOrientation(LayoutOrientation::kVertical)
              .SetProperty(views::kFlexBehaviorKey, FullFlex())
              .AddChildren(
                  // Enterprise icon
                  Builder<ImageView>()
                      .SetImageSize(kEnterpriseIconSize)
                      .SetSize(kEnterpriseIconSize)
                      .SetHorizontalAlignment(ImageView::Alignment::kLeading)
                      .SetProperty(views::kFlexBehaviorKey, FullFlex())
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(
                                       kVerticalPadding, kHorizontalPadding,
                                       kIconBottomMargin, kHorizontalPadding))
                      .CopyAddressTo(&enterprise_icon_),
                  // Title
                  Builder<views::Label>()
                      .SetText(TitleText())
                      .SetTextStyle(views::style::STYLE_EMPHASIZED)
                      .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                      .SetHorizontalAlignment(
                          gfx::HorizontalAlignment::ALIGN_LEFT)
                      .SetMultiLine(true)
                      .SetProperty(views::kFlexBehaviorKey, FullFlex())
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(0, kHorizontalPadding,
                                                     kTitleBottomMargin,
                                                     kHorizontalPadding))
                      .SetMaximumWidth(kWidth)
                      .CopyAddressTo(&title_text_),
                  // Description
                  Builder<views::Label>()
                      .SetText(MessageText())
                      .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                      .SetHorizontalAlignment(
                          gfx::HorizontalAlignment::ALIGN_LEFT)
                      .SetMultiLine(true)
                      .SetProperty(views::kFlexBehaviorKey, FullFlex())
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(0, kHorizontalPadding,
                                                     kVerticalPadding,
                                                     kHorizontalPadding))
                      .SetLineHeight(kContentLineHeight)
                      .SetMaximumWidth(kWidth)
                      .CopyAddressTo(&description_text_)))
      .BuildChildren();
}

void PowerButtonMenuCurtainView::OnImplicitAnimationsCompleted() {
  if (layer()->opacity() == 0.f) {
    SetVisible(false);
  }
}

BEGIN_METADATA(PowerButtonMenuCurtainView)
END_METADATA

}  // namespace ash
