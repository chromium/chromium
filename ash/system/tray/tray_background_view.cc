// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include <algorithm>
#include <memory>

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/window_factory.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/transform.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

const int kAnimationDurationForBubblePopupMs = 200;

// Duration of opacity animation for visibility changes.
constexpr base::TimeDelta kAnimationDurationForVisibilityMs =
    base::TimeDelta::FromMilliseconds(250);

// Bounce animation constants
const base::TimeDelta kAnimationDurationForBounceElement =
    base::TimeDelta::FromMilliseconds(250);
const int kAnimationBounceDistance = 16;
const float kAnimationBounceScaleFactor = 0.5;

// When becoming visible delay the animation so that StatusAreaWidgetDelegate
// can animate sibling views out of the position to be occupied by the
// TrayBackgroundView.
const base::TimeDelta kShowAnimationDelayMs =
    base::TimeDelta::FromMilliseconds(100);

// Switches left and right insets if RTL mode is active.
void MirrorInsetsIfNecessary(gfx::Insets* insets) {
  if (base::i18n::IsRTL()) {
    insets->Set(insets->top(), insets->right(), insets->bottom(),
                insets->left());
  }
}

// Returns background insets relative to the contents bounds of the view and
// mirrored if RTL mode is active.
gfx::Insets GetMirroredBackgroundInsets(bool is_shelf_horizontal) {
  gfx::Insets insets;
  // "Primary" is the same direction as the shelf, "secondary" is orthogonal.
  const int primary_padding = 0;
  const int secondary_padding =
      -ash::ShelfConfig::Get()->status_area_hit_region_padding();

  if (is_shelf_horizontal) {
    insets.Set(secondary_padding, primary_padding, secondary_padding,
               primary_padding + ash::kTraySeparatorWidth);
  } else {
    insets.Set(primary_padding, secondary_padding,
               primary_padding + ash::kTraySeparatorWidth, secondary_padding);
  }
  MirrorInsetsIfNecessary(&insets);
  return insets;
}

class HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit HighlightPathGenerator(TrayBackgroundView* tray_background_view)
      : tray_background_view_(tray_background_view), insets_(gfx::Insets()) {}

  HighlightPathGenerator(TrayBackgroundView* tray_background_view,
                         gfx::Insets insets)
      : tray_background_view_(tray_background_view), insets_(insets) {}

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  // HighlightPathGenerator:
  base::Optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::RectF bounds(tray_background_view_->GetBackgroundBounds());
    bounds.Inset(insets_);
    return gfx::RRectF(bounds, ShelfConfig::Get()->control_border_radius());
  }

 private:
  TrayBackgroundView* const tray_background_view_;
  const gfx::Insets insets_;
};

}  // namespace

// static
const char TrayBackgroundView::kViewClassName[] = "tray/TrayBackgroundView";

// Used to track when the anchor widget changes position on screen so that the
// bubble position can be updated.
class TrayBackgroundView::TrayWidgetObserver : public views::WidgetObserver {
 public:
  explicit TrayWidgetObserver(TrayBackgroundView* host) : host_(host) {}

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    host_->AnchorUpdated();
  }

  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    host_->AnchorUpdated();
  }

  void Add(views::Widget* widget) { observer_.Add(widget); }

 private:
  TrayBackgroundView* host_;
  ScopedObserver<views::Widget, views::WidgetObserver> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(TrayWidgetObserver);
};

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView(Shelf* shelf)
    // Note the ink drop style is ignored.
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
      shelf_(shelf),
      tray_container_(new TrayContainer(shelf)),
      is_active_(false),
      separator_visible_(true),
      visible_preferred_(false),
      show_with_virtual_keyboard_(false),
      show_when_collapsed_(true),
      widget_observer_(new TrayWidgetObserver(this)) {
  DCHECK(shelf_);
  SetNotifyEnterExitOnChild(true);

  auto ripple_attributes = AshColorProvider::Get()->GetRippleAttributes();
  SetInkDropBaseColor(ripple_attributes.base_color);
  SetInkDropVisibleOpacity(ripple_attributes.inkdrop_opacity);

  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetInstallFocusRingOnFocus(true);

  focus_ring()->SetColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  focus_ring()->SetPathGenerator(std::make_unique<HighlightPathGenerator>(
      this, kTrayBackgroundFocusPadding));
  SetFocusPainter(nullptr);

  views::HighlightPathGenerator::Install(
      this, std::make_unique<HighlightPathGenerator>(this));

  AddChildView(tray_container_);

  tray_event_filter_ = std::make_unique<TrayEventFilter>();

  // Use layer color to provide background color. Note that children views
  // need to have their own layers to be visible.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);

  // Start the tray items not visible, because visibility changes are animated.
  views::View::SetVisible(false);
}

