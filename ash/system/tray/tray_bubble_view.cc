// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_view.h"

#include <algorithm>

#include "base/macros.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

using views::BubbleBorder;
using views::BubbleFrameView;
using views::NonClientFrameView;
using views::View;
using views::ViewsDelegate;
using views::Widget;

namespace ash {

namespace {

BubbleBorder::Arrow GetArrowAlignment(
    TrayBubbleView::AnchorAlignment alignment) {
  if (alignment == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
    return base::i18n::IsRTL() ? BubbleBorder::BOTTOM_LEFT
                               : BubbleBorder::BOTTOM_RIGHT;
  }
  if (alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT)
    return BubbleBorder::LEFT_BOTTOM;
  return BubbleBorder::RIGHT_BOTTOM;
}

// Only one TrayBubbleView is visible at a time, but there are cases where the
// lifetimes of two different bubbles can overlap briefly.
int g_current_tray_bubble_showing_count_ = 0;

// Detects any mouse movement. This is needed to detect mouse movements by the
// user over the bubble if the bubble got created underneath the cursor.
class MouseMoveDetectorHost : public views::MouseWatcherHost {
 public:
  MouseMoveDetectorHost();
  ~MouseMoveDetectorHost() override;

  bool Contains(const gfx::Point& screen_point, MouseEventType type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MouseMoveDetectorHost);
};

MouseMoveDetectorHost::MouseMoveDetectorHost() {}

MouseMoveDetectorHost::~MouseMoveDetectorHost() {}

bool MouseMoveDetectorHost::Contains(const gfx::Point& screen_point,
                                     MouseEventType type) {
  return false;
}

// Custom layout for the bubble-view. Does the default box-layout if there is
// enough height. Otherwise, makes sure the bottom rows are visible.
class BottomAlignedBoxLayout : public views::BoxLayout {
 public:
  explicit BottomAlignedBoxLayout(TrayBubbleView* bubble_view)
      : BoxLayout(BoxLayout::kVertical), bubble_view_(bubble_view) {}

  ~BottomAlignedBoxLayout() override {}

 private:
  void Layout(View* host) override {
    if (host->height() >= host->GetPreferredSize().height() ||
        !bubble_view_->is_gesture_dragging()) {
      BoxLayout::Layout(host);
      return;
    }

    int consumed_height = 0;
    for (int i = host->child_count() - 1;
         i >= 0 && consumed_height < host->height(); --i) {
      View* child = host->child_at(i);
      if (!child->visible())
        continue;
      gfx::Size size = child->GetPreferredSize();
      child->SetBounds(0, host->height() - consumed_height - size.height(),
                       host->width(), size.height());
      consumed_height += size.height();
    }
  }

  TrayBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(BottomAlignedBoxLayout);
};

}  // namespace

TrayBubbleView::Delegate::~Delegate() {}

void TrayBubbleView::Delegate::BubbleViewDestroyed() {}

void TrayBubbleView::Delegate::OnMouseEnteredView() {}

void TrayBubbleView::Delegate::OnMouseExitedView() {}

base::string16 TrayBubbleView::Delegate::GetAccessibleNameForBubble() {
  return base::string16();
}

bool TrayBubbleView::Delegate::ShouldEnableExtraKeyboardAccessibility() {
  return false;
}

void TrayBubbleView::Delegate::HideBubble(const TrayBubbleView* bubble_view) {}

void TrayBubbleView::Delegate::ProcessGestureEventForBubble(
    ui::GestureEvent* event) {}

TrayBubbleView::InitParams::InitParams() = default;

TrayBubbleView::InitParams::InitParams(const InitParams& other) = default;

TrayBubbleView::RerouteEventHandler::RerouteEventHandler(
    TrayBubbleView* tray_bubble_view,
    aura::Env* aura_env)
    : tray_bubble_view_(tray_bubble_view), aura_env_(aura_env) {
  aura_env_->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
}

TrayBubbleView::RerouteEventHandler::~RerouteEventHandler() {
  aura_env_->RemovePreTargetHandler(this);
}

void TrayBubbleView::RerouteEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  // Do not handle a key event if it is targeted to the tray or its descendants,
  // or if the target has the tray as a transient ancestor. RerouteEventHandler
  // is for rerouting events which are not targetted to the tray. Those events
  // should be handled by the target.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* tray_window = tray_bubble_view_->GetWidget()->GetNativeView();
  if (target && (tray_window->Contains(target) ||
                 wm::HasTransientAncestor(target, tray_window))) {
    return;
  }

