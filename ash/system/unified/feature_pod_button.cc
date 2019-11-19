// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pod_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;
using AshColorMode = AshColorProvider::AshColorMode;

namespace {

void ConfigureFeaturePodLabel(views::Label* label) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->set_can_process_events_within_subtree(false);
}

}  // namespace

FeaturePodIconButton::FeaturePodIconButton(views::ButtonListener* listener,
                                           bool is_togglable)
    : views::ImageButton(listener), is_togglable_(is_togglable) {
  SetPreferredSize(kUnifiedFeaturePodIconSize);
  SetBorder(views::CreateEmptyBorder(kUnifiedFeaturePodIconPadding));
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallCircleHighlightPathGenerator(this);
}

FeaturePodIconButton::~FeaturePodIconButton() = default;

void FeaturePodIconButton::SetToggled(bool toggled) {
  if (!is_togglable_)
    return;

  toggled_ = toggled;
  SchedulePaint();
}

void FeaturePodIconButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  const AshColorProvider* color_provider = AshColorProvider::Get();
  SkColor color = color_provider->DeprecatedGetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kInactiveControlBackground,
      kUnifiedMenuButtonColor);
  if (GetEnabled()) {
    if (toggled_) {
      color = color_provider->DeprecatedGetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kActiveControlBackground,
          kUnifiedMenuButtonColorActive);
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
      GetInkDropCenterBasedOnLastEvent(),
      UnifiedSystemTrayView::GetBackgroundColor());
}

std::unique_ptr<views::InkDropHighlight>
FeaturePodIconButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      UnifiedSystemTrayView::GetBackgroundColor());
}

std::unique_ptr<views::InkDropMask> FeaturePodIconButton::CreateInkDropMask()
    const {
  gfx::Rect rect(GetContentsBounds());
  return std::make_unique<views::CircleInkDropMask>(size(), rect.CenterPoint(),
                                                    rect.width() / 2);
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

FeaturePodLabelButton::FeaturePodLabelButton(views::ButtonListener* listener)
    : Button(listener),
      label_(new views::Label),
      sub_label_(new views::Label),
      detailed_view_arrow_(new views::ImageView) {
  SetBorder(views::CreateEmptyBorder(kUnifiedFeaturePodHoverPadding));

  ConfigureFeaturePodLabel(label_);
  ConfigureFeaturePodLabel(sub_label_);
  sub_label_->SetVisible(false);

  detailed_view_arrow_->set_can_process_events_within_subtree(false);
  detailed_view_arrow_->SetVisible(false);

  OnEnabledChanged();

  AddChildView(label_);
  AddChildView(detailed_view_arrow_);
  AddChildView(sub_label_);

  TrayPopupUtils::ConfigureTrayPopupButton(this);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

FeaturePodLabelButton::~FeaturePodLabelButton() = default;

void FeaturePodLabelButton::Layout() {
  DCHECK(focus_ring());
  focus_ring()->Layout();
  LayoutInCenter(label_, GetContentsBounds().y());
  LayoutInCenter(sub_label_, GetContentsBounds().CenterPoint().y());

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

  int height = label_->GetPreferredSize().height() + GetInsets().height();
  if (sub_label_->GetVisible())
    height += sub_label_->GetPreferredSize().height();

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
      GetInkDropCenterBasedOnLastEvent(),
      UnifiedSystemTrayView::GetBackgroundColor());
}

std::unique_ptr<views::InkDropHighlight>
FeaturePodLabelButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      UnifiedSystemTrayView::GetBackgroundColor());
}

std::unique_ptr<views::InkDropMask> FeaturePodLabelButton::CreateInkDropMask()
    const {
  return std::make_unique<views::RoundRectInkDropMask>(
      size(), gfx::Insets(), kUnifiedFeaturePodHoverRadius);
}

const char* FeaturePodLabelButton::GetClassName() const {
  return "FeaturePodLabelButton";
}

void FeaturePodLabelButton::SetLabel(const base::string16& label) {
  label_->SetText(label);
  InvalidateLayout();
}

void FeaturePodLabelButton::SetSubLabel(const base::string16& sub_label) {
  sub_label_->SetText(sub_label);
  sub_label_->SetVisible(true);
  InvalidateLayout();
}

void FeaturePodLabelButton::ShowDetailedViewArrow() {
  detailed_view_arrow_->SetVisible(true);
  InvalidateLayout();
}

void FeaturePodLabelButton::OnEnabledChanged() {
  const AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor primary_text_color =
      color_provider->DeprecatedGetContentLayerColor(
          ContentLayerType::kTextPrimary, kUnifiedMenuTextColor);
  const SkColor secondary_text_color = color_provider->GetContentLayerColor(
      ContentLayerType::kTextSecondary, AshColorMode::kDark);
  label_->SetEnabledColor(
      GetEnabled() ? primary_text_color
                   : AshColorProvider::GetDisabledColor(primary_text_color));
  sub_label_->SetEnabledColor(
      GetEnabled() ? secondary_text_color
                   : AshColorProvider::GetDisabledColor(secondary_text_color));

  const SkColor icon_color = color_provider->GetContentLayerColor(
      ContentLayerType::kIconPrimary, AshColorMode::kDark);
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
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      ContentLayerType::kIconPrimary, AshColorMode::kDark);
  icon_button_->SetImage(views::Button::STATE_NORMAL,
                         gfx::CreateVectorIcon(icon, icon_color));
  icon_button_->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(icon,
                            AshColorProvider::GetDisabledColor(icon_color)));
}

void FeaturePodButton::SetLabel(const base::string16& label) {
  label_button_->SetLabel(label);
  Layout();
  label_button_->SchedulePaint();
}

void FeaturePodButton::SetSubLabel(const base::string16& sub_label) {
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
