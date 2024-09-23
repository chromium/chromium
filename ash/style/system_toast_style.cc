// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_toast_style.h"

#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/scoped_a11y_override_window_setter.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "ash/system/toast/toast_overlay.h"
#include "ash/wm/work_area_insets.h"
#include "base/strings/strcat.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash::deprecated {

namespace {

// UI constants in DIP (Density Independent Pixel).
constexpr int kToastTextMaximumWidth = 512;

constexpr int kOneLineHorizontalPadding = 16;
constexpr int kTwoLineHorizontalPadding = 24;

constexpr int kOneLineVerticalPadding = 8;
constexpr int kTwoLineVerticalPadding = 12;

constexpr int kOneLineButtonPadding = 2;
constexpr int kTwoLineButtonRightPadding = 12;

constexpr int kLeadingIconSize = 20;
constexpr int kLeadingIconLeftPadding = 18;
constexpr int kLeadingIconRightPadding = 14;

// Inset for the focus ring around the dismiss button.
constexpr int kDismissButtonFocusRingHaloInset = 1;

// The label inside SystemToastStyle, which allows two lines at maximum.
class SystemToastInnerLabel : public views::Label {
  METADATA_HEADER(SystemToastInnerLabel, views::Label)

 public:
  explicit SystemToastInnerLabel(const std::u16string& text)
      : views::Label(text) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetAutoColorReadabilityEnabled(false);
    SetMultiLine(true);
    SetMaximumWidth(kToastTextMaximumWidth);
    SetMaxLines(2);
    SetSubpixelRenderingEnabled(false);
    SetEnabledColorId(cros_tokens::kTextColorPrimary);

    SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kLegacyBody1));
  }

  SystemToastInnerLabel(const SystemToastInnerLabel&) = delete;
  SystemToastInnerLabel& operator=(const SystemToastInnerLabel&) = delete;
  ~SystemToastInnerLabel() override = default;
};

BEGIN_METADATA(SystemToastInnerLabel)
END_METADATA

// Returns the vertical padding for the layout given the presence of the dismiss
// button and whether the toast is multi-line.
int ComputeVerticalPadding(bool has_button, bool two_line) {
  if (two_line) {
    return kTwoLineVerticalPadding;
  }

  // For one line, the button is taller so it determines the height of the toast
  // so we use the button's padding.
  return has_button ? kOneLineButtonPadding : kOneLineVerticalPadding;
}

// Computes the outer insets for the Box Layout container that holds toast
// elements. Horizontal spacing may vary depending if there's a dismiss button
// or a leading icon present.
gfx::Insets ComputeInsets(bool has_button, bool two_line, bool has_icon) {
  int left_inset;
  int right_inset;

  if (has_icon) {
    left_inset = kLeadingIconLeftPadding;
  } else {
    left_inset =
        two_line ? kTwoLineHorizontalPadding : kOneLineHorizontalPadding;
  }

  if (has_button) {
    right_inset = two_line ? kTwoLineButtonRightPadding : kOneLineButtonPadding;
  } else {
    right_inset =
        two_line ? kTwoLineHorizontalPadding : kOneLineHorizontalPadding;
  }

  const int vertical_insets = ComputeVerticalPadding(has_button, two_line);

  return gfx::Insets::TLBR(vertical_insets, left_inset, vertical_insets,
                           right_inset);
}

}  // namespace

SystemToastStyle::SystemToastStyle(base::RepeatingClosure dismiss_callback,
                                   const std::u16string& text,
                                   const std::u16string& dismiss_text,
                                   const gfx::VectorIcon& leading_icon)
    : leading_icon_(&leading_icon),
      scoped_a11y_overrider_(
          std::make_unique<ScopedA11yOverrideWindowSetter>()) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));

  if (!leading_icon_->is_empty()) {
    leading_icon_view_ = AddChildView(std::make_unique<views::ImageView>());
    leading_icon_view_->SetPreferredSize(
        gfx::Size(kLeadingIconSize, kLeadingIconSize));
    leading_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *leading_icon_, cros_tokens::kCrosSysOnSurface));

    auto* icon_padding = AddChildView(std::make_unique<views::View>());
    icon_padding->SetPreferredSize(
        gfx::Size(kLeadingIconRightPadding, kLeadingIconSize));
  }

  label_ = AddChildView(std::make_unique<SystemToastInnerLabel>(text));

  if (!dismiss_text.empty()) {
    dismiss_button_ = AddChildView(std::make_unique<PillButton>(
        std::move(dismiss_callback), dismiss_text,
        PillButton::Type::kAccentFloatingWithoutIcon,
        /*icon=*/nullptr));
    dismiss_button_->SetFocusBehavior(
        views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }

  // Requesting size forces layout. Otherwise, we don't know how many lines
  // are needed.
  label_->GetPreferredSize(views::SizeBounds(label_->width(), {}));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->SetFlexForView(label_, 1);
  UpdateInsideBorderInsets();

  const int toast_height = GetPreferredSize().height();
  const float toast_corner_radius = toast_height / 2.0f;
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(toast_corner_radius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      toast_corner_radius,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderOnShadow
          : views::HighlightBorder::Type::kHighlightBorder1));

  // Since system toast has a very large corner radius, we should use the shadow
  // on texture layer. Refer to `ash::SystemShadowOnTextureLayer` for more
  // details.
  shadow_ = SystemShadow::CreateShadowOnTextureLayer(
      SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(toast_corner_radius);
}

SystemToastStyle::~SystemToastStyle() = default;

bool SystemToastStyle::ToggleA11yFocus() {
  if (!dismiss_button_) {
    return false;
  }

  auto* focus_ring = views::FocusRing::Get(dismiss_button_);
  focus_ring->SetHaloInset(kDismissButtonFocusRingHaloInset);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetHasFocusPredicate(base::BindRepeating(
      [](const SystemToastStyle* style, const views::View* view) {
        return style->is_dismiss_button_highlighted_;
      },
      base::Unretained(this)));

  is_dismiss_button_highlighted_ = !is_dismiss_button_highlighted_;
  if (is_dismiss_button_highlighted_) {
    scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
        dismiss_button_->GetWidget()->GetNativeWindow());
    dismiss_button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection,
                                              true);
  }

  focus_ring->SetVisible(is_dismiss_button_highlighted_);
  focus_ring->SchedulePaint();
  return is_dismiss_button_highlighted_;
}

void SystemToastStyle::SetText(const std::u16string& text) {
  label_->SetText(text);
}

void SystemToastStyle::AddedToWidget() {
  // Attach the shadow at the bottom of the widget layer.
  auto* shadow_layer = shadow_->GetLayer();
  auto* widget_layer = GetWidget()->GetLayer();

  widget_layer->Add(shadow_layer);
  widget_layer->StackAtBottom(shadow_layer);

  // Update shadow content bounds with the bounds of widget layer.
  shadow_->SetContentBounds(gfx::Rect(widget_layer->bounds().size()));

  // Make shadow observe the theme change of the widget.
  shadow_->ObserveColorProviderSource(GetWidget());
}

void SystemToastStyle::UpdateInsideBorderInsets() {
  static_cast<views::BoxLayout*>(GetLayoutManager())
      ->set_inside_border_insets(ComputeInsets(!!dismiss_button_,
                                               label_->GetRequiredLines() > 1,
                                               !leading_icon_->is_empty()));
  InvalidateLayout();
}

BEGIN_METADATA(SystemToastStyle)
END_METADATA

}  // namespace ash::deprecated