  // Only passes Tab, Shift+Tab, Esc to the widget as it can consume more key
  // events. e.g. Alt+Tab can be consumed as focus traversal by FocusManager.
  ui::KeyboardCode key_code = event->key_code();
  int flags = event->flags();
  if ((key_code == ui::VKEY_TAB && flags == ui::EF_NONE) ||
      (key_code == ui::VKEY_TAB && flags == ui::EF_SHIFT_DOWN) ||
      (key_code == ui::VKEY_ESCAPE && flags == ui::EF_NONE)) {
    // Make TrayBubbleView activatable as the following Widget::OnKeyEvent might
    // try to activate it.
    tray_bubble_view_->set_can_activate(true);

    tray_bubble_view_->GetWidget()->OnKeyEvent(event);

    if (event->handled())
      return;
  }

  // Always consumes key event not to pass it to other widgets. Calling
  // StopPropagation here to make this consistent with
  // MenuController::OnWillDispatchKeyEvent.
  event->StopPropagation();

  // To provide consistent behavior with a menu, process accelerator as a menu
  // is open if the event is not handled by the widget.
  ui::Accelerator accelerator(*event);
  ViewsDelegate::ProcessMenuAcceleratorResult result =
      ViewsDelegate::GetInstance()->ProcessAcceleratorWhileMenuShowing(
          accelerator);
  if (result == ViewsDelegate::ProcessMenuAcceleratorResult::CLOSE_MENU)
    tray_bubble_view_->CloseBubbleView();
}

