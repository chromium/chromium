// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_view.h"

#include <algorithm>
#include <memory>
#include <numeric>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/painter.h"
#include "ui/views/views_delegate.h"
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

BubbleBorder::Arrow GetArrowAlignment(ash::ShelfAlignment alignment) {
  // The tray bubble is in a corner. In this case, we want the arrow to be
  // flush with one side instead of centered on the bubble.
  switch (alignment) {
    case ash::ShelfAlignment::kBottom:
    case ash::ShelfAlignment::kBottomLocked:
      return base::i18n::IsRTL() ? BubbleBorder::BOTTOM_LEFT
                                 : BubbleBorder::BOTTOM_RIGHT;
    case ash::ShelfAlignment::kLeft:
      return BubbleBorder::LEFT_BOTTOM;
    case ash::ShelfAlignment::kRight:
      return BubbleBorder::RIGHT_BOTTOM;
  }
}

// Detects any mouse movement. This is needed to detect mouse movements by the
// user over the bubble if the bubble got created underneath the cursor.
class MouseMoveDetectorHost : public views::MouseWatcherHost {
 public:
  MouseMoveDetectorHost();

  MouseMoveDetectorHost(const MouseMoveDetectorHost&) = delete;
  MouseMoveDetectorHost& operator=(const MouseMoveDetectorHost&) = delete;

  ~MouseMoveDetectorHost() override;

  bool Contains(const gfx::Point& screen_point, EventType type) override;
};

MouseMoveDetectorHost::MouseMoveDetectorHost() {}

MouseMoveDetectorHost::~MouseMoveDetectorHost() {}

bool MouseMoveDetectorHost::Contains(const gfx::Point& screen_point,
                                     EventType type) {
  return false;
}

// Custom layout for the bubble-view. Does the default box-layout if there is
// enough height. Otherwise, makes sure the bottom rows are visible.
class BottomAlignedBoxLayout : public views::LayoutManagerBase {
 public:
  explicit BottomAlignedBoxLayout(TrayBubbleView* bubble_view)
      : bubble_view_(bubble_view) {
    box_layout_ = AddOwnedLayout(std::make_unique<views::BoxLayout>());
    box_layout_->SetDefaultFlex(1);
    box_layout_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  }

  BottomAlignedBoxLayout(const BottomAlignedBoxLayout&) = delete;
  BottomAlignedBoxLayout& operator=(const BottomAlignedBoxLayout&) = delete;

  ~BottomAlignedBoxLayout() override {}

  views::BoxLayout* box_layout() const { return box_layout_; }

 private:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    if (!size_bounds.is_fully_bounded() ||
        size_bounds.height() >= host_view()->GetPreferredSize({}).height() ||
        !bubble_view_->is_gesture_dragging()) {
      return box_layout_->GetProposedLayout(size_bounds, PassKey());
    }

    views::ProposedLayout layout;
    int consumed_height = 0;
    for (auto i = host_view()->children().rbegin();
         i != host_view()->children().rend() &&
         consumed_height < size_bounds.height().value();
         ++i) {
      auto& child = layout.child_layouts.emplace_back(*i, (*i)->GetVisible());
      if (!child.visible) {
        continue;
      }
      gfx::Size size = child.child_view->GetPreferredSize({});
      child.bounds = gfx::Rect(
          0, size_bounds.height().value() - consumed_height - size.height(),
          size_bounds.width().value(), size.height());
      consumed_height += size.height();
    }

    layout.host_size =
        gfx::Size(size_bounds.width().value(), size_bounds.height().value());

    return layout;
  }

  raw_ptr<views::BoxLayout> box_layout_;
  raw_ptr<TrayBubbleView> bubble_view_;
};

}  // namespace

TrayBubbleView::Delegate::Delegate() = default;

TrayBubbleView::Delegate::~Delegate() = default;

void TrayBubbleView::Delegate::BubbleViewDestroyed() {}

void TrayBubbleView::Delegate::OnMouseEnteredView() {}

void TrayBubbleView::Delegate::OnMouseExitedView() {}

std::u16string TrayBubbleView::Delegate::GetAccessibleNameForBubble() {
  return std::u16string();
}

bool TrayBubbleView::Delegate::ShouldEnableExtraKeyboardAccessibility() {
  return false;
}

base::WeakPtr<TrayBubbleView::Delegate> TrayBubbleView::Delegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::optional<AcceleratorAction>
TrayBubbleView::Delegate::GetAcceleratorAction() const {
  // TODO(crbug/1234891) Make this a pure virtual function so all
  // bubble delegates need to specify accelerator actions.
  return std::nullopt;
}

