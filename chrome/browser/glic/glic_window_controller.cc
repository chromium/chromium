// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_controller.h"

#include "base/check.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget_observer.h"

namespace {
// Default value for how close the top-right corner of the glic window must be
// to a browser's glic button to attach to said browser.
constexpr static int kAttachmentDistanceThreshold = 50;

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
  if (glic_window_widget_) {
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
    top_right_point = GetTopRightPositionForDetachedGlicWindow();
    padding = 50;
  } else {
    // If summoned from the tab strip button. This will always show up attached
    // because it is tied to a views::View object within the current browser
    // window.
    top_right_point =
        GetTopRightPositionForAttachedGlicWindow(glic_button_view);
    padding = GetLayoutConstant(TAB_STRIP_PADDING);
    should_attach_to_browser = true;
  }

  glic_window_widget_ = glic::GlicView::CreateWidget(
      profile_, {top_right_point.x() - kWidgetWidth - padding,
                 top_right_point.y() + padding, kWidgetWidth, kWidgetHeight});
  glic_window_widget_->AddObserver(this);
  glic_widget_observer_ =
      std::make_unique<GlicWidgetObserver>(this, glic_window_widget_.get());
  glic_window_widget_->Show();

  if (should_attach_to_browser) {
    views::Widget* glic_button_widget = glic_button_view->GetWidget();
    Browser* browser =
        chrome::FindBrowserWithWindow(glic_button_widget->GetNativeWindow());
    // TODO(cuianthony): we should be using the browser's native view as the
    // target to attach to.
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
  if (!glic_window_widget_) {
    return nullptr;
  }
  return GlicView::FromWidget(*glic_window_widget_);
}

gfx::Point GlicWindowController::GetTopRightPositionForAttachedGlicWindow(
    views::View* glic_button_view) {
  // Initial position determined by glic button bounds. Returns the top right
  // point of the button.
  gfx::Point top_right_bounds =
      glic_button_view->GetBoundsInScreen().top_right();

  return top_right_bounds;
}

gfx::Point GlicWindowController::GetTopRightPositionForDetachedGlicWindow() {
  // Position determined by screen bounds. Returns the top right point of the
  // screen.
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Point top_right_bounds =
      screen->GetPrimaryDisplay().work_area().top_right();

  return top_right_bounds;
}

void GlicWindowController::AttachedBrowserDidClose(
    BrowserWindowInterface* browser) {
  Close();
}

void GlicWindowController::AttachToBrowser(Browser* browser,
                                           views::Widget* target_widget) {
  // Makes the glic widget a child view of the given widget's browser.
  if (target_widget && glic_window_widget_ && browser) {
    // Add observer to new parent.
    attached_target_widget_observer_.SetAttachedTargetWidget(target_widget);
    attached_browser_ = browser->AsWeakPtr();
    views::Widget::ReparentNativeView(glic_window_widget_->GetNativeView(),
                                      target_widget->GetNativeView());
    NotifyIfPanelStateChanged();

    glic_window_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
    browser_close_subscription_ = browser->RegisterBrowserDidClose(
        base::BindRepeating(&GlicWindowController::AttachedBrowserDidClose,
                            base::Unretained(this)));
  }
}

bool GlicWindowController::Resize(const gfx::Size& size) {
  if (!glic_window_widget_) {
    return false;
  }

  glic_window_widget_->SetSize(size);
  GetGlicView()->web_view()->SetSize(size);
  return true;
}

gfx::Size GlicWindowController::GetSize() {
  if (!glic_window_widget_) {
    return gfx::Size();
  }

  return glic_window_widget_->GetSize();
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
  if (!glic_window_widget_) {
    return;
  }
  SetAudioDucking(false);
  glic_window_widget_->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  glic_widget_observer_.reset();
  window_event_observer_.reset();
  browser_close_subscription_.reset();
  glic_window_widget_.reset();
  NotifyIfPanelStateChanged();
}

bool GlicWindowController::SetAudioDucking(bool enabled) {
  glic::GlicView* glic_view = GetGlicView();
  if (!glic_view) {
    return false;
  }
  content::WebContents* contents = glic_view->web_view()->GetWebContents();
  AudioDucker* audio_ducker =
      AudioDucker::GetOrCreateForPage(contents->GetPrimaryPage());
  if (enabled) {
    return audio_ducker->StartDuckingOtherAudio();
  } else {
    return audio_ducker->StopDuckingOtherAudio();
  }
}

void GlicWindowController::HandleWindowDragWithOffset(
    gfx::Vector2d mouse_offset) {
  // This code isn't set up to handle nested run loops. Nested run loops will
  // lead to crashes.
  if (!in_move_loop_) {
    in_move_loop_ = true;
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    glic_window_widget_->RunMoveLoop(
        mouse_offset, move_loop_source,
        views::Widget::MoveLoopEscapeBehavior::kDontHide);
    in_move_loop_ = false;
    // Check whether `widget_` is in a position to attach to a browser window.
    HandleAttachmentToBrowserWindows(glic_window_widget_.get());
  }
}

void GlicWindowController::HandleAttachmentToBrowserWindows(
    views::Widget* widget) {
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
        !browser->window()->IsVisible() ||
        window_widget == glic_window_widget_.get() ||
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
    // If there is no active drag (i.e. the previous drag has ended)
    // then determine whether the glic window should be attached or detached
    // from the browser window.
    if (!in_move_loop_) {
      if (corner_distance < kAttachmentDistanceThreshold) {
        MovePositionToBrowserGlicButton(browser, true);
        // Close any existing holder widget in anticipation of reparenting under
        // the browser.
        if (holder_widget_) {
          holder_widget_->CloseWithReason(
              views::Widget::ClosedReason::kLostFocus);
          holder_widget_.reset();
        }
        AttachToBrowser(browser, window_widget);
      } else if (glic_window_widget_->parent() == window_widget) {
        // If farther than the attachment threshold from the current parent
        // widget, reparent under an empty holder widget.
        MaybeCreateHolderWindowAndReparent();
      }
    }
  }
}

