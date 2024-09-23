// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_overlay.h"

#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/system/toast/system_toast_view.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Duration of slide animation when overlay is shown or hidden.
constexpr int kSlideAnimationDurationMs = 100;

// Returns the work area bounds for the root window where new windows are added
// (including new toasts).
gfx::Rect GetUserWorkAreaBounds(aura::Window* window) {
  return WorkAreaInsets::ForWindow(window)->user_work_area_bounds();
}

// Offsets the bottom of bounds for toast to accommodate the hotseat, based on
// the current hotseat state
void AdjustWorkAreaBoundsForHotseatState(gfx::Rect& bounds,
                                         const HotseatWidget* hotseat_widget) {
  if (hotseat_widget->state() == HotseatState::kExtended) {
    bounds.set_height(bounds.height() - hotseat_widget->GetHotseatSize() -
                      ShelfConfig::Get()->hotseat_bottom_padding());
  }
  if (hotseat_widget->state() == HotseatState::kShownHomeLauncher)
    bounds.set_height(hotseat_widget->GetTargetBounds().y() - bounds.y());
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//  ToastDisplayObserver
class ToastOverlay::ToastDisplayObserver : public display::DisplayObserver {
 public:
  explicit ToastDisplayObserver(ToastOverlay* overlay) : overlay_(overlay) {}

  ToastDisplayObserver(const ToastDisplayObserver&) = delete;
  ToastDisplayObserver& operator=(const ToastDisplayObserver&) = delete;

  ~ToastDisplayObserver() override {}

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    overlay_->UpdateOverlayBounds();
  }

 private:
  const raw_ptr<ToastOverlay> overlay_;

  display::ScopedDisplayObserver display_observer_{this};
};

///////////////////////////////////////////////////////////////////////////////
//  ToastHoverObserver
class ToastOverlay::ToastHoverObserver : public ui::EventObserver {
 public:
  using HoverStateChangeCallback =
      base::RepeatingCallback<void(bool is_hovering)>;

  ToastHoverObserver(aura::Window* widget_window,
                     HoverStateChangeCallback on_hover_state_changed)
      : event_monitor_(views::EventMonitor::CreateWindowMonitor(
            /*event_observer=*/this,
            widget_window,
            {ui::EventType::kMouseEntered, ui::EventType::kMouseExited})),
        on_hover_state_changed_(std::move(on_hover_state_changed)) {}

  ToastHoverObserver(const ToastHoverObserver&) = delete;
  ToastHoverObserver& operator=(const ToastHoverObserver&) = delete;

