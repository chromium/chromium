// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pod_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;

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

FeaturePodIconButton::FeaturePodIconButton(views::ButtonListener* listener,
                                           bool is_togglable)
    : views::ImageButton(listener), is_togglable_(is_togglable) {
  SetPreferredSize(kUnifiedFeaturePodIconSize);
  SetBorder(views::CreateEmptyBorder(kUnifiedFeaturePodIconPadding));
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  GetViewAccessibility().OverrideIsLeaf(true);

  // Focus ring is around the whole view's bounds, but the ink drop should be
  // the same size as the content.
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  focus_ring()->SetColor(UnifiedSystemTrayView::GetFocusRingColor());
  focus_ring()->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));
  views::InstallCircleHighlightPathGenerator(this,
                                             kUnifiedFeaturePodIconPadding);
}

FeaturePodIconButton::~FeaturePodIconButton() = default;

void FeaturePodIconButton::SetToggled(bool toggled) {
  if (!is_togglable_ || toggled_ == toggled)
    return;

  toggled_ = toggled;
  UpdateVectorIcon();
}

void FeaturePodIconButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;
  UpdateVectorIcon();
}

void FeaturePodIconButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  const AshColorProvider* color_provider = AshColorProvider::Get();
  SkColor color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  if (GetEnabled()) {
    if (toggled_) {
      color = color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);
    }
  } else {
    color = AshColorProvider::GetDisabledColor(color);
  }
  flags.setColor(color);

  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), rect.width() / 2, flags);

  views::ImageButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> FeaturePodIconButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple>
FeaturePodIconButton::CreateInkDropRipple() const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
FeaturePodIconButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(this);
}

void FeaturePodIconButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ImageButton::GetAccessibleNodeData(node_data);
  node_data->SetName(GetTooltipText(gfx::Point()));
  if (is_togglable_) {
    node_data->role = ax::mojom::Role::kToggleButton;
    node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
  } else {
    node_data->role = ax::mojom::Role::kButton;
  }
}

const char* FeaturePodIconButton::GetClassName() const {
  return "FeaturePodIconButton";
}

void FeaturePodIconButton::UpdateVectorIcon() {
  if (!icon_)
    return;

  const auto* color_provider = AshColorProvider::Get();
  const SkColor normal_color =
      color_provider->GetContentLayerColor(ContentLayerType::kButtonIconColor);
  const SkColor toggled_icon_color = color_provider->GetContentLayerColor(
      ContentLayerType::kButtonIconColorPrimary);
  const SkColor icon_color = toggled_ ? toggled_icon_color : normal_color;

  // Skip repainting if the incoming icon is the same as the current icon. If
  // the icon has been painted before, |gfx::CreateVectorIcon()| will simply
  // grab the ImageSkia from a cache, so it will be cheap. Note that this
  // assumes that toggled/disabled images changes at the same time as the normal
  // image, which it currently does.
  const gfx::ImageSkia new_normal_image = gfx::CreateVectorIcon(
      *icon_, kUnifiedFeaturePodVectorIconSize, icon_color);
  const gfx::ImageSkia& old_normal_image =
      GetImage(views::Button::STATE_NORMAL);
  if (!new_normal_image.isNull() && !old_normal_image.isNull() &&
      new_normal_image.BackedBySameObjectAs(old_normal_image)) {
    return;
  }

  SetImage(views::Button::STATE_NORMAL, new_normal_image);
  SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(*icon_, kUnifiedFeaturePodVectorIconSize,
                            AshColorProvider::GetDisabledColor(normal_color)));
}

