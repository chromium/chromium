// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma clang optimize off

#include "chrome/browser/glic/glic_window_controller.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/glic/glic_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget_observer.h"

namespace {
// Default value for how close the corner of glic has to be from a browser's
// glic button to snap.
constexpr static int kSnapDistanceThreshold = 50;
constexpr static int kUnsnapDistanceThreshold = kSnapDistanceThreshold + 10;

constexpr static int kWidgetWidth = 400;
constexpr static int kWidgetHeight = 800;
constexpr static int kWidgetTopBarHeight = 80;

// Helper class for observing mouse and key events from native window.
class WindowEventObserver : public ui::EventObserver {
 public:
  explicit WindowEventObserver(
      glic::GlicWindowController* glic_window_controller,
      glic::GlicView* glic_view)
      : glic_window_controller_(glic_window_controller), glic_view_(glic_view) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, glic_view->GetWidget()->GetNativeWindow(),
        {ui::EventType::kMousePressed, ui::EventType::kMouseReleased,
         ui::EventType::kMouseDragged});
  }

  WindowEventObserver(const WindowEventObserver&) = delete;
  WindowEventObserver& operator=(const WindowEventObserver&) = delete;
  ~WindowEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
    if (!event.IsMouseEvent()) {
      return;
    }

    gfx::Point mouse_location = event_monitor_->GetLastMouseLocation();
    views::View::ConvertPointFromScreen(glic_view_, &mouse_location);
    if (event.type() == ui::EventType::kMousePressed &&
        glic_view_->IsPointWithinDraggableArea(mouse_location)) {
      mouse_down_in_draggable_area_ = true;
    }

    if (event.type() == ui::EventType::kMouseReleased ||
        event.type() == ui::EventType::kMouseExited) {
      mouse_down_in_draggable_area_ = false;
    }

    // Window should only be dragged if a corresponding mouse drag event was
    // initiated in the draggable area.
    if (mouse_down_in_draggable_area_ &&
        event.type() == ui::EventType::kMouseDragged) {
      glic_window_controller_->HandleWindowDragWithOffset(
          mouse_location.OffsetFromOrigin());
    }
  }

  raw_ptr<glic::GlicWindowController> glic_window_controller_;
  raw_ptr<glic::GlicView> glic_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // Tracks whether the mouse is pressed and was initially within a draggable
  // area of the window.
  bool mouse_down_in_draggable_area_;
};

}  // namespace

