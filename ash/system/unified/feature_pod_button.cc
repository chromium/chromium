// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pod_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;
using ControlsLayerType = AshColorProvider::ControlsLayerType;

namespace {

void ConfigureFeaturePodLabel(views::Label* label,
                              int line_height,
                              int font_size) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetCanProcessEventsWithinSubtree(false);
  label->SetLineHeight(line_height);

  gfx::Font default_font;
  gfx::Font label_font =
      default_font.Derive(font_size - default_font.GetFontSize(),
                          gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(label_font);
  label->SetFontList(font_list);
}

}  // namespace

FeaturePodIconButton::FeaturePodIconButton(PressedCallback callback,
                                           bool is_togglable)
    : IconButton(std::move(callback),
                 IconButton::Type::kLarge,
                 /*icon=*/nullptr,
                 is_togglable,
                 /*has_border=*/true) {
  SetFlipCanvasOnPaintForRTLUI(false);
  GetViewAccessibility().SetIsLeaf(true);
}

FeaturePodIconButton::~FeaturePodIconButton() = default;

BEGIN_METADATA(FeaturePodIconButton)
END_METADATA

FeaturePodLabelButton::FeaturePodLabelButton(PressedCallback callback)
    : Button(std::move(callback)),
      label_(new views::Label),
      sub_label_(new views::Label),
      detailed_view_arrow_(new views::ImageView) {
  SetBorder(views::CreateEmptyBorder(kUnifiedFeaturePodHoverPadding));
  GetViewAccessibility().SetIsLeaf(true);

  label_->SetLineHeight(kUnifiedFeaturePodLabelLineHeight);
  label_->SetMultiLine(true);
  label_->SetMaxLines(kUnifiedFeaturePodLabelMaxLines);
  ConfigureFeaturePodLabel(label_, kUnifiedFeaturePodLabelLineHeight,
                           kUnifiedFeaturePodLabelFontSize);
  ConfigureFeaturePodLabel(sub_label_, kUnifiedFeaturePodSubLabelLineHeight,
                           kUnifiedFeaturePodSubLabelFontSize);
  sub_label_->SetVisible(false);

  detailed_view_arrow_->SetCanProcessEventsWithinSubtree(false);
  detailed_view_arrow_->SetVisible(false);

  AddChildView(label_.get());
  AddChildView(detailed_view_arrow_.get());
  AddChildView(sub_label_.get());

  StyleUtil::SetUpInkDropForButton(this);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kUnifiedFeaturePodHoverCornerRadius);

  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
}

FeaturePodLabelButton::~FeaturePodLabelButton() = default;

void FeaturePodLabelButton::Layout(PassKey) {
  DCHECK(views::FocusRing::Get(this));
  views::FocusRing::Get(this)->DeprecatedLayoutImmediately();
  LayoutInCenter(label_, GetContentsBounds().y());
  LayoutInCenter(sub_label_, GetContentsBounds().CenterPoint().y() +
                                 kUnifiedFeaturePodInterLabelPadding);

  if (!detailed_view_arrow_->GetVisible())
    return;

  // We need custom layout because |label_| is first laid out in the center
  // without considering |detailed_view_arrow_|, then |detailed_view_arrow_| is
  // placed on the right side of |label_|.
  gfx::Size arrow_size = detailed_view_arrow_->GetPreferredSize();
  detailed_view_arrow_->SetBoundsRect(gfx::Rect(
      gfx::Point(label_->bounds().right() + kUnifiedFeaturePodArrowSpacing,
                 label_->bounds().CenterPoint().y() - arrow_size.height() / 2),
      arrow_size));
}

gfx::Size FeaturePodLabelButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Minimum width of the button
  int width = kUnifiedFeaturePodLabelWidth + GetInsets().width();
  if (detailed_view_arrow_->GetVisible()) {
    const int label_width = std::min(
        kUnifiedFeaturePodLabelWidth,
        label_->GetPreferredSize(views::SizeBounds(label_->width(), {}))
            .width());
    // Symmetrically increase the width to accommodate the arrow
    const int extra_space_for_arrow =
        2 * (kUnifiedFeaturePodArrowSpacing +
             detailed_view_arrow_->GetPreferredSize().width());
    width = std::max(width,
                     label_width + extra_space_for_arrow + GetInsets().width());
  }

  // Make sure there is sufficient margin around the label.
  int horizontal_margin =
      width -
      label_->GetPreferredSize(views::SizeBounds(label_->width(), {})).width();
  if (horizontal_margin < 2 * kUnifiedFeaturePodMinimumHorizontalMargin)
    width += 2 * kUnifiedFeaturePodMinimumHorizontalMargin - horizontal_margin;

  int height = label_->GetPreferredSize(views::SizeBounds(label_->width(), {}))
                   .height() +
               GetInsets().height();
  if (sub_label_->GetVisible()) {
    height +=
        kUnifiedFeaturePodInterLabelPadding +
        sub_label_->GetPreferredSize(views::SizeBounds(sub_label_->width(), {}))
            .height();
  }

  return gfx::Size(width, height);
}

void FeaturePodLabelButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  OnEnabledChanged();
}

void FeaturePodLabelButton::SetLabel(const std::u16string& label) {
  label_->SetText(label);
  InvalidateLayout();
}

const std::u16string& FeaturePodLabelButton::GetLabelText() const {
  return label_->GetText();
}