TrayBackgroundView::~TrayBackgroundView() {
  Shell::Get()->system_tray_model()->virtual_keyboard()->RemoveObserver(this);
  widget_observer_.reset();
}

void TrayBackgroundView::Initialize() {
  widget_observer_->Add(GetWidget());
  Shell::Get()->system_tray_model()->virtual_keyboard()->AddObserver(this);

  UpdateBackground();
}

// static
void TrayBackgroundView::InitializeBubbleAnimations(
    views::Widget* bubble_widget) {
  aura::Window* window = bubble_widget->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_HIDE);
  ::wm::SetWindowVisibilityAnimationDuration(
      window,
      base::TimeDelta::FromMilliseconds(kAnimationDurationForBubblePopupMs));
}

void TrayBackgroundView::SetVisiblePreferred(bool visible_preferred) {
  if (visible_preferred_ == visible_preferred)
    return;
  visible_preferred_ = visible_preferred;
  StartVisibilityAnimation(GetEffectiveVisibility());

  // We need to update which trays overflow after showing or hiding a tray.
  auto* status_area_widget = shelf_->GetStatusAreaWidget();
  if (status_area_widget) {
    status_area_widget->UpdateCollapseState();
    status_area_widget->LogVisiblePodCountMetric();
  }
}

void TrayBackgroundView::StartVisibilityAnimation(bool visible) {
  if (visible == layer()->GetTargetVisibility())
    return;

  if (visible) {
    views::View::SetVisible(true);
    // If SetVisible(true) is called while animating to not visible, then
    // views::View::SetVisible(true) is a no-op. When the previous animation
    // ends layer->SetVisible(false) is called. To prevent this
    // layer->SetVisible(true) immediately interrupts the animation of this
    // property, and keeps the layer visible.
    layer()->SetVisible(true);
  }

  if (visible) {
    if (use_bounce_in_animation_)
      BounceInAnimation();
    else
      FadeInAnimation();
  } else {
    HideAnimation();
  }
}

const char* TrayBackgroundView::GetClassName() const {
  return kViewClassName;
}

void TrayBackgroundView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  StatusAreaWidgetDelegate* delegate =
      shelf->GetStatusAreaWidget()->status_area_widget_delegate();
  if (!delegate || !delegate->ShouldFocusOut(reverse))
    return;

  shelf_->shelf_focus_cycler()->FocusOut(reverse, SourceView::kStatusAreaView);
}

void TrayBackgroundView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleNameForTray());

  if (LockScreen::HasInstance()) {
    GetViewAccessibility().OverrideNextFocus(LockScreen::Get()->widget());
  }

  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  GetViewAccessibility().OverridePreviousFocus(shelf_widget->hotseat_widget());
  GetViewAccessibility().OverrideNextFocus(shelf_widget->navigation_widget());
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

std::unique_ptr<views::InkDropRipple> TrayBackgroundView::CreateInkDropRipple()
    const {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetBackgroundInsets(), GetInkDropCenterBasedOnLastEvent(),
      ripple_attributes.base_color, ripple_attributes.inkdrop_opacity);
}

std::unique_ptr<views::InkDropHighlight>
TrayBackgroundView::CreateInkDropHighlight() const {
  gfx::Rect bounds = GetBackgroundBounds();
  // Currently, we don't handle view resize. To compensate for that, enlarge the
  // bounds by two tray icons so that the highlight looks good even if two more
  // icons are added when it is visible. Note that ink drop mask handles resize
  // correctly, so the extra highlight would be clipped.
  // TODO(mohsen): Remove this extra size when resize is handled properly (see
  // https://crbug.com/669253).
  const int icon_size = kTrayIconSize + 2 * kTrayImageItemPadding;
  bounds.set_width(bounds.width() + 2 * icon_size);
  bounds.set_height(bounds.height() + 2 * icon_size);
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(bounds.size()), ripple_attributes.base_color);
  highlight->set_visible_opacity(ripple_attributes.highlight_opacity);
  return highlight;
}

void TrayBackgroundView::OnVirtualKeyboardVisibilityChanged() {
  // We call the base class' SetVisible to skip animations.
  if (GetVisible() != GetEffectiveVisibility())
    views::View::SetVisible(GetEffectiveVisibility());
}

TrayBubbleView* TrayBackgroundView::GetBubbleView() {
  return nullptr;
}

void TrayBackgroundView::CloseBubble() {}

void TrayBackgroundView::ShowBubble(bool show_by_click) {}

void TrayBackgroundView::CalculateTargetBounds() {
  tray_container_->CalculateTargetBounds();
}

void TrayBackgroundView::UpdateLayout() {
  UpdateBackground();
  tray_container_->UpdateLayout();
}

void TrayBackgroundView::UpdateAfterLoginStatusChange() {
  // Handled in subclasses.
}