namespace glic {

GlicWindowController::GlicWindowController(Profile* profile)
    : profile_(profile) {}

GlicWindowController::~GlicWindowController() = default;

void GlicWindowController::Show(views::View* glic_button_view) {
  // TODO(crbug.com/379943498): If a glic window already exists, handle showing
  // by bringing to front or activating.
  if (widget_) {
    return;
  }

  int padding;
  gfx::Point top_right_point;
  bool should_attach_to_browser = false;

  if (!glic_button_view) {
    // Right now this only detects whether the glic widget is summoned from the
    // OS entrypoint and positions itself detached from the browser.
    // TODO(crbug.com/384061064): Add more logic for when the glic window should
    // show up in a detached state.
    top_right_point = GetTopRightPositionForDetachedWindow();
    padding = 50;
  } else {
    // If summoned from the tab strip button. This will always show up attached
    // because it is tied to a views::View object within the current browser
    // window.
    top_right_point = GetTopRightPositionForAttachedWindow(glic_button_view);
    padding = GetLayoutConstant(TAB_STRIP_PADDING);
    should_attach_to_browser = true;
  }

  widget_ = glic::GlicView::CreateWidget(
      profile_, {top_right_point.x() - kWidgetWidth - padding,
                 top_right_point.y() + padding, kWidgetWidth, kWidgetHeight});
  widget_->AddObserver(this);
  glic_widget_observer_ =
      std::make_unique<GlicWidgetObserver>(this, widget_.get());
  widget_->Show();

  if (should_attach_to_browser) {
    views::Widget* glic_button_widget = glic_button_view->GetWidget();
    Browser* browser =
        chrome::FindBrowserWithWindow(glic_button_widget->GetNativeWindow());
    AttachToBrowser(browser, glic_button_widget);
  } else {
    MaybeCreateHolderWindowAndReparent();
  }

  window_event_observer_ =
      std::make_unique<WindowEventObserver>(this, GetGlicView());
  // Set the draggable area to the top bar of the window, by default.
  GetGlicView()->SetDraggableAreas({{0, 0, kWidgetWidth, kWidgetTopBarHeight}});
}

GlicView* GlicWindowController::GetGlicView() {
  if (!widget_) {
    return nullptr;
  }
  return GlicView::FromWidget(*widget_);
}

gfx::Point GlicWindowController::GetTopRightPositionForAttachedWindow(
    views::View* glic_button_view) {
  // Initial position determined by glic button bounds. Returns the top right
  // point of the button.
  gfx::Point top_right_bounds =
      glic_button_view->GetBoundsInScreen().top_right();

  return top_right_bounds;
}

gfx::Point GlicWindowController::GetTopRightPositionForDetachedWindow() {
  // Position determined by screen bounds. Returns the top right point of the
  // screen.
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Point top_right_bounds =
      screen->GetPrimaryDisplay().work_area().top_right();

  return top_right_bounds;
}

void GlicWindowController::AttachToBrowser(Browser* browser,
                                           views::Widget* widget) {
  // Makes the glic widget a child view of the given widget's browser.
  if (widget && widget_ && browser) {
    // Add observer to new parent.
    pinned_target_widget_observer_.SetPinnedTargetWidget(widget);
    pinned_browser_ = browser->AsWeakPtr();
    views::Widget::ReparentNativeView(widget_->GetNativeView(),
                                      widget->GetNativeView());
    NotifyIfPanelStateChanged();

    widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  }
}

bool GlicWindowController::Resize(const gfx::Size& size) {
  if (!widget_) {
    return false;
  }

  widget_->SetSize(size);
  GetGlicView()->web_view()->SetSize(size);
  return true;
}

gfx::Size GlicWindowController::GetSize() {
  if (!widget_) {
    return gfx::Size();
  }

  return widget_->GetSize();
}

void GlicWindowController::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  GlicView* glic_view = GetGlicView();
  if (!glic_view) {
    return;
  }

  glic_view->SetDraggableAreas(draggable_areas);
}

void GlicWindowController::Close() {
  if (!widget_) {
    return;
  }
  widget_->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  glic_widget_observer_.reset();
  window_event_observer_.reset();
  widget_.reset();
  NotifyIfPanelStateChanged();
}

void GlicWindowController::HandleWindowDragWithOffset(
    gfx::Vector2d mouse_offset) {
  // This code isn't set up to handle nested run loops. Nested run loops will
  // lead to crashes.
  if (!in_move_loop_) {
    // Prepare to start a new drag of the glic window using holder_widget_ as
    // the new parent.
    MaybeCreateHolderWindowAndReparent();
    in_move_loop_ = true;
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    // On Windows, RunMoveLoop doesn't work with `holder_widget_` because
    // `widget_`'s hwnd received the mouse down and move messages, not
    // `holder_widget_`'s hwnd. Calling RunMoveLoop on `holder_widget_` exits
    // the loop immediately.
    views::Widget* run_loop_widget =
#if BUILDFLAG(IS_WIN)
        widget_.get();
#else
        holder_widget_.get();
#endif
    run_loop_widget->RunMoveLoop(
        mouse_offset, move_loop_source,
        views::Widget::MoveLoopEscapeBehavior::kDontHide);
    in_move_loop_ = false;
    // Look for pins
    HandleBrowserPinning(widget_.get());
  } else {
    // while in move loop, find browser pin targets close to the holder widget
    // to animate glic to for a magnetic effect.
    HandleBrowserPinning(holder_widget_.get());
  }
}