void FeaturePodLabelButton::SetSubLabel(const std::u16string& sub_label) {
  sub_label_->SetText(sub_label);
  sub_label_->SetVisible(!sub_label.empty());
  label_->SetMultiLine(sub_label.empty());
  InvalidateLayout();
}

const std::u16string& FeaturePodLabelButton::GetSubLabelText() const {
  return sub_label_->GetText();
}

void FeaturePodLabelButton::ShowDetailedViewArrow() {
  detailed_view_arrow_->SetVisible(true);
  InvalidateLayout();
}

void FeaturePodLabelButton::OnEnabledChanged() {
  views::Button::OnEnabledChanged();
  const AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor primary_text_color =
      color_provider->GetContentLayerColor(ContentLayerType::kTextColorPrimary);
  const SkColor secondary_text_color = color_provider->GetContentLayerColor(
      ContentLayerType::kTextColorSecondary);
  label_->SetEnabledColor(
      GetEnabled() ? primary_text_color
                   : ColorUtil::GetDisabledColor(primary_text_color));
  sub_label_->SetEnabledColor(
      GetEnabled() ? secondary_text_color
                   : ColorUtil::GetDisabledColor(secondary_text_color));

  const SkColor icon_color =
      color_provider->GetContentLayerColor(ContentLayerType::kIconColorPrimary);
  detailed_view_arrow_->SetImage(gfx::CreateVectorIcon(
      kUnifiedMenuMoreIcon,
      GetEnabled() ? icon_color : ColorUtil::GetDisabledColor(icon_color)));
}

void FeaturePodLabelButton::LayoutInCenter(views::View* child, int y) {
  gfx::Rect contents_bounds = GetContentsBounds();
  gfx::Size preferred_size =
      child->GetPreferredSize(views::SizeBounds(child->width(), {}));
  int child_width =
      std::min(kUnifiedFeaturePodLabelWidth, preferred_size.width());
  child->SetBounds(
      contents_bounds.x() + (contents_bounds.width() - child_width) / 2, y,
      child_width, preferred_size.height());
}

BEGIN_METADATA(FeaturePodLabelButton)
END_METADATA

FeaturePodButton::FeaturePodButton(FeaturePodControllerBase* controller,
                                   bool is_togglable)
    : icon_button_(new FeaturePodIconButton(
          base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                              base::Unretained(controller)),
          is_togglable)),
      label_button_(new FeaturePodLabelButton(
          base::BindRepeating(&FeaturePodControllerBase::OnLabelPressed,
                              base::Unretained(controller)))) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kUnifiedFeaturePodSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddChildView(icon_button_.get());
  AddChildView(label_button_.get());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

FeaturePodButton::~FeaturePodButton() = default;

double FeaturePodButton::GetOpacityForExpandedAmount(double expanded_amount) {
  // TODO(amehfooz): Confirm the animation curve with UX.
  return std::max(0., 5. * expanded_amount - 4.);
}

void FeaturePodButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_button_->SetVectorIcon(icon);
}

void FeaturePodButton::SetLabel(const std::u16string& label) {
  if (label_button_->GetLabelText() == label)
    return;

  label_button_->SetLabel(label);
  DeprecatedLayoutImmediately();
  label_button_->SchedulePaint();
}

void FeaturePodButton::SetSubLabel(const std::u16string& sub_label) {
  if (label_button_->GetSubLabelText() == sub_label)
    return;

  label_button_->SetSubLabel(sub_label);
  DeprecatedLayoutImmediately();
  label_button_->SchedulePaint();
}

void FeaturePodButton::SetIconTooltip(const std::u16string& text) {
  icon_button_->SetTooltipText(text);
}

void FeaturePodButton::SetLabelTooltip(const std::u16string& text) {
  label_button_->SetTooltipText(text);
}

void FeaturePodButton::SetIconAndLabelTooltips(const std::u16string& text) {
  SetIconTooltip(text);
  SetLabelTooltip(text);
}

void FeaturePodButton::ShowDetailedViewArrow() {
  label_button_->ShowDetailedViewArrow();
  DeprecatedLayoutImmediately();
  label_button_->SchedulePaint();
}

void FeaturePodButton::DisableLabelButtonFocus() {
  label_button_->SetFocusBehavior(FocusBehavior::NEVER);
}

void FeaturePodButton::SetToggled(bool toggled) {
  icon_button_->SetToggled(toggled);
}

void FeaturePodButton::SetExpandedAmount(double expanded_amount,
                                         bool fade_icon_button) {
  label_button_->SetVisible(expanded_amount > 0.0);
  label_button_->layer()->SetOpacity(
      GetOpacityForExpandedAmount(expanded_amount));

  if (fade_icon_button)
    layer()->SetOpacity(GetOpacityForExpandedAmount(expanded_amount));
  else
    layer()->SetOpacity(1.0);
}

void FeaturePodButton::SetVisibleByContainer(bool visible) {
  View::SetVisible(visible);
}

void FeaturePodButton::SetVisible(bool visible) {
  visible_preferred_ = visible;
  View::SetVisible(visible);
}

bool FeaturePodButton::HasFocus() const {
  return icon_button_->HasFocus() || label_button_->HasFocus();
}

void FeaturePodButton::RequestFocus() {
  label_button_->RequestFocus();
}

void FeaturePodButton::OnEnabledChanged() {
  icon_button_->SetEnabled(GetEnabled());
  label_button_->SetEnabled(GetEnabled());
}

BEGIN_METADATA(FeaturePodButton)
END_METADATA

}  // namespace ash
