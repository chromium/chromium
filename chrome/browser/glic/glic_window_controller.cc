// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_controller.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/glic/glic_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "ui/events/event_observer.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"

namespace {
// Default value for how close the corner of glic has to be from a browser's
// glic button to snap.
constexpr static int kSnapDistanceThreshold = 50;

// Helper class for observing mouse and key events from native window.
class WindowEventObserver : public ui::EventObserver {
 public:
  explicit WindowEventObserver(
      glic::GlicWindowController* glic_window_controller,
      glic::GlicView* glic_view)
      : glic_window_controller_(glic_window_controller), glic_view_(glic_view) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, glic_view->GetWidget()->GetNativeWindow(),
        {ui::EventType::kMouseDragged});
  }

  WindowEventObserver(const WindowEventObserver&) = delete;
  WindowEventObserver& operator=(const WindowEventObserver&) = delete;
  ~WindowEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
    if (event.IsMouseEvent()) {
      if (event.type() == ui::EventType::kMouseDragged) {
        gfx::Point mouse_location = event_monitor_->GetLastMouseLocation();
        views::View::ConvertPointFromScreen(glic_view_, &mouse_location);
        glic_window_controller_->DragFromPoint(
            mouse_location.OffsetFromOrigin());
      }
    }
  }

  raw_ptr<glic::GlicWindowController> glic_window_controller_;
  raw_ptr<glic::GlicView> glic_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

}  // namespace

namespace glic {

GlicWindowController::GlicWindowController(Profile* profile)
    : profile_(profile) {}

void GlicWindowController::Show(const views::View* glic_button_view) {
  // TODO(crbug.com/379943498): possibly bring to front or activate in this case
  if (widget_) {
    return;
  }

  if (!glic_button_view) {
    // TODO(crbug.com/382311793): Position the window when opened from the
    // launcher.
    return;
  }

  // Initial position determined by glic view bounds.
  gfx::Point top_right_bounds =
      glic_button_view->GetBoundsInScreen().top_right();
  const int width = 400;
  const int height = 800;
  const int padding = GetLayoutConstant(TAB_STRIP_PADDING);

  std::tie(widget_, glic_view_) = glic::GlicView::CreateWidget(
      profile_, {top_right_bounds.x() - width - padding,
                 top_right_bounds.y() + padding, width, height});
  widget_->Show();
  window_event_observer_ =
      std::make_unique<WindowEventObserver>(this, glic_view_);
}

bool GlicWindowController::Resize(const gfx::Size& size) {
  if (!widget_) {
    return false;
  }

  widget_->SetSize(size);
  glic_view_->web_view()->SetSize(size);
  return true;
}

gfx::Size GlicWindowController::GetSize() {
  if (!widget_) {
    return gfx::Size();
  }

  return widget_->GetSize();
}

void GlicWindowController::Close() {
  if (!widget_) {
    return;
  }

  widget_->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  widget_.reset();
  glic_view_ = nullptr;
}

void GlicWindowController::DragFromPoint(gfx::Vector2d mouse_location) {
  // This code isn't set up to handle nested run loops. Nested run loops will
  // lead to crashes.
  if (!in_move_loop_) {
    in_move_loop_ = true;
    gfx::Vector2d drag_offset = mouse_location;
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    widget_->RunMoveLoop(drag_offset, move_loop_source,
                         views::Widget::MoveLoopEscapeBehavior::kDontHide);
    HandleBrowserPinning(widget_->GetWindowBoundsInScreen().OffsetFromOrigin() +
                         mouse_location);
    in_move_loop_ = false;
  }
}

void GlicWindowController::HandleBrowserPinning(gfx::Vector2d mouse_location) {
  // Loops through all browsers in activation order with the latest accessed
  // browser first.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    views::Widget* window_widget =
        browser->window()->AsBrowserView()->GetWidget();
    // Skips if:
    // - incognito
    // - not visible
    // - is the same widget as glic
    // - is a different profile (uses browser context to check)
    if (browser->profile()->IsOffTheRecord() ||
        !browser->window()->IsVisible() || window_widget == widget_.get() ||
        browser->GetWebView()->GetBrowserContext() !=
            glic_view_->web_view()->GetBrowserContext()) {
      continue;
    }
    auto* tab_strip_region_view =
        browser->window()->AsBrowserView()->tab_strip_region_view();
    if (!tab_strip_region_view || !tab_strip_region_view->glic_button()) {
      continue;
    }
    gfx::Rect glic_button_rect =
        tab_strip_region_view->glic_button()->GetBoundsInScreen();

    float glic_button_mouse_distance =
        (glic_button_rect.CenterPoint() -
         gfx::PointAtOffsetFromOrigin(mouse_location))
            .Length();
    if (glic_button_mouse_distance < kSnapDistanceThreshold) {
      MoveToBrowserPinTarget(browser);
      // Close holder window if existing
      if (holder_widget_) {
        holder_widget_->CloseWithReason(
            views::Widget::ClosedReason::kLostFocus);
        holder_widget_.reset();
      }
      // add observer to new parent
      pinned_target_widget_observer_.SetPinnedTargetWidget(window_widget);
      views::Widget::ReparentNativeView(widget_->GetNativeView(),
                                        window_widget->GetNativeView());
    } else if (widget_->parent() == window_widget) {
      // If farther than the snapping threshold from the current parent
      // widget, open a blank holder window to reparent to
      MaybeCreateHolderWindowAndReparent();
    }
  }
}

void GlicWindowController::MoveToBrowserPinTarget(Browser* browser) {
  if (!widget_) {
    return;
  }
  gfx::Rect glic_rect = widget_->GetWindowBoundsInScreen();
  // TODO fix exact snap location
  gfx::Rect glic_button_rect = browser->window()
                                   ->AsBrowserView()
                                   ->tab_strip_region_view()
                                   ->glic_button()
                                   ->GetBoundsInScreen();
  gfx::Point top_right = glic_button_rect.top_right();
  int tab_strip_padding = GetLayoutConstant(TAB_STRIP_PADDING);
  glic_rect.set_x(top_right.x() - glic_rect.width() - tab_strip_padding);
  glic_rect.set_y(top_right.y() + tab_strip_padding);
  widget_->SetBounds(glic_rect);
}

void GlicWindowController::MaybeCreateHolderWindowAndReparent() {
  pinned_target_widget_observer_.SetPinnedTargetWidget(nullptr);
  if (!holder_widget_) {
    holder_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = false;
    // Name specified for debug purposes
    params.name = "HolderWindow";
    params.bounds = gfx::Rect(0, 0, 0, 0);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    holder_widget_->Init(std::move(params));
  }
  views::Widget::ReparentNativeView(widget_->GetNativeView(),
                                    holder_widget_->GetNativeView());
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
      chrome::FindBrowserWithWindow(widget->GetNativeWindow()));
}

void GlicWindowController::PinnedTargetWidgetObserver::OnWidgetDestroying(
    views::Widget* widget) {
  SetPinnedTargetWidget(nullptr);
}

base::WeakPtr<GlicWindowController> GlicWindowController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GlicWindowController::~GlicWindowController() = default;

}  // namespace glic