void GlicWindowController::HandleBrowserPinning(views::Widget* widget) {
  // Loops through all browsers in activation order with the latest accessed
  // browser first.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    views::Widget* window_widget =
        browser->window()->AsBrowserView()->GetWidget();
    // Skips if:
    // - incognito
    // - not visible
    // - is a glic-owned widget
    // - is a different profile (uses browser context to check)
    if (browser->profile()->IsOffTheRecord() ||
        !browser->window()->IsVisible() || window_widget == widget_.get() ||
        window_widget == holder_widget_.get() ||
        browser->GetWebView()->GetBrowserContext() !=
            GetGlicView()->web_view()->GetBrowserContext()) {
      continue;
    }
    auto* tab_strip_region_view =
        browser->window()->AsBrowserView()->tab_strip_region_view();
    if (!tab_strip_region_view || !tab_strip_region_view->GetGlicButton()) {
      continue;
    }
    gfx::Rect glic_button_rect =
        tab_strip_region_view->GetGlicButton()->GetBoundsInScreen();

    float corner_distance = (glic_button_rect.CenterPoint() -
                             widget->GetWindowBoundsInScreen().top_right())
                                .Length();
    // While glic window is actively being dragged, simulate a visual effect of
    // being pulled to and away from the browser by updating its parent.
    if (in_move_loop_) {
      if (corner_distance > kUnsnapDistanceThreshold &&
          widget_->parent() == window_widget) {
        // Pull the glic window away from the browser window anchor point when a
        // drag is far enough away
        views::Widget::ReparentNativeView(widget_->GetNativeView(),
                                          holder_widget_->GetNativeView());
        // TODO(https://crbug.com/384792988): Should use GlicView->
        // AnimateFrameBounds here but is currently having a glitchy animation.
      } else if (corner_distance < kSnapDistanceThreshold &&
                 widget_->parent() != window_widget) {
        // Temporarily attach the window for the visual effect of being
        // magnetised to the snapping point
        MoveToBrowserPinTarget(browser, true);
        views::Widget::ReparentNativeView(widget_->GetNativeView(),
                                          window_widget->GetNativeView());
      }
    } else {
      // If there is no active drag (i.e. the previous drag has ended)
      // then determine whether to snap the glic window to the browser,
      // or to detach it from the browser.
      if (corner_distance < kSnapDistanceThreshold) {
        MoveToBrowserPinTarget(browser, true);
        // Close holder window if existing
        if (holder_widget_) {
          holder_widget_->CloseWithReason(
              views::Widget::ClosedReason::kLostFocus);
          holder_widget_.reset();
        }
        AttachToBrowser(browser, window_widget);
      } else if (widget_->parent() == window_widget) {
        // If farther than the snapping threshold from the current parent
        // widget, open a blank holder window to reparent to
        MaybeCreateHolderWindowAndReparent();
      }
    }
  }
}

void GlicWindowController::MoveToBrowserPinTarget(Browser* browser,
                                                  bool animate) {
  if (!widget_) {
    return;
  }
  pinned_browser_ = browser->AsWeakPtr();
  gfx::Rect glic_rect = widget_->GetWindowBoundsInScreen();
  // TODO(andreaxg): Fix exact snap location.
  gfx::Rect glic_button_rect = browser->window()
                                   ->AsBrowserView()
                                   ->tab_strip_region_view()
                                   ->GetGlicButton()
                                   ->GetBoundsInScreen();
  gfx::Point top_right = glic_button_rect.top_right();
  int tab_strip_padding = GetLayoutConstant(TAB_STRIP_PADDING);
  glic_rect.set_x(top_right.x() - glic_rect.width() - tab_strip_padding);
  glic_rect.set_y(top_right.y() + tab_strip_padding);
  // Avoid conversions between pixels and DIP on non 1.0 scale factor displays
  // changing widget width and height.
  glic_rect.set_width(kWidgetWidth);
  glic_rect.set_height(kWidgetHeight);
  if (animate) {
    GetGlicView()->AnimateFrameBounds(glic_rect);
  } else {
    widget_->SetBounds(glic_rect);
  }
  NotifyIfPanelStateChanged();
}