void TrayBackgroundView::UpdateAfterStatusAreaCollapseChange() {
  views::View::SetVisible(GetEffectiveVisibility());
}

void TrayBackgroundView::BubbleResized(const TrayBubbleView* bubble_view) {}

void TrayBackgroundView::UpdateBackground() {
  const int radius = ShelfConfig::Get()->control_border_radius();
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  layer()->SetRoundedCornerRadius(rounded_corners);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(
      ShelfConfig::Get()->GetShelfControlButtonBlurRadius());
  layer()->SetColor(ShelfConfig::Get()->GetShelfControlButtonColor());
  layer()->SetClipRect(GetBackgroundBounds());
}

void TrayBackgroundView::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  if (!visible_preferred_)
    views::View::SetVisible(false);
  else
    UpdateBackground();
}

void TrayBackgroundView::FadeInAnimation() {
  std::unique_ptr<ui::LayerAnimationSequence> fade_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  std::unique_ptr<ui::LayerAnimationElement> fade_in =
      ui::LayerAnimationElement::CreateOpacityElement(
          1.0f, kAnimationDurationForVisibilityMs);
  fade_sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      ui::LayerAnimationElement::OPACITY, kShowAnimationDelayMs));
  fade_sequence->AddElement(
      ui::LayerAnimationElement::CreateOpacityElement(0.0f, base::TimeDelta()));
  fade_sequence->AddElement(std::move(fade_in));

  gfx::Transform transform;
  if (shelf_->IsHorizontalAlignment())
    transform.Translate(width(), 0.0f);
  else
    transform.Translate(0.0f, height());

  std::unique_ptr<ui::LayerAnimationSequence> translate_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  translate_sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      ui::LayerAnimationElement::TRANSFORM, kShowAnimationDelayMs));
  translate_sequence->AddElement(
      ui::LayerAnimationElement::CreateTransformElement(transform,
                                                        base::TimeDelta()));
  translate_sequence->AddElement(
      ui::LayerAnimationElement::CreateTransformElement(
          gfx::Transform(), kAnimationDurationForVisibilityMs));
  layer()->GetAnimator()->StartTogether(
      {fade_sequence.release(), translate_sequence.release()});
}

void TrayBackgroundView::BounceInAnimation() {
  layer()->SetColor(color_utils::GetResultingPaintColor(
      ShelfConfig::Get()->GetShelfControlButtonColor(),
      AshColorProvider::Get()->GetBackgroundColor()));

  std::unique_ptr<ui::InterpolatedTransform> scale =
      std::make_unique<ui::InterpolatedScale>(
          gfx::Point3F(kAnimationBounceScaleFactor, kAnimationBounceScaleFactor,
                       1),
          gfx::Point3F(1, 1, 1));

  std::unique_ptr<ui::InterpolatedTransform> scale_about_pivot =
      std::make_unique<ui::InterpolatedTransformAboutPivot>(
          GetLocalBounds().CenterPoint(), std::move(scale));

  scale_about_pivot->SetChild(std::make_unique<ui::InterpolatedTranslation>(
      gfx::PointF(0, kAnimationBounceDistance),
      gfx::PointF(0, -kAnimationBounceDistance)));

  std::unique_ptr<ui::LayerAnimationElement> scale_and_move_up =
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(scale_about_pivot), kAnimationDurationForBounceElement);

  std::unique_ptr<ui::LayerAnimationElement> move_down =
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::make_unique<ui::InterpolatedTranslation>(
              gfx::PointF(0, -kAnimationBounceDistance),
              gfx::PointF(0, kAnimationBounceDistance)),
          kAnimationDurationForBounceElement);
  move_down->set_tween_type(gfx::Tween::EASE_IN);

  std::unique_ptr<ui::LayerAnimationElement> move_up =
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::make_unique<ui::InterpolatedTranslation>(
              gfx::PointF(0, kAnimationBounceDistance), gfx::PointF(0, 0)),
          kAnimationDurationForBounceElement);

  std::unique_ptr<ui::LayerAnimationSequence> sequence =
      std::make_unique<ui::LayerAnimationSequence>();

  sequence->AddElement(
      ui::LayerAnimationElement::CreateOpacityElement(1.0, base::TimeDelta()));
  sequence->AddElement(std::move(scale_and_move_up));
  sequence->AddElement(std::move(move_down));
  sequence->AddElement(std::move(move_up));
  sequence->AddObserver(this);

  layer()->GetAnimator()->StartAnimation(sequence.release());
}