TrayBubbleView::InitParams::InitParams() = default;

TrayBubbleView::InitParams::~InitParams() = default;

TrayBubbleView::InitParams::InitParams(const InitParams& other) = default;

TrayBubbleView::RerouteEventHandler::RerouteEventHandler(
    TrayBubbleView* tray_bubble_view)
    : tray_bubble_view_(tray_bubble_view) {
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
}

TrayBubbleView::RerouteEventHandler::~RerouteEventHandler() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
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
  int flags = event->flags() &
              (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
               ui::EF_COMMAND_DOWN | ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN);
  if ((key_code == ui::VKEY_TAB && flags == ui::EF_NONE) ||
      (key_code == ui::VKEY_TAB && flags == ui::EF_SHIFT_DOWN) ||
      (key_code == ui::VKEY_ESCAPE && flags == ui::EF_NONE) ||
      // Do not dismiss the bubble immediately when a user triggers a feedback
      // report; if they're reporting an issue with the bubble we want the
      // screenshot to contain it.
      (key_code == ui::VKEY_I &&
       flags == (ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN))) {
    // Make TrayBubbleView activatable as the following Widget::OnKeyEvent might
    // try to activate it.
    tray_bubble_view_->SetCanActivate(true);

    tray_bubble_view_->GetWidget()->OnKeyEvent(event);

    if (event->handled()) {
      return;
    }
  }

  // For shelf pod bubble that is anchored to the shelf corner, popup
  // notifications and the bubble can be shown at the same time. If the keyboard
  // happens inside the popup collection, we need to let the keyboard event pass
  // there to make sure notification popups can receive keyboard events.
  if (target &&
      tray_bubble_view_->GetBubbleType() == TrayBubbleType::kShelfPodBubble &&
      tray_bubble_view_->IsAnchoredToShelfCorner()) {
    auto* popup_collection = RootWindowController::ForWindow(target)
                                 ->shelf()
                                 ->GetStatusAreaWidget()
                                 ->notification_center_tray()
                                 ->popup_collection();

    if (popup_collection->popup_collection_bounds().Contains(
            target->GetActualBoundsInScreen())) {
      return;
    }
  }

  // Always consumes key event not to pass it to other widgets. Calling
  // StopPropagation here to make this consistent with
  // MenuController::OnWillDispatchKeyEvent.
  event->StopPropagation();

  // To provide consistent behavior with a menu, process accelerator as a menu
  // is open if the event is not handled by the widget.
  ui::Accelerator accelerator(*event);

  // crbug/1212857 Immediately close the bubble if the accelerator action
  // is going to do it and do not process the accelerator. If the accelerator
  // action is executed asynchronously it will execute after the bubble has
  // already been closed and it will result in the accelerator action reopening
  // the bubble.
  if (tray_bubble_view_->GetAcceleratorAction().has_value() &&
      AcceleratorControllerImpl::Get()->DoesAcceleratorMatchAction(
          ui::Accelerator(*event),
          tray_bubble_view_->GetAcceleratorAction().value())) {
    tray_bubble_view_->CloseBubbleView();
  } else {
    ViewsDelegate::GetInstance()->ProcessAcceleratorWhileMenuShowing(
        accelerator);
  }
}

void TrayBubbleView::RerouteEventHandler::OnEvent(ui::Event* event) {
  if (!tray_bubble_view_->set_can_activate_on_click_or_tap_) {
    EventHandler::OnEvent(event);
    return;
  }

  if (event->type() == ui::EventType::kMousePressed ||
      event->type() == ui::EventType::kTouchPressed ||
      event->type() == ui::EventType::kGestureTap) {
    tray_bubble_view_->SetCanActivate(true);
  }

  EventHandler::OnEvent(event);
}

