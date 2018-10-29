// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include <algorithm>
#include <memory>

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/lock_window.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager.h"
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
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/transform.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/wm/core/window_animations.h"

namespace {

const int kAnimationDurationForPopupMs = 200;

// Duration of opacity animation for visibility changes.
const int kAnimationDurationForVisibilityMs = 250;

// When becoming visible delay the animation so that StatusAreaWidgetDelegate
// can animate sibling views out of the position to be occuped by the
// TrayBackgroundView.
const int kShowAnimationDelayMs = 100;

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
  const int secondary_padding = -ash::kHitRegionPadding;
  const int separator_width = ash::TrayConstants::separator_width();

  if (is_shelf_horizontal) {
    insets.Set(secondary_padding, primary_padding, secondary_padding,
               primary_padding + separator_width);
  } else {
    insets.Set(primary_padding, secondary_padding,
               primary_padding + separator_width, secondary_padding);
  }
  MirrorInsetsIfNecessary(&insets);
  return insets;
}

}  // namespace

namespace ash {

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

 private:
  TrayBackgroundView* host_;

  DISALLOW_COPY_AND_ASSIGN(TrayWidgetObserver);
};

class TrayBackground : public views::Background {
 public:
  explicit TrayBackground(TrayBackgroundView* tray_background_view)
      : tray_background_view_(tray_background_view),
        color_(SK_ColorTRANSPARENT) {}

  ~TrayBackground() override = default;

  void set_color(SkColor color) { color_ = color; }

 private:
  // Overridden from views::Background.
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    cc::PaintFlags background_flags;
    background_flags.setAntiAlias(true);
    int border_radius = kTrayRoundedBorderRadius;
    background_flags.setColor(kShelfControlPermanentHighlightBackground);
    border_radius = ShelfConstants::control_border_radius();

    gfx::Rect bounds = tray_background_view_->GetBackgroundBounds();
    const float dsf = canvas->UndoDeviceScaleFactor();
    canvas->DrawRoundRect(gfx::ScaleToRoundedRect(bounds, dsf),
                          border_radius * dsf, background_flags);
  }

  // Reference to the TrayBackgroundView for which this is a background.
  TrayBackgroundView* tray_background_view_;

  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackground);
};

// CloseBubbleObserver is used to delay closing the tray bubbles until the
// animation completes.
class CloseBubbleObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit CloseBubbleObserver(TrayBackgroundView* tray_background_view)
      : tray_background_view_(tray_background_view) {}

  ~CloseBubbleObserver() override = default;

  void OnImplicitAnimationsCompleted() override {
    tray_background_view_->CloseBubble();
    delete this;
  }

 private:
  TrayBackgroundView* tray_background_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CloseBubbleObserver);
};

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView(Shelf* shelf)
    // Note the ink drop style is ignored.
    : ActionableView(nullptr, TrayPopupInkDropStyle::FILL_BOUNDS),
      shelf_(shelf),
      tray_container_(new TrayContainer(shelf)),
      background_(new TrayBackground(this)),
      is_active_(false),
      separator_visible_(true),
      visible_preferred_(false),
      show_with_virtual_keyboard_(false),
      widget_observer_(new TrayWidgetObserver(this)) {
  DCHECK(shelf_);
  set_notify_enter_exit_on_child(true);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBackground(std::unique_ptr<views::Background>(background_));

  AddChildView(tray_container_);

  tray_event_filter_.reset(new TrayEventFilter);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // Start the tray items not visible, because visibility changes are animated.
  views::View::SetVisible(false);
}

TrayBackgroundView::~TrayBackgroundView() {
  Shell::Get()->system_tray_model()->virtual_keyboard()->RemoveObserver(this);
  if (GetWidget())
    GetWidget()->RemoveObserver(widget_observer_.get());
  StopObservingImplicitAnimations();
}

void TrayBackgroundView::Initialize() {
  GetWidget()->AddObserver(widget_observer_.get());
  Shell::Get()->system_tray_model()->virtual_keyboard()->AddObserver(this);
}

// static
void TrayBackgroundView::InitializeBubbleAnimations(
    views::Widget* bubble_widget) {
  aura::Window* window = bubble_widget->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_HIDE);
  ::wm::SetWindowVisibilityAnimationDuration(
      window, base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMs));
}