void TrayBackgroundView::HideAnimation() {
  std::unique_ptr<ui::LayerAnimationSequence> fade_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  std::unique_ptr<ui::LayerAnimationElement> fade_out =
      ui::LayerAnimationElement::CreateOpacityElement(
          0.0f, kAnimationDurationForVisibilityMs);
  fade_sequence->AddElement(std::move(fade_out));

  gfx::Transform transform;
  if (shelf_->IsHorizontalAlignment())
    transform.Translate(width(), 0.0f);
  else
    transform.Translate(0.0f, height());

  std::unique_ptr<ui::LayerAnimationSequence> translate_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  translate_sequence->AddElement(
      ui::LayerAnimationElement::CreateTransformElement(
          transform, kAnimationDurationForVisibilityMs));
  fade_sequence->AddObserver(this);
  layer()->GetAnimator()->StartTogether(
      {fade_sequence.release(), translate_sequence.release()});
}

void TrayBackgroundView::SetIsActive(bool is_active) {
  if (is_active_ == is_active)
    return;
  is_active_ = is_active;
  AnimateInkDrop(is_active_ ? views::InkDropState::ACTIVATED
                            : views::InkDropState::DEACTIVATED,
                 nullptr);
}

void TrayBackgroundView::UpdateBubbleViewArrow(TrayBubbleView* bubble_view) {
  // Nothing to do here.
}

views::View* TrayBackgroundView::GetBubbleAnchor() const {
  return tray_container_;
}

gfx::Insets TrayBackgroundView::GetBubbleAnchorInsets() const {
  gfx::Insets anchor_insets = GetBubbleAnchor()->GetInsets();
  gfx::Insets tray_bg_insets = GetInsets();
  if (shelf_->alignment() == ShelfAlignment::kBottom ||
      shelf_->alignment() == ShelfAlignment::kBottomLocked) {
    return gfx::Insets(-tray_bg_insets.top(), anchor_insets.left(),
                       -tray_bg_insets.bottom(), anchor_insets.right());
  } else {
    return gfx::Insets(anchor_insets.top(), -tray_bg_insets.left(),
                       anchor_insets.bottom(), -tray_bg_insets.right());
  }
}

aura::Window* TrayBackgroundView::GetBubbleWindowContainer() {
  return Shell::GetContainer(
      tray_container()->GetWidget()->GetNativeWindow()->GetRootWindow(),
      kShellWindowId_SettingBubbleContainer);
}

gfx::Rect TrayBackgroundView::GetBackgroundBounds() const {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(GetBackgroundInsets());
  return bounds;
}

void TrayBackgroundView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateBackground();

  ActionableView::OnBoundsChanged(previous_bounds);
}

bool TrayBackgroundView::ShouldEnterPushedState(const ui::Event& event) {
  if (is_active_)
    return false;

  return ActionableView::ShouldEnterPushedState(event);
}

bool TrayBackgroundView::PerformAction(const ui::Event& event) {
  return false;
}

void TrayBackgroundView::HandlePerformActionResult(bool action_performed,
                                                   const ui::Event& event) {
  // When an action is performed, ink drop ripple is handled in SetIsActive().
  if (action_performed)
    return;
  ActionableView::HandlePerformActionResult(action_performed, event);
}

views::PaintInfo::ScaleType TrayBackgroundView::GetPaintScaleType() const {
  return views::PaintInfo::ScaleType::kUniformScaling;
}

gfx::Insets TrayBackgroundView::GetBackgroundInsets() const {
  gfx::Insets insets =
      GetMirroredBackgroundInsets(shelf_->IsHorizontalAlignment());

  // |insets| are relative to contents bounds. Change them to be relative to
  // local bounds.
  gfx::Insets local_contents_insets =
      GetLocalBounds().InsetsFrom(GetContentsBounds());
  MirrorInsetsIfNecessary(&local_contents_insets);
  insets += local_contents_insets;

  if (Shell::Get()->IsInTabletMode() && ShelfConfig::Get()->is_in_app()) {
    insets += gfx::Insets(
        ShelfConfig::Get()->in_app_control_button_height_inset(), 0);
  }

  return insets;
}

bool TrayBackgroundView::GetEffectiveVisibility() {
  // When the virtual keyboard is visible, the effective visibility of the view
  // is solely determined by |show_with_virtual_keyboard_|.
  if (Shell::Get()->system_tray_model()->virtual_keyboard()->visible())
    return show_with_virtual_keyboard_;

  if (!visible_preferred_)
    return false;

  DCHECK(GetWidget());

  // When the status area is collapsed, the effective visibility of the view is
  // determined by |show_when_collapsed_|.
  StatusAreaWidget::CollapseState collapse_state =
      Shelf::ForWindow(GetWidget()->GetNativeWindow())
          ->GetStatusAreaWidget()
          ->collapse_state();
  if (collapse_state == StatusAreaWidget::CollapseState::COLLAPSED)
    return show_when_collapsed_;

  return true;
}

}  // namespace ash