TrayBubbleView::TrayBubbleView(const InitParams& init_params)
    : BubbleDialogDelegateView(init_params.anchor_view,
                               GetArrowAlignment(init_params.shelf_alignment)),
      params_(init_params),
      layout_(nullptr),
      delegate_(init_params.delegate),
      preferred_width_(init_params.preferred_width),
      is_gesture_dragging_(false),
      set_can_activate_on_click_or_tap_(
          init_params.set_can_activate_on_click_or_tap),
      mouse_actively_entered_(false) {
  // We set the dialog role because views::BubbleDialogDelegate defaults this to
  // an alert dialog. This would make screen readers announce the whole of the
  // system tray which is undesirable.
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
  // We force to create contents background since the bubble border background
  // is not shown in this view.
  set_force_create_contents_background(true);
  // Bubbles that use transparent colors should not paint their ClientViews to a
  // layer as doing so could result in visual artifacts.
  SetPaintClientToLayer(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  DCHECK(delegate_);
  DCHECK(params_.parent_window);
  // anchor_widget() is computed by BubbleDialogDelegateView().
  DCHECK((init_params.anchor_mode != TrayBubbleView::AnchorMode::kView) ||
         anchor_widget());
  set_parent_window(params_.parent_window);
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  SetCanActivate(controller->spoken_feedback().enabled() ||
                 controller->dictation().enabled());
  SetNotifyEnterExitOnChild(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  set_margins(init_params.margin.has_value() ? init_params.margin.value()
                                             : gfx::Insets());

  if (init_params.translucent) {
    // TODO(crbug.com/40832096): In the dark light mode feature, remove layer
    // creation in children views of this view to improve performance.
    SetPaintToLayer(ui::LAYER_TEXTURED);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF{static_cast<float>(params_.corner_radius)});
    layer()->SetIsFastRoundedCorner(true);
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  } else {
    // Create a layer so that the layer for FocusRing stays in this view's
    // layer. Without it, the layer for FocusRing goes above the
    // NativeViewHost and may steal events.
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);

    if (!init_params.transparent) {
      SetPaintToLayer();
      layer()->SetRoundedCornerRadius(
          gfx::RoundedCornersF{static_cast<float>(params_.corner_radius)});
    }
  }

  if (init_params.transparent) {
    set_color(SK_ColorTRANSPARENT);
  }

  if (params_.has_shadow) {
    // Draws shadow on texture layer for large corner radius bubbles.
    if (params_.has_large_corner_radius) {
      shadow_ = SystemShadow::CreateShadowOnTextureLayer(params_.shadow_type);
      shadow_->SetRoundedCornerRadius(params_.corner_radius);
    } else if (features::IsSystemTrayShadowEnabled()) {
      shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
          this, params_.shadow_type);
      shadow_->SetRoundedCornerRadius(params_.corner_radius);
    }
  }

  auto layout = std::make_unique<BottomAlignedBoxLayout>(this);
  layout_ = SetLayoutManager(std::move(layout))->box_layout();

  if (init_params.anchor_mode == AnchorMode::kRect) {
    SetAnchorView(nullptr);
    SetAnchorRect(init_params.anchor_rect);
  } else {
    SetAnchorView(init_params.anchor_view);
    SetAnchorRect(gfx::Rect());
  }

  message_center::MessageCenter::Get()->AddObserver(this);
  Shell::Get()->display_manager()->AddDisplayObserver(this);

  GetViewAccessibility().SetRole(ax::mojom::Role::kWindow);
  UpdateAccessibleIgnoredState();
}

TrayBubbleView::~TrayBubbleView() {
  Shell::Get()->display_manager()->RemoveDisplayObserver(this);
  message_center::MessageCenter::Get()->RemoveObserver(this);

  mouse_watcher_.reset();

  if (delegate_) {
    // Inform host items (models) that their views are being destroyed.
    delegate_->BubbleViewDestroyed();
  }
}

void TrayBubbleView::InitializeAndShowBubble() {
  GetWidget()->Show();
  UpdateBubble();
  UpdateAccessibleIgnoredState();

  // Manually sets the shadow position since `CreateShadowOnTextureLayer` only
  // constructs the shadow but doesn't deal with shadow positioning.
  if (params_.has_shadow && params_.has_large_corner_radius) {
    AddLayerToRegion(shadow_->GetLayer(), views::LayerRegion::kBelow);
    shadow_->SetContentBounds(layer()->bounds());
  }

  // Register pre target event handler to reroute key
  // events to the widget for activating the view or closing it.
  if (!CanActivate() && params_.reroute_event_handler) {
    reroute_event_handler_ = std::make_unique<RerouteEventHandler>(this);
  }

  if (IsAnchoredToStatusArea()) {
    NotifyTrayBubbleOpen();
  }
}

void TrayBubbleView::UpdateBubble() {
  if (GetWidget()) {
    SizeToContents();
    GetWidget()->GetRootView()->SchedulePaint();
  }
}

void TrayBubbleView::SetMaxHeight(int height) {
  params_.max_height = height;
  if (GetWidget()) {
    SizeToContents();
  }
}

void TrayBubbleView::SetBottomPadding(int padding) {
  layout_->set_inside_border_insets(gfx::Insets::TLBR(0, 0, padding, 0));
}

void TrayBubbleView::SetPreferredWidth(int width) {
  if (preferred_width_ == width) {
    return;
  }
  preferred_width_ = width;
  if (GetWidget()) {
    SizeToContents();
  }
}

