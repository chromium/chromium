// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/tab_slider_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Color Ids shared by all types of tab slider buttons.
constexpr ui::ColorId kSelectedColorId =
    cros_tokens::kCrosSysSystemOnPrimaryContainer;
constexpr ui::ColorId kDisabledSelectedColorId =
    cros_tokens::kCrosSysSystemOnPrimaryContainerDisabled;
constexpr ui::ColorId kUnselectedColorId = cros_tokens::kCrosSysOnSurface;
constexpr ui::ColorId kDisabledUnselectedColorId =
    cros_tokens::kCrosSysDisabled;
// The padding between the button and the focus ring.
constexpr int kFocusRingPadding = 2;

// Icon slider buttons' layout parameters.
constexpr int kIconButtonSize = 32;
constexpr int kIconSize = 20;

// Label slider buttons' layout parameters.
constexpr int kLabelButtonHeight = 32;
constexpr int kLabelButtonMinWidth = 80;
constexpr gfx::Insets kLabelButtonBorderInsets = gfx::Insets::VH(6, 16);

}  // namespace

//------------------------------------------------------------------------------
// TabSliderButton:

TabSliderButton::TabSliderButton(PressedCallback callback,
                                 const std::u16string& tooltip_text)
    : views::Button(std::move(callback)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Configure the focus ring.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  const float halo_inset =
      focus_ring->GetHaloThickness() / 2.f + kFocusRingPadding;
  focus_ring->SetHaloInset(-halo_inset);
  // Set a pill shaped (fully rounded rect) highlight path to focus ring.
  focus_ring->SetPathGenerator(
      std::make_unique<views::PillHighlightPathGenerator>());
  // Set the highlight path to `kHighlightPathGeneratorKey` property for the ink
  // drop to use.
  views::InstallPillHighlightPathGenerator(this);

  SetTooltipText(tooltip_text);
}

TabSliderButton::~TabSliderButton() = default;

void TabSliderButton::AddedToSlider(TabSlider* tab_slider) {
  DCHECK(tab_slider);
  tab_slider_ = tab_slider;
}

void TabSliderButton::SetSelected(bool selected) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;
  if (selected_ && tab_slider_) {
    tab_slider_->OnButtonSelected(this);
  }

  OnSelectedChanged();
}

SkColor TabSliderButton::GetColorIdOnButtonState() {
  const bool enabled = GetEnabled();
  return selected()
             ? (enabled ? kSelectedColorId : kDisabledSelectedColorId)
             : (enabled ? kUnselectedColorId : kDisabledUnselectedColorId);
}

void TabSliderButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
  const std::u16string tooltip = GetTooltipText(gfx::Point());
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetName(tooltip);
  node_data->SetCheckedState(selected_ ? ax::mojom::CheckedState::kTrue
                                       : ax::mojom::CheckedState::kFalse);
}

void TabSliderButton::NotifyClick(const ui::Event& event) {
  // Select the button on clicking.
  SetSelected(true);
  views::Button::NotifyClick(event);
}

BEGIN_METADATA(TabSliderButton, views::Button)
END_METADATA

//------------------------------------------------------------------------------
// IconSliderButton:

IconSliderButton::IconSliderButton(PressedCallback callback,
                                   const gfx::VectorIcon* icon,
                                   const std::u16string& tooltip_text)
    : TabSliderButton(std::move(callback), tooltip_text), icon_(icon) {
  SetPreferredSize(gfx::Size(kIconButtonSize, kIconButtonSize));

  // Replace the pill shaped highlight path of focus ring with a circle shaped
  // highlight path.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(-gfx::Insets(
          focus_ring->GetHaloThickness() / 2 + kFocusRingPadding)));
  // Set the circle highlight path to `kHighlightPathGeneratorKey` property for
  // the ink drop to use.
  views::InstallCircleHighlightPathGenerator(this);
}

IconSliderButton::~IconSliderButton() = default;

void IconSliderButton::OnSelectedChanged() {
  SchedulePaint();
}

void IconSliderButton::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