TrayBubbleView::TrayBubbleView(const InitParams& init_params)
    : BubbleDialogDelegateView(init_params.anchor_view,
                               GetArrowAlignment(init_params.anchor_alignment)),
      params_(init_params),
      layout_(nullptr),
      delegate_(init_params.delegate),
      preferred_width_(init_params.min_width),
      bubble_border_(new BubbleBorder(
          arrow(),
          init_params.has_shadow ? BubbleBorder::NO_ASSETS
                                 : BubbleBorder::BIG_SHADOW,
          init_params.bg_color.value_or(gfx::kPlaceholderColor))),
      owned_bubble_border_(bubble_border_),
      is_gesture_dragging_(false),
      mouse_actively_entered_(false) {
  DCHECK(delegate_);
  DCHECK(params_.parent_window);
  // anchor_widget() is computed by BubbleDialogDelegateView().
  DCHECK(((init_params.anchor_mode != TrayBubbleView::AnchorMode::kView) ||
          anchor_widget()));
  bubble_border_->set_use_theme_background_color(!init_params.bg_color);
  if (init_params.corner_radius)
    bubble_border_->SetCornerRadius(init_params.corner_radius.value());
  set_parent_window(params_.parent_window);
  set_can_activate(false);
  set_notify_enter_exit_on_child(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  set_margins(gfx::Insets());
  SetPaintToLayer();

  bubble_content_mask_ = views::Painter::CreatePaintedLayer(
      views::Painter::CreateSolidRoundRectPainter(
          SK_ColorBLACK, bubble_border_->GetBorderCornerRadius()));

  auto layout = std::make_unique<BottomAlignedBoxLayout>(this);
  layout->SetDefaultFlex(1);
  layout_ = SetLayoutManager(std::move(layout));

  if (init_params.anchor_mode == AnchorMode::kRect) {
    SetAnchorView(nullptr);
    SetAnchorRect(init_params.anchor_rect);
  }
}

TrayBubbleView::~TrayBubbleView() {
  mouse_watcher_.reset();

  if (delegate_) {
    // Inform host items (models) that their views are being destroyed.
    delegate_->BubbleViewDestroyed();
  }
}

// static
bool TrayBubbleView::IsATrayBubbleOpen() {
  return g_current_tray_bubble_showing_count_ > 0;
}

void TrayBubbleView::InitializeAndShowBubble() {
  layer()->parent()->SetMaskLayer(bubble_content_mask_->layer());

  GetWidget()->Show();
  UpdateBubble();

  ++g_current_tray_bubble_showing_count_;

  // If TrayBubbleView cannot be activated and is shown by clicking on the
  // corresponding tray view, register pre target event handler to reroute key
  // events to the widget for activating the view or closing it.
  if (!CanActivate() && params_.show_by_click) {
    reroute_event_handler_ = std::make_unique<RerouteEventHandler>(
        this, GetWidget()->GetNativeWindow()->env());
  }
}

void TrayBubbleView::UpdateBubble() {
  if (GetWidget()) {
    SizeToContents();
    GetWidget()->GetRootView()->SchedulePaint();

    // When extra keyboard accessibility is enabled, focus the default item if
    // no item is focused.
    if (delegate_ && delegate_->ShouldEnableExtraKeyboardAccessibility())
      FocusDefaultIfNeeded();
  }
}

void TrayBubbleView::SetMaxHeight(int height) {
  params_.max_height = height;
  if (GetWidget())
    SizeToContents();
}

void TrayBubbleView::SetBottomPadding(int padding) {
  layout_->set_inside_border_insets(gfx::Insets(0, 0, padding, 0));
}

void TrayBubbleView::SetWidth(int width) {
  width = std::max(std::min(width, params_.max_width), params_.min_width);
  if (preferred_width_ == width)
    return;
  preferred_width_ = width;
  if (GetWidget())
    SizeToContents();
}

gfx::Insets TrayBubbleView::GetBorderInsets() const {
  return bubble_border_->GetInsets();
}

void TrayBubbleView::ResetDelegate() {
  reroute_event_handler_.reset();

  delegate_ = nullptr;
}

void TrayBubbleView::ChangeAnchorView(views::View* anchor_view) {
  DCHECK_EQ(AnchorMode::kView, params_.anchor_mode);
  BubbleDialogDelegateView::SetAnchorView(anchor_view);
}

void TrayBubbleView::ChangeAnchorRect(const gfx::Rect& rect) {
  DCHECK_EQ(AnchorMode::kRect, params_.anchor_mode);
  BubbleDialogDelegateView::SetAnchorRect(rect);
}

void TrayBubbleView::ChangeAnchorAlignment(
    TrayBubbleView::AnchorAlignment alignment) {
  SetArrow(GetArrowAlignment(alignment));
}

int TrayBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

ax::mojom::Role TrayBubbleView::GetAccessibleWindowRole() const {
  // We override the role because the base class sets it to alert dialog.
  // This would make screen readers announce the whole of the system tray
  // which is undesirable.
  return ax::mojom::Role::kDialog;
}

void TrayBubbleView::SizeToContents() {
  BubbleDialogDelegateView::SizeToContents();
  bubble_content_mask_->layer()->SetBounds(layer()->parent()->bounds());
}

void TrayBubbleView::OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                              Widget* bubble_widget) const {
  if (bubble_border_->shadow() == BubbleBorder::NO_ASSETS) {
    // Apply a WM-provided shadow (see ui/wm/core/).
    params->shadow_type = Widget::InitParams::SHADOW_TYPE_DROP;
    params->shadow_elevation = wm::kShadowElevationActiveWindow;
  }
}

void TrayBubbleView::OnWidgetClosing(Widget* widget) {
  // We no longer need to watch key events for activation if the widget is
  // closing.
  reroute_event_handler_.reset();

  BubbleDialogDelegateView::OnWidgetClosing(widget);
  --g_current_tray_bubble_showing_count_;
  DCHECK_GE(g_current_tray_bubble_showing_count_, 0)
      << "Closing " << widget->GetName();
}