gfx::Insets TrayBubbleView::GetBorderInsets() const {
  auto* bubble_border = GetBubbleFrameView()->bubble_border();
  return bubble_border ? bubble_border->GetInsets() : gfx::Insets();
}

std::optional<AcceleratorAction> TrayBubbleView::GetAcceleratorAction() const {
  return delegate_->GetAcceleratorAction();
}

void TrayBubbleView::ResetDelegate() {
  reroute_event_handler_.reset();

  delegate_ = nullptr;
  UpdateAccessibleIgnoredState();
}

void TrayBubbleView::ChangeAnchorView(views::View* anchor_view) {
  DCHECK_EQ(AnchorMode::kView, params_.anchor_mode);
  BubbleDialogDelegateView::SetAnchorView(anchor_view);
}

void TrayBubbleView::ChangeAnchorRect(const gfx::Rect& rect) {
  DCHECK_EQ(AnchorMode::kRect, params_.anchor_mode);
  BubbleDialogDelegateView::SetAnchorRect(rect);
}

void TrayBubbleView::ChangeAnchorAlignment(ShelfAlignment alignment) {
  SetArrow(GetArrowAlignment(alignment));
}

bool TrayBubbleView::IsAnchoredToStatusArea() const {
  return params_.is_anchored_to_status_area;
}

bool TrayBubbleView::IsAnchoredToShelfCorner() const {
  return params_.anchor_to_shelf_corner;
}

void TrayBubbleView::StopReroutingEvents() {
  reroute_event_handler_.reset();
}

TrayBubbleView::TrayBubbleType TrayBubbleView::GetBubbleType() const {
  return params_.type;
}

void TrayBubbleView::OnWidgetClosing(Widget* widget) {
  // We no longer need to watch key events for activation if the widget is
  // closing.
  reroute_event_handler_.reset();

  if (IsAnchoredToStatusArea()) {
    NotifyTrayBubbleClosed();
  }

  BubbleDialogDelegateView::OnWidgetClosing(widget);
}

void TrayBubbleView::OnWidgetActivationChanged(Widget* widget, bool active) {
  // We no longer need to watch key events for activation if the widget is
  // activated.
  reroute_event_handler_.reset();

  BubbleDialogDelegateView::OnWidgetActivationChanged(widget, active);
}

void TrayBubbleView::OnWidgetBoundsChanged(views::Widget* widget,
                                           const gfx::Rect& bounds) {
  Shell::Get()->system_tray_notifier()->NotifyTrayBubbleBoundsChanged(this);
}

ui::LayerType TrayBubbleView::GetLayerType() const {
  if (params_.translucent) {
    return ui::LAYER_NOT_DRAWN;
  }
  return ui::LAYER_TEXTURED;
}

std::unique_ptr<NonClientFrameView> TrayBubbleView::CreateNonClientFrameView(
    Widget* widget) {
  // Create the customized bubble border.
  std::unique_ptr<BubbleBorder> bubble_border =
      std::make_unique<BubbleBorder>(arrow(), BubbleBorder::NO_SHADOW);
  if (params_.corner_radius) {
    bubble_border->SetCornerRadius(params_.corner_radius);
  }
  bubble_border->set_avoid_shadow_overlap(true);
  if (params_.insets.has_value()) {
    bubble_border->set_insets(params_.insets.value());
  }

  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  auto* frame_ptr = static_cast<views::BubbleFrameView*>(frame.get());
  frame_ptr->SetBubbleBorder(std::move(bubble_border));
  if (params_.anchor_mode == AnchorMode::kView) {
    frame_ptr->set_use_anchor_window_bounds(false);
  }

  return frame;
}

bool TrayBubbleView::WidgetHasHitTestMask() const {
  return true;
}

void TrayBubbleView::GetWidgetHitTestMask(SkPath* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
}

std::u16string TrayBubbleView::GetAccessibleWindowTitle() const {
  if (delegate_) {
    return delegate_->GetAccessibleNameForBubble();
  } else {
    return std::u16string();
  }
}

void TrayBubbleView::AddedToWidget() {
  // If the view has a shadow on texture layer, should make it observe widget
  // theme change to update its colors. The function is called here since we
  // should guarantee that `GetWidget()` returns non-nullptr.
  if (params_.has_shadow && params_.has_large_corner_radius) {
    shadow_->ObserveColorProviderSource(GetWidget());
  }
}