  ~ToastHoverObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::EventType::kMouseEntered:
        on_hover_state_changed_.Run(/*is_hovering=*/true);
        break;
      case ui::EventType::kMouseExited:
        on_hover_state_changed_.Run(/*is_hovering=*/false);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  // While this `EventMonitor` object exists, this object will only look for
  // `ui::EventType::kMouseEntered` and `ui::EventType::kMouseExited` events
  // that occur in the `widget_window` indicated in the constructor.
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // This is run whenever the mouse enters or exits the observed window with a
  // parameter to indicate whether the window is being hovered.
  HoverStateChangeCallback on_hover_state_changed_;
};

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlay
ToastOverlay::ToastOverlay(Delegate* delegate,
                           const ToastData& toast_data,
                           aura::Window* root_window)
    : delegate_(delegate),
      text_(toast_data.text),
      dismiss_text_(toast_data.dismiss_text),
      overlay_widget_(new views::Widget),
      display_observer_(std::make_unique<ToastDisplayObserver>(this)),
      root_window_(root_window),
      dismiss_callback_(std::move(toast_data.dismiss_callback)) {
  // The provided callback is stored in the overlay's `dismiss_callback_`.
  overlay_view_ = std::make_unique<SystemToastView>(
      toast_data.text, toast_data.dismiss_text, /*dismiss_callback=*/
      base::BindRepeating(
          &ToastOverlay::OnButtonClicked,
          // Unretained is safe because `this` owns `overlay_view_`.
          base::Unretained(this)),
      toast_data.leading_icon);

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.name = "ToastOverlay";
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = true;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.bounds = CalculateOverlayBounds();
  params.parent =
      root_window_->GetChildById(kShellWindowId_SettingBubbleContainer);
  params.activatable = toast_data.activatable
                           ? views::Widget::InitParams::Activatable::kYes
                           : views::Widget::InitParams::Activatable::kNo;
  params.init_properties_container.SetProperty(kStayInOverviewOnActivationKey,
                                               true);
  overlay_widget_->Init(std::move(params));
  overlay_widget_->SetVisibilityChangedAnimationsEnabled(true);
  overlay_widget_->SetContentsView(overlay_view_.get());
  UpdateOverlayBounds();

  aura::Window* overlay_window = overlay_widget_->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      overlay_window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationDuration(
      overlay_window, base::Milliseconds(kSlideAnimationDurationMs));

  // Only toasts that expire should be able to persist on hover (i.e. toasts
  // with infinite duration persist regardless of hover).
  if (toast_data.persist_on_hover &&
      (toast_data.duration != ToastData::kInfiniteDuration)) {
    hover_observer_ = std::make_unique<ToastHoverObserver>(
        overlay_window, base::BindRepeating(&ToastOverlay::OnHoverStateChanged,
                                            base::Unretained(this)));
  }

  keyboard::KeyboardUIController::Get()->AddObserver(this);
  shelf_observation_.Observe(Shelf::ForWindow(overlay_window));

  if (features::AreSideAlignedToastsEnabled()) {
    auto* window_controller = RootWindowController::ForWindow(root_window_);
    if (window_controller->GetStatusAreaWidget()) {
      // `UnifiedSystemTray` is observed when side aligned toasts are enabled so
      // we can shift the toast baseline up when slider bubbles are visible.
      // The observation is safe on external monitor disconnect because
      // ToastManagerImpl deletes the ToastOverlay before the root window is
      // destroyed.
      scoped_unified_system_tray_observer_.Observe(
          window_controller->GetStatusAreaWidget()->unified_system_tray());
    }
  }
}

ToastOverlay::~ToastOverlay() {
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  overlay_widget_->Close();
}

void ToastOverlay::Show(bool visible) {
  if (overlay_widget_->GetLayer()->GetTargetVisibility() == visible)
    return;

  ui::LayerAnimator* animator = overlay_widget_->GetLayer()->GetAnimator();
  DCHECK(animator);

  base::TimeDelta original_duration = animator->GetTransitionDuration();
  ui::ScopedLayerAnimationSettings animation_settings(animator);
  // ScopedLayerAnimationSettings ctor changes the transition duration, so
  // change it back to the original value (should be zero).
  animation_settings.SetTransitionDuration(original_duration);

  animation_settings.AddObserver(this);

  if (visible) {
    overlay_widget_->ShowInactive();

    // Notify accessibility about the overlay.
    overlay_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, false);
  } else {
    overlay_widget_->Hide();
  }
}

void ToastOverlay::UpdateOverlayBounds() {
  overlay_widget_->SetBounds(CalculateOverlayBounds());
}

const std::u16string ToastOverlay::GetText() const {
  return text_;
}

bool ToastOverlay::RequestFocusOnActiveToastDismissButton() {
  overlay_view_->dismiss_button()->RequestFocus();
  return overlay_view_->dismiss_button()->HasFocus();
}

bool ToastOverlay::IsDismissButtonFocused() const {
  if (auto* dismiss_button = overlay_view_->dismiss_button()) {
    return dismiss_button->HasFocus();
  }
  return false;
}

void ToastOverlay::OnSliderBubbleHeightChanged() {
  // We only update toast baseline if they are aligned to the side.
  if (features::AreSideAlignedToastsEnabled()) {
    UpdateOverlayBounds();
  }
}