FeaturePodLabelButton::FeaturePodLabelButton(views::ButtonListener* listener)
    : Button(listener),
      label_(new views::Label),
      sub_label_(new views::Label),
      detailed_view_arrow_(new views::ImageView) {
  SetBorder(views::CreateEmptyBorder(kUnifiedFeaturePodHoverPadding));
  GetViewAccessibility().OverrideIsLeaf(true);

  label_->SetLineHeight(kUnifiedFeaturePodLabelLineHeight);
  ConfigureFeaturePodLabel(label_, kUnifiedFeaturePodLabelLineHeight,
                           kUnifiedFeaturePodLabelFontSize);
  ConfigureFeaturePodLabel(sub_label_, kUnifiedFeaturePodSubLabelLineHeight,
                           kUnifiedFeaturePodSubLabelFontSize);
  sub_label_->SetVisible(false);

  detailed_view_arrow_->SetCanProcessEventsWithinSubtree(false);
  detailed_view_arrow_->SetVisible(false);

  OnEnabledChanged();

  AddChildView(label_);
  AddChildView(detailed_view_arrow_);
  AddChildView(sub_label_);

  TrayPopupUtils::ConfigureTrayPopupButton(this);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  focus_ring()->SetColor(UnifiedSystemTrayView::GetFocusRingColor());
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kUnifiedFeaturePodHoverCornerRadius);
}

FeaturePodLabelButton::~FeaturePodLabelButton() = default;

void FeaturePodLabelButton::Layout() {
  DCHECK(focus_ring());
  focus_ring()->Layout();
  LayoutInCenter(label_, GetContentsBounds().y());
  LayoutInCenter(sub_label_, GetContentsBounds().CenterPoint().y() +
                                 kUnifiedFeaturePodInterLabelPadding);

  if (!detailed_view_arrow_->GetVisible())
    return;

  // We need custom Layout() because |label_| is first laid out in the center
  // without considering |detailed_view_arrow_|, then |detailed_view_arrow_| is
  // placed on the right side of |label_|.
  gfx::Size arrow_size = detailed_view_arrow_->GetPreferredSize();
  detailed_view_arrow_->SetBoundsRect(gfx::Rect(
      gfx::Point(label_->bounds().right() + kUnifiedFeaturePodArrowSpacing,
                 label_->bounds().CenterPoint().y() - arrow_size.height() / 2),
      arrow_size));
}

gfx::Size FeaturePodLabelButton::CalculatePreferredSize() const {
  // Minimum width of the button
  int width = kUnifiedFeaturePodLabelWidth + GetInsets().width();
  if (detailed_view_arrow_->GetVisible()) {
    const int label_width = std::min(kUnifiedFeaturePodLabelWidth,
                                     label_->GetPreferredSize().width());
    // Symmetrically increase the width to accommodate the arrow
    const int extra_space_for_arrow =
        2 * (kUnifiedFeaturePodArrowSpacing +
             detailed_view_arrow_->GetPreferredSize().width());
    width = std::max(width,
                     label_width + extra_space_for_arrow + GetInsets().width());
  }

  // Make sure there is sufficient margin around the label.
  int horizontal_margin = width - label_->GetPreferredSize().width();
  if (horizontal_margin < 2 * kUnifiedFeaturePodMinimumHorizontalMargin)
    width += 2 * kUnifiedFeaturePodMinimumHorizontalMargin - horizontal_margin;

  int height = label_->GetPreferredSize().height() + GetInsets().height();
  if (sub_label_->GetVisible()) {
    height += kUnifiedFeaturePodInterLabelPadding +
              sub_label_->GetPreferredSize().height();
  }

  return gfx::Size(width, height);
}

std::unique_ptr<views::InkDrop> FeaturePodLabelButton::CreateInkDrop() {
  auto ink_drop = TrayPopupUtils::CreateInkDrop(this);
  ink_drop->SetShowHighlightOnHover(true);
  return ink_drop;
}

std::unique_ptr<views::InkDropRipple>
FeaturePodLabelButton::CreateInkDropRipple() const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
FeaturePodLabelButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(this);
}

const char* FeaturePodLabelButton::GetClassName() const {
  return "FeaturePodLabelButton";
}