void GlicWindowController::MovePositionToBrowserGlicButton(Browser* browser,
                                                           bool animate) {
  if (!glic_window_widget_) {
    return;
  }
  attached_browser_ = browser->AsWeakPtr();
  gfx::Rect glic_rect = glic_window_widget_->GetWindowBoundsInScreen();
  // TODO(andreaxg): Fix exact attachment position.
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
    glic_window_widget_->SetBounds(glic_rect);
  }
  NotifyIfPanelStateChanged();
}

void GlicWindowController::MaybeCreateHolderWindowAndReparent() {
  attached_target_widget_observer_.SetAttachedTargetWidget(nullptr);
  browser_close_subscription_.reset();
  attached_browser_ = nullptr;
  if (!holder_widget_) {
    holder_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = false;
    // Widget name is specified for debug purposes.
    params.name = "HolderWindow";
    params.bounds = glic_window_widget_->GetWindowBoundsInScreen();
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    holder_widget_->Init(std::move(params));
    holder_widget_->ShowInactive();
  }
  views::Widget::ReparentNativeView(glic_window_widget_->GetNativeView(),
                                    holder_widget_->GetNativeView());
  NotifyIfPanelStateChanged();

  // When the glic window is in a detached state, elevate it's z-order to be
  // always on top.
  glic_window_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
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
  if (!glic_window_widget_visible_) {
    panel_state.kind = mojom::PanelState_Kind::kHidden;
  } else if (attached_browser_) {
    panel_state.kind = mojom::PanelState_Kind::kDocked;
    panel_state.window_id = attached_browser_->session_id().id();
  } else {
    panel_state.kind = mojom::PanelState_Kind::kFloating;
  }
  return panel_state;
}

void GlicWindowController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  // Store visibility locally because calling widget_->IsVisible() at this point
  // returns the old value.
  glic_window_widget_visible_ = visible;
  NotifyIfPanelStateChanged();
}

bool GlicWindowController::IsActive() {
  return glic_window_widget_ && glic_window_widget_->IsActive();
}

bool GlicWindowController::HasWindow() const {
  return !!glic_window_widget_;
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
// AttachedTargetWidgetObserver implementations:
GlicWindowController::AttachedTargetWidgetObserver::
    AttachedTargetWidgetObserver(
        glic::GlicWindowController* glic_window_controller)
    : glic_window_controller_(glic_window_controller) {}

GlicWindowController::AttachedTargetWidgetObserver::
    ~AttachedTargetWidgetObserver() {
  SetAttachedTargetWidget(nullptr);
}

void GlicWindowController::AttachedTargetWidgetObserver::
    SetAttachedTargetWidget(views::Widget* new_attachment_target) {
  if (new_attachment_target == current_attachment_target_) {
    return;
  }
  if (current_attachment_target_ &&
      current_attachment_target_->HasObserver(this)) {
    current_attachment_target_->RemoveObserver(this);
    current_attachment_target_ = nullptr;
  }
  // Start observing the new widget that the glic window has attached to.
  if (new_attachment_target && !new_attachment_target->HasObserver(this)) {
    new_attachment_target->AddObserver(this);
    current_attachment_target_ = new_attachment_target;
  }
}

void GlicWindowController::AttachedTargetWidgetObserver::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  // Update the position of the glic window to follow changes in the browser
  // window widget that it is attached to.
  glic_window_controller_->MovePositionToBrowserGlicButton(
      chrome::FindBrowserWithWindow(widget->GetNativeWindow()), false);
}

void GlicWindowController::AttachedTargetWidgetObserver::OnWidgetDestroying(
    views::Widget* widget) {
  SetAttachedTargetWidget(nullptr);
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