void TrayBackgroundView::SetVisible(bool visible) {
  visible_preferred_ = visible;

  // If virtual keyboard is visible and TrayBackgroundView is hidden because of
  // that, ignore SetVisible() call. |visible_preferred_|  will be restored
  // in OnVirtualKeyboardVisibilityChanged() when virtual keyboard is hidden.
  if (!show_with_virtual_keyboard_ &&
      Shell::Get()->system_tray_model()->virtual_keyboard()->visible()) {
    return;
  }

  if (visible == layer()->GetTargetVisibility())
    return;

  if (visible) {
    // The alignment of the shelf can change while the TrayBackgroundView is
    // hidden. Reset the offscreen transform so that the animation to becoming
    // visible reflects the current layout.
    HideTransformation();
    // SetVisible(false) is defered until the animation for hiding is done.
    // Otherwise the view is immediately hidden and the animation does not
    // render.
    views::View::SetVisible(true);
    // If SetVisible(true) is called while animating to not visible, then
    // views::View::SetVisible(true) is a no-op. When the previous animation
    // ends layer->SetVisible(false) is called. To prevent this
    // layer->SetVisible(true) immediately interrupts the animation of this
    // property, and keeps the layer visible.
    layer()->SetVisible(true);
  }

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationForVisibilityMs));
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  if (visible) {
    animation.SetTweenType(gfx::Tween::EASE_OUT);
    // Show is delayed so as to allow time for other children of
    // StatusAreaWidget to begin animating to their new positions.
    layer()->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(kShowAnimationDelayMs),
        ui::LayerAnimationElement::OPACITY |
            ui::LayerAnimationElement::TRANSFORM);
    layer()->SetOpacity(1.0f);
    gfx::Transform transform;
    transform.Translate(0.0f, 0.0f);
    layer()->SetTransform(transform);
  } else {
    // Listen only to the hide animation. As we cannot turn off visibility
    // until the animation is over.
    animation.AddObserver(this);
    animation.SetTweenType(gfx::Tween::EASE_IN);
    layer()->SetOpacity(0.0f);
    layer()->SetVisible(false);
    HideTransformation();
  }
}

void TrayBackgroundView::Layout() {
  ActionableView::Layout();

  // The tray itself expands to the right and bottom edge of the screen to make
  // sure clicking on the edges brings up the popup. However, the focus border
  // should be only around the container.
  gfx::Rect paint_bounds(GetBackgroundBounds());
  paint_bounds.Inset(gfx::Insets(-kFocusBorderThickness));
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, kFocusBorderThickness,
      GetLocalBounds().InsetsFrom(paint_bounds)));
}

const char* TrayBackgroundView::GetClassName() const {
  return kViewClassName;
}

void TrayBackgroundView::OnGestureEvent(ui::GestureEvent* event) {
  if (drag_controller())
    drag_controller_->ProcessGestureEvent(event, this);

  if (!event->handled())
    ActionableView::OnGestureEvent(event);
}

void TrayBackgroundView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  StatusAreaWidgetDelegate* delegate =
      shelf->GetStatusAreaWidget()->status_area_widget_delegate();
  if (!delegate || !delegate->ShouldFocusOut(reverse))
    return;
  // Focus shelf widget when shift+tab is used and views-based shelf is shown.
  if (reverse && ShelfWidget::IsUsingViewsShelf()) {
    shelf->shelf_widget()->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
  } else {
    // Focus should leave the system tray if:
    // 1) Tab is used, or
    // 2) Shift+tab is used but views-based shelf is disabled. The shelf is not
    // part of the system tray in this case.
    Shell::Get()->system_tray_notifier()->NotifyFocusOut(reverse);
  }
}

void TrayBackgroundView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleNameForTray());

  if (LockScreen::HasInstance()) {
    int next_id = views::AXAuraObjCache::GetInstance()->GetID(
        static_cast<views::Widget*>(LockScreen::Get()->window()));
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kNextFocusId, next_id);
  }

  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  int previous_id = views::AXAuraObjCache::GetInstance()->GetID(shelf_widget);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPreviousFocusId,
                             previous_id);
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