void GlicWindowController::MaybeCreateHolderWindowAndReparent() {
  pinned_target_widget_observer_.SetPinnedTargetWidget(nullptr);
  pinned_browser_ = nullptr;
  if (!holder_widget_) {
    holder_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = false;
    // Widget name is specified for debug purposes.
    params.name = "HolderWindow";
    params.bounds = widget_->GetWindowBoundsInScreen();
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    holder_widget_->Init(std::move(params));
    holder_widget_->ShowInactive();
  }
  views::Widget::ReparentNativeView(widget_->GetNativeView(),
                                    holder_widget_->GetNativeView());
  NotifyIfPanelStateChanged();

  // When the glic window is in a detached state, elevate it's z-order to be
  // always on top.
  widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
}

void GlicWindowController::AddStateObserver(StateObserver* observer) {
  state_observers_.AddObserver(observer);
}

void GlicWindowController::RemoveStateObserver(StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void GlicWindowController::NotifyIfPanelStateChanged() {
  auto new_state = ComputePanelState();
  if (new_state != panel_state_) {
    panel_state_ = new_state;
    state_observers_.Notify(&StateObserver::PanelStateChanged, panel_state_);
  }
}

mojom::PanelState GlicWindowController::ComputePanelState() const {
  mojom::PanelState panel_state;
  if (!widget_visible_) {
    panel_state.kind = mojom::PanelState_Kind::kHidden;
  } else if (pinned_browser_) {
    panel_state.kind = mojom::PanelState_Kind::kDocked;
    panel_state.window_id = pinned_browser_->session_id().id();
  } else {
    panel_state.kind = mojom::PanelState_Kind::kFloating;
  }
  return panel_state;
}

void GlicWindowController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  // Store visibility locally because calling widget_->IsVisible() at this point
  // returns the old value.
  widget_visible_ = visible;
  NotifyIfPanelStateChanged();
}

bool GlicWindowController::IsActive() {
  return widget_ && widget_->IsActive();
}

bool GlicWindowController::HasWindow() const {
  return !!widget_;
}

base::CallbackListSubscription
GlicWindowController::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  return window_activation_callback_list_.Add(std::move(callback));
}

void GlicWindowController::NotifyWindowActivationChanged(bool active) {
  window_activation_callback_list_.Notify(active);
}

base::WeakPtr<GlicWindowController> GlicWindowController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

///////////////////////////////////////////////////////////////////////////////
// PinnedTargetWidgetObserver implementations:
GlicWindowController::PinnedTargetWidgetObserver::PinnedTargetWidgetObserver(
    glic::GlicWindowController* glic_window_controller)
    : glic_window_controller_(glic_window_controller) {}

GlicWindowController::PinnedTargetWidgetObserver::
    ~PinnedTargetWidgetObserver() {
  SetPinnedTargetWidget(nullptr);
}

void GlicWindowController::PinnedTargetWidgetObserver::SetPinnedTargetWidget(
    views::Widget* widget) {
  if (widget == pinned_target_widget_) {
    return;
  }
  if (pinned_target_widget_ && pinned_target_widget_->HasObserver(this)) {
    pinned_target_widget_->RemoveObserver(this);
    pinned_target_widget_ = nullptr;
  }
  if (widget && !widget->HasObserver(this)) {
    widget->AddObserver(this);
    pinned_target_widget_ = widget;
  }
}

void GlicWindowController::PinnedTargetWidgetObserver::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  glic_window_controller_->MoveToBrowserPinTarget(
      chrome::FindBrowserWithWindow(widget->GetNativeWindow()), false);
}

void GlicWindowController::PinnedTargetWidgetObserver::OnWidgetDestroying(
    views::Widget* widget) {
  SetPinnedTargetWidget(nullptr);
}

///////////////////////////////////////////////////////////////////////////////
// GlicWidgetObserver implementations:
GlicWindowController::GlicWidgetObserver::GlicWidgetObserver(
    glic::GlicWindowController* glic_window_controller,
    views::Widget* widget)
    : glic_window_controller_(glic_window_controller), widget_(widget) {
  if (widget) {
    widget->AddObserver(this);
  }
}

GlicWindowController::GlicWidgetObserver::~GlicWidgetObserver() {
  if (widget_ && widget_->HasObserver(this)) {
    widget_->RemoveObserver(this);
  }
}

void GlicWindowController::GlicWidgetObserver::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  glic_window_controller_->NotifyWindowActivationChanged(active);
}

}  // namespace glic
