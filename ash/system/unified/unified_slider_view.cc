// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_slider_view.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;

namespace {

// Custom the slider to use different colors.
class SystemSlider : public views::Slider {
 public:
  explicit SystemSlider(views::SliderListener* listener = nullptr)
      : views::Slider(listener) {}
  SystemSlider(const SystemSlider&) = delete;
  SystemSlider& operator=(const SystemSlider&) = delete;
  ~SystemSlider() override {}

 private:
  // views::Slider:
  SkColor GetThumbColor() const override {
    using Type = AshColorProvider::ContentLayerType;
    return AshColorProvider::Get()->GetContentLayerColor(
        (style() == RenderingStyle::kMinimalStyle) ? Type::kSliderColorInactive
                                                   : Type::kSliderColorActive);
  }

  // views::Slider:
  SkColor GetTroughColor() const override {
    return AshColorProvider::Get()->GetSecondToneColor(GetThumbColor());
  }

  // views::View:
  void OnThemeChanged() override {
    views::Slider::OnThemeChanged();
    SchedulePaint();
  }
};

// A slider that ignores inputs.
class ReadOnlySlider : public SystemSlider {
 public:
  ReadOnlySlider() : SystemSlider() {}
  ReadOnlySlider(const ReadOnlySlider&) = delete;
  ReadOnlySlider& operator=(const ReadOnlySlider&) = delete;
  ~ReadOnlySlider() override {}

 private:
  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override { return false; }
  bool OnMouseDragged(const ui::MouseEvent& event) override { return false; }
  void OnMouseReleased(const ui::MouseEvent& event) override {}
  bool OnKeyPressed(const ui::KeyEvent& event) override { return false; }
  const char* GetClassName() const override { return "ReadOnlySlider"; }

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {}
};

std::unique_ptr<views::Slider> CreateSlider(UnifiedSliderListener* listener,
                                            bool readonly) {
  return readonly ? std::make_unique<ReadOnlySlider>()
                  : std::make_unique<SystemSlider>(listener);
}

}  // namespace

UnifiedSliderButton::UnifiedSliderButton(PressedCallback callback,
                                         const gfx::VectorIcon& icon,
                                         int accessible_name_id)
    : views::ImageButton(std::move(callback)) {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  if (accessible_name_id)
    SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));

  SetVectorIcon(icon);
  SetBorder(views::CreateEmptyBorder(kUnifiedCircularButtonFocusPadding));

  // Focus ring is around the whole view's bounds, but the ink drop should be
  // the same size as the content.
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  focus_ring()->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));
  views::InstallCircleHighlightPathGenerator(
      this, kUnifiedCircularButtonFocusPadding);
}

UnifiedSliderButton::~UnifiedSliderButton() = default;

void UnifiedSliderButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;
  UpdateVectorIcon();
}

void UnifiedSliderButton::SetToggled(bool toggled) {
  toggled_ = toggled;
  UpdateVectorIcon();
}

void UnifiedSliderButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      toggled_
          ? AshColorProvider::ControlsLayerType::kControlBackgroundColorActive
          : AshColorProvider::ControlsLayerType::
                kControlBackgroundColorInactive));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), kTrayItemCornerRadius,
                     flags);

  views::ImageButton::PaintButtonContents(canvas);
}

std::unique_ptr<views::InkDrop> UnifiedSliderButton::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> UnifiedSliderButton::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
UnifiedSliderButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(this);
}

void UnifiedSliderButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (!GetEnabled())
    return;
  views::ImageButton::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                      : ax::mojom::CheckedState::kFalse);
}

const char* UnifiedSliderButton::GetClassName() const {
  return "UnifiedSliderButton";
}

gfx::Size UnifiedSliderButton::CalculatePreferredSize() const {
  return gfx::Size(kTrayItemSize + kUnifiedCircularButtonFocusPadding.width(),
                   kTrayItemSize + kUnifiedCircularButtonFocusPadding.height());
}

void UnifiedSliderButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  UpdateVectorIcon();
  focus_ring()->SetColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  SchedulePaint();
}

void UnifiedSliderButton::UpdateVectorIcon() {
  if (!icon_)
    return;

  AshColorProvider::Get()->DecorateIconButton(
      this, *icon_, toggled_, GetDefaultSizeOfVectorIcon(*icon_));
}

UnifiedSliderView::UnifiedSliderView(views::Button::PressedCallback callback,
                                     UnifiedSliderListener* listener,
                                     const gfx::VectorIcon& icon,
                                     int accessible_name_id,
                                     bool readonly)
    : button_(AddChildView(
          std::make_unique<UnifiedSliderButton>(std::move(callback),
                                                icon,
                                                accessible_name_id))),
      slider_(AddChildView(CreateSlider(listener, readonly))) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedSliderRowPadding,
      kUnifiedSliderViewSpacing));

  // Prevent an accessibility event while initiallizing this view. Typically
  // the first update of the slider value is conducted by the caller function
  // to reflect the current value.
  slider_->SetEnableAccessibilityEvents(false);

  slider_->GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(accessible_name_id));
  slider_->SetBorder(views::CreateEmptyBorder(kUnifiedSliderPadding));
  slider_->SetPreferredSize(gfx::Size(0, kTrayItemSize));
  layout->SetFlexForView(slider_, 1);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void UnifiedSliderView::SetSliderValue(float value, bool by_user) {
  // SetValue() calls |listener|, so we should ignore the call when the widget
  // is closed, because controllers are already deleted.
  // It should allow the case GetWidget() returning null, so that initial
  // position can be properly set by controllers before the view is attached to
  // a widget.
  if (GetWidget() && GetWidget()->IsClosed())
    return;

  slider_->SetValue(value);
  if (by_user)
    slider_->SetEnableAccessibilityEvents(true);
}

const char* UnifiedSliderView::GetClassName() const {
  return "UnifiedSliderView";
}

UnifiedSliderView::~UnifiedSliderView() = default;

}  // namespace ash
