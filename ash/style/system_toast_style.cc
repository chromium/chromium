// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_toast_style.h"

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/scoped_a11y_override_window_setter.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "ash/system/toast/toast_overlay.h"
#include "ash/wm/work_area_insets.h"
#include "base/strings/strcat.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// UI constants in DIP (Density Independent Pixel).
constexpr int kToastTextMaximumWidth = 512;
constexpr int kOneLineHorizontalSpacing = 16;
constexpr int kTwoLineHorizontalSpacing = 24;
constexpr int kSpacingBetweenLabelAndButton = 16;
constexpr int kOnelineButtonPadding = 2;
constexpr int kTwolineButtonRightSpacing = 12;
constexpr int kToastLabelVerticalSpacing = 8;
constexpr int kManagedIconSize = 32;
constexpr int kTwolineVerticalPadding = 12;

// The label inside SystemToastStyle, which allows two lines at maximum.
class SystemToastInnerLabel : public views::Label {
 public:
  METADATA_HEADER(SystemToastInnerLabel);
  explicit SystemToastInnerLabel(const std::u16string& text)
      : views::Label(text) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetAutoColorReadabilityEnabled(false);
    SetMultiLine(true);
    SetMaximumWidth(kToastTextMaximumWidth);
    SetMaxLines(2);
    SetSubpixelRenderingEnabled(false);
    SetEnabledColorId(cros_tokens::kTextColorPrimary);

    SetFontList(views::Label::GetDefaultFontList().Derive(
        2, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  }

  SystemToastInnerLabel(const SystemToastInnerLabel&) = delete;
  SystemToastInnerLabel& operator=(const SystemToastInnerLabel&) = delete;
  ~SystemToastInnerLabel() override = default;
};

BEGIN_METADATA(SystemToastInnerLabel, views::Label)
END_METADATA

// Returns the vertical padding for the layout given button presence and
// `two_line`.
int ComputeVerticalSpacing(bool has_button, bool two_line) {
  if (two_line)
    return kTwolineVerticalPadding;

  // For one line, the button is taller so it determines the height of the toast
  // so we use the button's padding.
  if (has_button)
    return kOnelineButtonPadding;

  return kToastLabelVerticalSpacing;
}

}  // namespace

SystemToastStyle::SystemToastStyle(base::RepeatingClosure dismiss_callback,
                                   const std::u16string& text,
                                   const std::u16string& dismiss_text,
                                   const bool is_managed)
    : scoped_a11y_overrider_(
          std::make_unique<ScopedA11yOverrideWindowSetter>()) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));

  if (is_managed) {
    managed_icon_ = AddChildView(std::make_unique<views::ImageView>());
    managed_icon_->SetPreferredSize(
        gfx::Size(kManagedIconSize, kManagedIconSize));
  }

  label_ = AddChildView(std::make_unique<SystemToastInnerLabel>(text));

  if (!dismiss_text.empty()) {
    button_ = AddChildView(std::make_unique<PillButton>(
        std::move(dismiss_callback), dismiss_text,
        PillButton::Type::kAccentFloatingWithoutIcon,
        /*icon=*/nullptr));
    button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }

  // Requesting size forces layout. Otherwise, we don't know how many lines
  // are needed.
  label_->GetPreferredSize();
  const bool two_line = label_->GetRequiredLines() > 1;
  const int vertical_spacing = ComputeVerticalSpacing(!!button_, two_line);

  auto insets =
      gfx::Insets::VH(vertical_spacing, two_line ? kTwoLineHorizontalSpacing
                                                 : kOneLineHorizontalSpacing);
  if (button_) {
    insets.set_right(two_line ? kTwolineButtonRightSpacing
                              : kOnelineButtonPadding);
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, insets,
      button_ ? kSpacingBetweenLabelAndButton : 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->SetFlexForView(label_, 1);

  int toast_height = GetPreferredSize().height();
  const float toast_corner_radius = toast_height / 2.0f;
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(toast_corner_radius));
  if (features::IsDarkLightModeEnabled()) {
    SetBorder(std::make_unique<views::HighlightBorder>(
        toast_corner_radius, views::HighlightBorder::Type::kHighlightBorder1,
        /*use_light_colors=*/false));
  }

  // Since system toast has a very large corner radius, we should use the shadow
  // on texture layer. Refer to `ash::SystemShadowOnTextureLayer` for more
  // details.
  shadow_ = SystemShadow::CreateShadowOnTextureLayer(
      SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(toast_corner_radius);
}

SystemToastStyle::~SystemToastStyle() = default;

bool SystemToastStyle::ToggleA11yFocus() {
  if (!button_ ||
      !Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    return false;
  }

  auto* focus_ring = views::FocusRing::Get(button_);
  focus_ring->SetHasFocusPredicate([&](views::View* view) -> bool {
    return is_dismiss_button_highlighted_;
  });

  is_dismiss_button_highlighted_ = !is_dismiss_button_highlighted_;
  scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
      is_dismiss_button_highlighted_ ? button_->GetWidget()->GetNativeWindow()
                                     : nullptr);

  if (is_dismiss_button_highlighted_)
    button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);

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
}

void SystemToastStyle::OnThemeChanged() {
  views::View::OnThemeChanged();

  if (managed_icon_) {
    managed_icon_->SetImage(gfx::CreateVectorIcon(
        kSystemMenuBusinessIcon,
        GetColorProvider()->GetColor(cros_tokens::kIconColorPrimary)));
  }

  SchedulePaint();
}

BEGIN_METADATA(SystemToastStyle, views::View)
END_METADATA

}  // namespace ash