std::unique_ptr<views::InkDropRipple> TrayBackgroundView::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetBackgroundInsets(), GetInkDropCenterBasedOnLastEvent(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
TrayBackgroundView::CreateInkDropHighlight() const {
  gfx::Rect bounds = GetBackgroundBounds();
  // Currently, we don't handle view resize. To compensate for that, enlarge the
  // bounds by two tray icons so that the hightlight looks good even if two more
  // icons are added when it is visible. Note that ink drop mask handles resize
  // correctly, so the extra highlight would be clipped.
  // TODO(mohsen): Remove this extra size when resize is handled properly (see
  // https://crbug.com/669253).
  const int icon_size = kTrayIconSize + 2 * kTrayImageItemPadding;
  bounds.set_width(bounds.width() + 2 * icon_size);
  bounds.set_height(bounds.height() + 2 * icon_size);
  std::unique_ptr<views::InkDropHighlight> highlight(
      new views::InkDropHighlight(bounds.size(), 0,
                                  gfx::RectF(bounds).CenterPoint(),
                                  GetInkDropBaseColor()));
  highlight->set_visible_opacity(kTrayPopupInkDropHighlightOpacity);
  return highlight;
}

void TrayBackgroundView::PaintButtonContents(gfx::Canvas* canvas) {
  if (shelf()->GetBackgroundType() ==
          ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT ||
      !separator_visible_) {
    return;
  }
  // In the given |canvas|, for a horizontal shelf draw a separator line to the
  // right or left of the TrayBackgroundView when the system is LTR or RTL
  // aligned, respectively. For a vertical shelf draw the separator line
  // underneath the items instead.
  const gfx::Rect local_bounds = GetLocalBounds();
  const SkColor color = SkColorSetA(SK_ColorWHITE, 0x4D);
  const int shelf_size = ShelfConstants::shelf_size();
  const int separator_width = ash::TrayConstants::separator_width();

  if (shelf_->IsHorizontalAlignment()) {
    const gfx::PointF point(
        base::i18n::IsRTL() ? 0 : (local_bounds.width() - separator_width),
        (shelf_size - kTrayItemSize) / 2);
    const gfx::Vector2dF vector(0, kTrayItemSize);
    canvas->Draw1pxLine(point, point + vector, color);
  } else {
    const gfx::PointF point((shelf_size - kTrayItemSize) / 2,
                            local_bounds.height() - separator_width);
    const gfx::Vector2dF vector(kTrayItemSize, 0);
    canvas->Draw1pxLine(point, point + vector, color);
  }
}

void TrayBackgroundView::ProcessGestureEventForBubble(ui::GestureEvent* event) {
  if (drag_controller())
    drag_controller_->ProcessGestureEvent(event, this);
}

void TrayBackgroundView::OnVirtualKeyboardVisibilityChanged() {
  if (show_with_virtual_keyboard_) {
    // The view always shows up when virtual keyboard is visible if
    // |show_with_virtual_keyboard| is true.
    views::View::SetVisible(
        Shell::Get()->system_tray_model()->virtual_keyboard()->visible() ||
        visible_preferred_);
    return;
  }

  // If virtual keyboard is hidden and current preferred visibility is true,
  // set the visibility to true. We call base class' SetVisible because we don't
  // want |visible_preferred_| to be updated here.
  views::View::SetVisible(
      !Shell::Get()->system_tray_model()->virtual_keyboard()->visible() &&
      visible_preferred_);
}

TrayBubbleView* TrayBackgroundView::GetBubbleView() {
  return nullptr;
}

void TrayBackgroundView::CloseBubble() {}

void TrayBackgroundView::ShowBubble(bool show_by_click) {}

void TrayBackgroundView::UpdateAfterShelfAlignmentChange() {
  tray_container_->UpdateAfterShelfAlignmentChange();
}

void TrayBackgroundView::UpdateAfterRootWindowBoundsChange(
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  // Do nothing by default. Child class may do something.
}

void TrayBackgroundView::AnchorUpdated() {
  if (GetBubbleView())
    UpdateClippingWindowBounds();
}

void TrayBackgroundView::BubbleResized(const TrayBubbleView* bubble_view) {}

void TrayBackgroundView::OnImplicitAnimationsCompleted() {
  // If there is another animation in the queue, the reverse animation was
  // triggered before the completion of animating to invisible. Do not turn off
  // the visibility so that the next animation may render. The value of
  // layer()->GetTargetVisibility() can be incorrect if the hide animation was
  // aborted to schedule an animation to become visible. As the new animation
  // is not yet added to the queue. crbug.com/374236
  if (layer()->GetAnimator()->is_animating() || layer()->GetTargetVisibility())
    return;
  views::View::SetVisible(false);
}

bool TrayBackgroundView::RequiresNotificationWhenAnimatorDestroyed() const {
  // This is needed so that OnImplicitAnimationsCompleted() is called even upon
  // destruction of the animator. This can occure when parallel animations
  // caused by ScreenRotationAnimator end before the animations of
  // TrayBackgroundView. This allows for a proper update to the visual state of
  // the view. (crbug.com/476667)
  return true;
}

void TrayBackgroundView::HideTransformation() {
  gfx::Transform transform;
  if (shelf_->IsHorizontalAlignment())
    transform.Translate(width(), 0.0f);
  else
    transform.Translate(0.0f, height());
  layer()->SetTransform(transform);
}

TrayBubbleView::AnchorAlignment TrayBackgroundView::GetAnchorAlignment() const {
  if (shelf_->alignment() == SHELF_ALIGNMENT_LEFT)
    return TrayBubbleView::ANCHOR_ALIGNMENT_LEFT;
  if (shelf_->alignment() == SHELF_ALIGNMENT_RIGHT)
    return TrayBubbleView::ANCHOR_ALIGNMENT_RIGHT;
  return TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
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

void TrayBackgroundView::UpdateShelfItemBackground(SkColor color) {
  background_->set_color(color);
  SchedulePaint();
}

views::View* TrayBackgroundView::GetBubbleAnchor() const {
  return tray_container_;
}

gfx::Insets TrayBackgroundView::GetBubbleAnchorInsets() const {
  gfx::Insets anchor_insets = GetBubbleAnchor()->GetInsets();
  gfx::Insets tray_bg_insets = GetInsets();
  if (GetAnchorAlignment() == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
    return gfx::Insets(-tray_bg_insets.top(), anchor_insets.left(),
                       -tray_bg_insets.bottom(), anchor_insets.right());
  } else {
    return gfx::Insets(anchor_insets.top(), -tray_bg_insets.left(),
                       anchor_insets.bottom(), -tray_bg_insets.right());
  }
}

void TrayBackgroundView::UpdateClippingWindowBounds() {
  if (clipping_window_.get())
    clipping_window_->SetBounds(shelf_->GetUserWorkAreaBounds());
}

aura::Window* TrayBackgroundView::GetBubbleWindowContainer() {
  aura::Window* container = Shell::GetContainer(
      tray_container()->GetWidget()->GetNativeWindow()->GetRootWindow(),
      kShellWindowId_SettingBubbleContainer);

  // Place the bubble in |container|, or in a window clipped to the work area
  // in maximize mode, to avoid tray bubble and shelf overlap when dragging the
  // bubble from the tray.
  if (Shell::Get()
          ->tablet_mode_controller()
          ->IsTabletModeWindowManagerEnabled() &&
      drag_controller()) {
    if (!clipping_window_.get()) {
      clipping_window_ = window_factory::NewWindow();
      clipping_window_->Init(ui::LAYER_NOT_DRAWN);
      clipping_window_->layer()->SetMasksToBounds(true);
      container->AddChild(clipping_window_.get());
      clipping_window_->Show();
    }
    clipping_window_->SetBounds(shelf_->GetUserWorkAreaBounds());
    return clipping_window_.get();
  }
  return container;
}

void TrayBackgroundView::AnimateToTargetBounds(const gfx::Rect& target_bounds,
                                               bool close_bubble) {
  const int kAnimationDurationMS = 200;

  ui::ScopedLayerAnimationSettings settings(
      GetBubbleView()->GetWidget()->GetNativeView()->layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMS));
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  if (close_bubble)
    settings.AddObserver(new CloseBubbleObserver(this));
  GetBubbleView()->GetWidget()->SetBounds(target_bounds);
}

gfx::Rect TrayBackgroundView::GetBackgroundBounds() const {
  gfx::Insets insets = GetBackgroundInsets();
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(insets);
  return bounds;
}

std::unique_ptr<views::InkDropMask> TrayBackgroundView::CreateInkDropMask()
    const {
  const int border_radius = ShelfConstants::control_border_radius();
  return std::make_unique<views::RoundRectInkDropMask>(
      size(), GetBackgroundInsets(), border_radius);
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

  return insets;
}


}  // namespace ash