gfx::Rect ToastOverlay::CalculateOverlayBounds() {
  // If the native window has not been initialized, as in the first call, get
  // the default root window. Otherwise get the window for this `overlay_widget`
  // to handle multiple monitors properly.
  auto* window = overlay_widget_->IsNativeWidgetInitialized()
                     ? overlay_widget_->GetNativeWindow()
                     : root_window_.get();
  DCHECK(window);

  auto* window_controller = RootWindowController::ForWindow(window);
  auto* hotseat_widget = window_controller->shelf()->hotseat_widget();
  auto widget_size = overlay_view_->GetPreferredSize();

  gfx::Rect bounds = GetUserWorkAreaBounds(window);

  if (hotseat_widget) {
    AdjustWorkAreaBoundsForHotseatState(bounds, hotseat_widget);
  }

  if (features::AreSideAlignedToastsEnabled()) {
    // Toasts should always follow the status area and will usually show on the
    // bottom-right of the screen. They will show at the bottom-left whenever
    // the shelf is left-aligned or for RTL when the shelf is not right aligned.
    auto alignment = window_controller->shelf()->alignment();
    const int target_x =
        ((base::i18n::IsRTL() && alignment != ShelfAlignment::kRight) ||
         alignment == ShelfAlignment::kLeft)
            ? bounds.x() + ToastOverlay::kOffset
            : bounds.right() - widget_size.width() - ToastOverlay::kOffset;

    const int target_y = bounds.bottom() - widget_size.height() -
                         ToastOverlay::kOffset - CalculateSliderBubbleOffset();

    return gfx::Rect(gfx::Point(target_x, target_y), widget_size);
  }

  const int target_y =
      bounds.bottom() - widget_size.height() - ToastOverlay::kOffset;
  bounds.ClampToCenteredSize(widget_size);
  bounds.set_y(target_y);
  return bounds;
}

// Calculates the y offset used to shift side aligned toasts up whenever case a
// slider bubble is visible.
int ToastOverlay::CalculateSliderBubbleOffset() {
  // Slider bubble offset is only used for side aligned toasts.
  if (!features::AreSideAlignedToastsEnabled()) {
    return 0;
  }

  auto* window_controller = RootWindowController::ForWindow(root_window_);
  if (!window_controller) {
    return 0;
  }

  auto* status_area_widget = window_controller->GetStatusAreaWidget();
  if (!status_area_widget) {
    return 0;
  }

  auto* unified_system_tray = status_area_widget->unified_system_tray();
  if (!unified_system_tray) {
    return 0;
  }

  // If a slider bubble is visible, the toast baseline will be shifted
  // up by the slider bubble's height + a default spacing offset.
  if (unified_system_tray->IsSliderBubbleShown()) {
    auto* slider_view = unified_system_tray->GetSliderView();
    if (!slider_view) {
      return 0;
    }

    return slider_view->height() + ToastOverlay::kOffset;
  }

  return 0;
}

void ToastOverlay::OnButtonClicked() {
  if (dismiss_callback_) {
    dismiss_callback_.Run();
  }
  Show(/*visible=*/false);
}

void ToastOverlay::OnHoverStateChanged(bool is_hovering) {
  DCHECK(hover_observer_);

  if (!overlay_widget_->IsVisible())
    return;

  // We want to update the `delegate_` here in case this toast is also
  // displaying on other monitors.
  delegate_->OnToastHoverStateChanged(is_hovering);
}

void ToastOverlay::OnImplicitAnimationsScheduled() {}

void ToastOverlay::OnImplicitAnimationsCompleted() {
  if (!overlay_widget_->GetLayer()->GetTargetVisibility())
    delegate_->CloseToast();
}

void ToastOverlay::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  // TODO(crbug.com/40619022): Observe changes in user work area bounds
  // directly instead of listening for keyboard bounds changes.
  UpdateOverlayBounds();
}

void ToastOverlay::OnShelfWorkAreaInsetsChanged() {
  UpdateOverlayBounds();
}

void ToastOverlay::OnHotseatStateChanged(HotseatState old_state,
                                         HotseatState new_state) {
  UpdateOverlayBounds();
}

views::Widget* ToastOverlay::widget_for_testing() {
  return overlay_widget_.get();
}

views::LabelButton* ToastOverlay::dismiss_button_for_testing() {
  return overlay_view_->dismiss_button();
}

}  // namespace ash