void FeaturePodLabelButton::SetLabel(const base::string16& label) {
  label_->SetText(label);
  InvalidateLayout();
}

const base::string16& FeaturePodLabelButton::GetLabelText() const {
  return label_->GetText();
}

void FeaturePodLabelButton::SetSubLabel(const base::string16& sub_label) {
  sub_label_->SetText(sub_label);
  sub_label_->SetVisible(true);
  InvalidateLayout();
}

const base::string16& FeaturePodLabelButton::GetSubLabelText() const {
  return sub_label_->GetText();
}

void FeaturePodLabelButton::ShowDetailedViewArrow() {
  detailed_view_arrow_->SetVisible(true);
  InvalidateLayout();
}

void FeaturePodLabelButton::OnEnabledChanged() {
  const AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor primary_text_color =
      color_provider->GetContentLayerColor(ContentLayerType::kTextColorPrimary);
  const SkColor secondary_text_color = color_provider->GetContentLayerColor(
      ContentLayerType::kTextColorSecondary);
  label_->SetEnabledColor(
      GetEnabled() ? primary_text_color
                   : AshColorProvider::GetDisabledColor(primary_text_color));
  sub_label_->SetEnabledColor(
      GetEnabled() ? secondary_text_color
                   : AshColorProvider::GetDisabledColor(secondary_text_color));

  const SkColor icon_color =
      color_provider->GetContentLayerColor(ContentLayerType::kIconColorPrimary);
  detailed_view_arrow_->SetImage(gfx::CreateVectorIcon(
      kUnifiedMenuMoreIcon,
      GetEnabled() ? icon_color
                   : AshColorProvider::GetDisabledColor(icon_color)));
}

void FeaturePodLabelButton::LayoutInCenter(views::View* child, int y) {
  gfx::Rect contents_bounds = GetContentsBounds();
  gfx::Size preferred_size = child->GetPreferredSize();
  int child_width =
      std::min(kUnifiedFeaturePodLabelWidth, preferred_size.width());
  child->SetBounds(
      contents_bounds.x() + (contents_bounds.width() - child_width) / 2, y,
      child_width, preferred_size.height());
}

FeaturePodButton::FeaturePodButton(FeaturePodControllerBase* controller,
                                   bool is_togglable)
    : controller_(controller),
      icon_button_(new FeaturePodIconButton(this, is_togglable)),
      label_button_(new FeaturePodLabelButton(this)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kUnifiedFeaturePodSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddChildView(icon_button_);
  AddChildView(label_button_);

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

void FeaturePodButton::SetLabel(const base::string16& label) {
  if (label_button_->GetLabelText() == label)
    return;

  label_button_->SetLabel(label);
  Layout();
  label_button_->SchedulePaint();
}

void FeaturePodButton::SetSubLabel(const base::string16& sub_label) {
  if (label_button_->GetSubLabelText() == sub_label)
    return;

  label_button_->SetSubLabel(sub_label);
  Layout();
  label_button_->SchedulePaint();
}

void FeaturePodButton::SetIconTooltip(const base::string16& text) {
  icon_button_->SetTooltipText(text);
}

void FeaturePodButton::SetLabelTooltip(const base::string16& text) {
  label_button_->SetTooltipText(text);
}

void FeaturePodButton::SetIconAndLabelTooltips(const base::string16& text) {
  SetIconTooltip(text);
  SetLabelTooltip(text);
}

void FeaturePodButton::ShowDetailedViewArrow() {
  label_button_->ShowDetailedViewArrow();
  Layout();
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

const char* FeaturePodButton::GetClassName() const {
  return "FeaturePodButton";
}

void FeaturePodButton::OnEnabledChanged() {
  icon_button_->SetEnabled(GetEnabled());
  label_button_->SetEnabled(GetEnabled());
}

void FeaturePodButton::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == label_button_) {
    controller_->OnLabelPressed();
    return;
  }
  controller_->OnIconPressed();
}

}  // namespace ash