gfx::Size TrayBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int width = std::max(preferred_width_ - GetInsets().width(), 0);
  const int height = std::transform_reduce(
      children().cbegin(), children().cend(), GetInsets().height(),
      std::plus<>(), [width](const views::View* child) {
        return child->GetVisible() ? child->GetHeightForWidth(width) : 0;
      });
  if (params_.use_fixed_height) {
    return gfx::Size(preferred_width_,
                     (params_.max_height != 0) ? params_.max_height : height);
  }
  return gfx::Size(preferred_width_, (params_.max_height != 0)
                                         ? std::min(height, params_.max_height)
                                         : height);
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
  if (delegate_ && mouse_actively_entered_) {
    delegate_->OnMouseExitedView();
  }
}

void TrayBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (delegate_ && CanActivate()) {
    node_data->SetNameChecked(delegate_->GetAccessibleNameForBubble());
  }
}

void TrayBubbleView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  if (params_.transparent) {
    return;
  }

  SetBorder(std::make_unique<views::HighlightBorder>(
      params_.corner_radius,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderOnShadow
          : views::HighlightBorder::Type::kHighlightBorder1));
  set_color(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemBaseElevated));
}

void TrayBubbleView::MouseMovedOutOfHost() {
  // The user moved the mouse that was over the bubble when it was first shown.
  if (delegate_) {
    delegate_->OnMouseEnteredView();
  }
  mouse_actively_entered_ = true;
  mouse_watcher_.reset();
}

bool TrayBubbleView::ShouldUseFixedHeight() const {
  return params_.use_fixed_height;
}

void TrayBubbleView::SetShouldUseFixedHeight(bool shoud_use_fixed_height) {
  params_.use_fixed_height = shoud_use_fixed_height;
}

void TrayBubbleView::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  // Stack bubble view at the bottom when a new popup is displayed so popup
  // collection can be shown in the front.
  if (source == message_center::DISPLAY_SOURCE_POPUP) {
    aura::Window* tray_window = GetWidget()->GetNativeView();
    tray_window->parent()->StackChildAtBottom(tray_window);
  }
}

void TrayBubbleView::OnDisplayTabletStateChanged(display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    // Do nothing when the tablet state is still in the process of transition.
    return;
  }

  aura::Window* tray_window = GetWidget()->GetNativeView();
  Shelf* shelf = Shelf::ForWindow(tray_window);
  if (params_.anchor_mode == AnchorMode::kRect) {
    SetAnchorRect(shelf->GetSystemTrayAnchorRect());
  }

  // The shelf alignment may change when transitioning between tablet and
  // clamshell mode. In those cases, we need to update the shelf alighment.
  if (ash::ShelfAlignment current_alignment = shelf->alignment();
      current_alignment != params_.shelf_alignment) {
    params_.shelf_alignment = current_alignment;
    ChangeAnchorAlignment(current_alignment);
  }

  SetBubbleBorderInsets(GetTrayBubbleInsets(tray_window));
  UpdateBubble();
}

void TrayBubbleView::NotifyTrayBubbleOpen() {
  DCHECK(IsAnchoredToStatusArea());

  if (GetBubbleType() == TrayBubbleType::kShelfPodBubble) {
    StatusAreaWidget::ForWindow(GetWidget()->GetNativeView())
        ->SetOpenShelfPodBubble(this);
  }

  Shell::Get()
      ->system_tray_notifier()
      ->NotifyStatusAreaAnchoredBubbleVisibilityChanged(/*tray_bubble=*/this,
                                                        /*visible=*/true);
}

void TrayBubbleView::NotifyTrayBubbleClosed() {
  DCHECK(IsAnchoredToStatusArea());

  auto* status_area = StatusAreaWidget::ForWindow(GetWidget()->GetNativeView());

  // `TrayBubbleView` may live longer than `StatusAreaWidget`.
  if (status_area && GetBubbleType() == TrayBubbleType::kShelfPodBubble) {
    status_area->SetOpenShelfPodBubble(nullptr);
  }

  Shell::Get()
      ->system_tray_notifier()
      ->NotifyStatusAreaAnchoredBubbleVisibilityChanged(/*tray_bubble=*/this,
                                                        /*visible=*/false);
}

void TrayBubbleView::CloseBubbleView() {
  if (!delegate_) {
    return;
  }

  delegate_->HideBubble(this);
}

void TrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void TrayBubbleView::SetBubbleBorderInsets(gfx::Insets insets) {
  if (GetBubbleFrameView()->bubble_border()) {
    GetBubbleFrameView()->bubble_border()->set_insets(insets);
  }
}

void TrayBubbleView::UpdateAccessibleIgnoredState() {
  GetViewAccessibility().SetIsIgnored(!delegate_ || !CanActivate());
}

BEGIN_METADATA(TrayBubbleView)
END_METADATA

}  // namespace ash