void TrayBubbleView::OnWidgetActivationChanged(Widget* widget, bool active) {
  // We no longer need to watch key events for activation if the widget is
  // activated.
  reroute_event_handler_.reset();

  BubbleDialogDelegateView::OnWidgetActivationChanged(widget, active);
}

NonClientFrameView* TrayBubbleView::CreateNonClientFrameView(Widget* widget) {
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(
      BubbleDialogDelegateView::CreateNonClientFrameView(widget));
  frame->SetBubbleBorder(std::move(owned_bubble_border_));
  return frame;
}

bool TrayBubbleView::WidgetHasHitTestMask() const {
  return true;
}

void TrayBubbleView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
}

base::string16 TrayBubbleView::GetAccessibleWindowTitle() const {
  if (delegate_)
    return delegate_->GetAccessibleNameForBubble();
  else
    return base::string16();
}

gfx::Size TrayBubbleView::CalculatePreferredSize() const {
  DCHECK_LE(preferred_width_, params_.max_width);
  return gfx::Size(preferred_width_, GetHeightForWidth(preferred_width_));
}

int TrayBubbleView::GetHeightForWidth(int width) const {
  int height = GetInsets().height();
  width = std::max(width - GetInsets().width(), 0);
  for (int i = 0; i < child_count(); ++i) {
    const View* child = child_at(i);
    if (child->visible())
      height += child->GetHeightForWidth(width);
  }

  return (params_.max_height != 0) ? std::min(height, params_.max_height)
                                   : height;
}

void TrayBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_watcher_.reset();
  if (delegate_ && !(event.flags() & ui::EF_IS_SYNTHESIZED)) {
    // The user actively moved the mouse over the bubble; inform the delegate.
    delegate_->OnMouseEnteredView();
    mouse_actively_entered_ = true;
  } else {
    // The mouse was located over the bubble when it was first shown; use
    // MouseWatcher to wait for user interaction before signaling the delegate.
    mouse_watcher_ = std::make_unique<views::MouseWatcher>(
        std::make_unique<MouseMoveDetectorHost>(), this);
    mouse_watcher_->set_notify_on_exit_time(base::TimeDelta());
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
}

void TrayBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  // Disable any MouseWatcher waiting for user interaction inside the bubble.
  mouse_watcher_.reset();
  // Only notify the delegate on exit if it was notified on enter.
  if (delegate_ && mouse_actively_entered_)
    delegate_->OnMouseExitedView();
}

void TrayBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (delegate_ && CanActivate()) {
    node_data->role = ax::mojom::Role::kWindow;
    node_data->SetName(delegate_->GetAccessibleNameForBubble());
  }
}

void TrayBubbleView::OnGestureEvent(ui::GestureEvent* event) {
  if (delegate_)
    delegate_->ProcessGestureEventForBubble(event);
}

void TrayBubbleView::MouseMovedOutOfHost() {
  // The user moved the mouse that was over the bubble when it was first shown.
  if (delegate_)
    delegate_->OnMouseEnteredView();
  mouse_actively_entered_ = true;
  mouse_watcher_.reset();
}

void TrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void TrayBubbleView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    details.parent->SetPaintToLayer();
    details.parent->layer()->SetMasksToBounds(true);
  }
}

void TrayBubbleView::CloseBubbleView() {
  if (!delegate_)
    return;

  delegate_->HideBubble(this);
}

void TrayBubbleView::FocusDefaultIfNeeded() {
  views::FocusManager* manager = GetFocusManager();
  if (!manager || manager->GetFocusedView())
    return;

  views::View* view =
      manager->GetNextFocusableView(nullptr, nullptr, false, false);
  if (!view)
    return;

  // No need to explicitly activate the widget. View::RequestFocus will activate
  // it if necessary.
  set_can_activate(true);

  view->RequestFocus();
}

}  // namespace ash