void IconSliderButton::PaintButtonContents(gfx::Canvas* canvas) {
  DCHECK(GetWidget());

  // Paint the icon in the color according to the current state.
  const gfx::ImageSkia img = gfx::CreateVectorIcon(
      *icon_, kIconSize,
      GetColorProvider()->GetColor(GetColorIdOnButtonState()));
  const int origin_offset = (kIconButtonSize - kIconSize) / 2;
  canvas->DrawImageInt(img, origin_offset, origin_offset);
}

BEGIN_METADATA(IconSliderButton, TabSliderButton)
END_METADATA

//------------------------------------------------------------------------------
// LabelSliderButton:

LabelSliderButton::LabelSliderButton(PressedCallback callback,
                                     const std::u16string& text,
                                     const std::u16string& tooltip_text)
    : TabSliderButton(std::move(callback), tooltip_text),
      label_(AddChildView(std::make_unique<views::Label>(text))) {
  SetBorder(views::CreateEmptyBorder(kLabelButtonBorderInsets));
  SetUseDefaultFillLayout(true);
  // Force the label to use requested colors.
  label_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *label_);

  // If custom tooltip is not indicated, use the label text as the tooltip.
  if (tooltip_text.empty()) {
    SetTooltipText(text);
  }
}

LabelSliderButton::~LabelSliderButton() = default;

void LabelSliderButton::UpdateLabelColor() {
  label_->SetEnabledColorId(GetColorIdOnButtonState());
  SchedulePaint();
}

void LabelSliderButton::OnSelectedChanged() {
  // Update label color on selected state changed.
  UpdateLabelColor();
}

int LabelSliderButton::GetHeightForWidth(int w) const {
  return kLabelButtonHeight;
}

gfx::Size LabelSliderButton::CalculatePreferredSize() const {
  // The width of the container equals to the label width with horizontal
  // padding.
  return gfx::Size(
      std::max(label_->GetPreferredSize().width() + GetInsets().width(),
               kLabelButtonMinWidth),
      kLabelButtonHeight);
}

void LabelSliderButton::StateChanged(ButtonState old_state) {
  // Update the label color when enabled state changed.
  if (old_state != ButtonState::STATE_DISABLED &&
      GetState() != ButtonState::STATE_DISABLED) {
    return;
  }

  UpdateLabelColor();
}

BEGIN_METADATA(LabelSliderButton, TabSliderButton)
END_METADATA

//------------------------------------------------------------------------------
// IconLabelSliderButton:

IconLabelSliderButton::IconLabelSliderButton(PressedCallback callback,
                                             const gfx::VectorIcon* icon,
                                             const std::u16string& text,
                                             const std::u16string& tooltip_text)
    : TabSliderButton(std::move(callback),
                      tooltip_text.empty() ? text : tooltip_text),
      image_view_(AddChildView(std::make_unique<views::ImageView>())),
      label_(AddChildView(std::make_unique<views::Label>(text))) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/gfx::Insets::VH(8, 16),
      /*between_child_spacing=*/8));

  DCHECK(icon);
  image_view_->SetImage(ui::ImageModel::FromImageGenerator(
      base::BindRepeating(
          [](TabSliderButton* tab_slider_button,
             const gfx::VectorIcon* vector_icon, const ui::ColorProvider*) {
            return gfx::CreateVectorIcon(
                *vector_icon, kIconSize,
                tab_slider_button->GetColorProvider()->GetColor(
                    tab_slider_button->GetColorIdOnButtonState()));
          },
          /*tab_slider_button=*/this, icon),
      gfx::Size(kIconSize, kIconSize)));

  // Force the label to use requested colors.
  label_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *label_);
}

IconLabelSliderButton::~IconLabelSliderButton() = default;

void IconLabelSliderButton::UpdateColors() {
  label_->SetEnabledColorId(GetColorIdOnButtonState());
  // `SchedulePaint()` will result in the `gfx::VectorIcon` for `image_view_`
  // getting re-generated with the proper color.
  SchedulePaint();
}

void IconLabelSliderButton::OnSelectedChanged() {
  UpdateColors();
}

BEGIN_METADATA(IconLabelSliderButton, TabSliderButton)
END_METADATA

}  // namespace ash
