// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_controller.h"

#include "base/check.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_resize_animation.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace glic {

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kGlicWidgetAttached);

namespace {
// Default value for adding a buffer to the attachment zone.
constexpr static int kAttachmentBuffer = 20;

constexpr static int kWidgetDefaultWidth = 400;
constexpr static int kWidgetTopBarHeight = 80;
static constexpr int kAnimationDurationMs = 300;

constexpr char kHistogramGlicPanelPresentationTimeAllModes[] =
    "Glic.PanelPresentationTime.All";

mojom::PanelState CreatePanelState(bool widget_visible,
                                   Browser* attached_browser) {
  mojom::PanelState panel_state;
  if (!widget_visible) {
    panel_state.kind = mojom::PanelState_Kind::kHidden;
  } else if (attached_browser) {
    panel_state.kind = mojom::PanelState_Kind::kAttached;
    panel_state.window_id = attached_browser->session_id().id();
  } else {
    panel_state.kind = mojom::PanelState_Kind::kDetached;
  }
  return panel_state;
}

}  // namespace

class GlicWindowController::ContentsAndProfileKeepAlive
    : public content::WebContentsDelegate {
 public:
  ContentsAndProfileKeepAlive(Profile* profile,
                              GlicWindowController* glic_window_controller)
      : profile_keep_alive_(profile, ProfileKeepAliveOrigin::kGlicView),
        web_contents_(content::WebContents::Create(
            content::WebContents::CreateParams(profile))),
        glic_window_controller_(glic_window_controller) {
    DCHECK(web_contents_);
    web_contents_->SetDelegate(this);
    web_contents_->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
    web_contents_->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(
            GURL{chrome::kChromeUIGlicURL}));
  }

  ~ContentsAndProfileKeepAlive() override { web_contents_->ClosePage(); }

  ContentsAndProfileKeepAlive(const ContentsAndProfileKeepAlive&) = delete;
  ContentsAndProfileKeepAlive& operator=(const ContentsAndProfileKeepAlive&) =
      delete;

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    GlicView* glic_view = glic_window_controller_->GetGlicView();
    if (!glic_view) {
      return false;
    }
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, glic_view->web_view()->GetFocusManager());
  }
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
        web_contents, request, std::move(callback), nullptr);
  }

  ScopedProfileKeepAlive profile_keep_alive_;
  std::unique_ptr<content::WebContents> web_contents_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  // Unowned
  raw_ptr<GlicWindowController> glic_window_controller_;
};

// Helper class for observing mouse and key events from native window.
class GlicWindowController::WindowEventObserver : public ui::EventObserver {
 public:
  WindowEventObserver(glic::GlicWindowController* glic_window_controller,
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
    if (event.type() == ui::EventType::kMousePressed) {
      mouse_down_in_draggable_area_ =
          glic_view_->IsPointWithinDraggableArea(mouse_location);
    }
    if (event.type() == ui::EventType::kMouseReleased &&
        event.AsMouseEvent()->IsRightMouseButton() &&
        mouse_down_in_draggable_area_) {
      glic_window_controller_->ShowTitleBarContextMenuAt(mouse_location);
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

GlicWindowController::GlicWindowController(Profile* profile)
    : profile_(profile) {}

GlicWindowController::~GlicWindowController() = default;

void GlicWindowController::WebClientInitializeFailed() {
  if (state_ == State::kOpenAnimation ||
      state_ == State::kWaitingForGlicToLoad) {
    // TODO(crbug.com/388328847): The web client failed to initialize. Decide
    // what the fallback behavior is. Additionally, we probably need some kind
    // of timeout and/or loading indicator if loading takes too much time. For
    // now, show the UI anyway, which should be helpful in development.
    LOG(ERROR)
        << "Glic web client failed to initialize, it won't work properly.";
    show_start_time_ = base::TimeTicks();
    ShowFinish();
  }
}

void GlicWindowController::LoginPageCommitted() {
  login_page_committed_ = true;
  if ((state_ == State::kOpenAnimation ||
       state_ == State::kWaitingForGlicToLoad) &&
      !web_client_) {
    // TODO(crbug.com/388328847): Temporarily allow showing the UI when a login
    // page is reached.
    show_start_time_ = base::TimeTicks();
    ShowFinish();
  }
}

void GlicWindowController::SetWebClient(GlicWebClientAccess* web_client) {
  // If state_ == kClosed, then store web_client_ for a future call to Open().
  // Once we get crash/error flows, this can theoretically happen with state_ ==
  // kOpen, but those will those need to handled alongside the crash/error
  // flows.
  web_client_ = web_client;

  // Always reset `glic_loaded_` since the web client has changed.
  glic_loaded_ = false;

  if (state_ == State::kOpenAnimation ||
      state_ == State::kWaitingForGlicToLoad) {
    if (web_client_) {
      WaitForGlicToLoad();
    } else {
      // TODO(crbug.com/388328847): The web client could disconnect without a
      // WebClientInitializeFailed() call, for example, if the renderer crashes.
      // Determine the correct behavior in this case.
      LOG(ERROR) << "Glic web client disconnected before showing the window.";
      show_start_time_ = base::TimeTicks();
      ShowFinish();
    }
  }
}

// Monitoring the glic widget.
void GlicWindowController::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  if (GetGlicWidget() == widget) {
    window_activation_callback_list_.Notify(active);
  }
}

// Monitoring the glic widget.
void GlicWindowController::OnWidgetDestroyed(views::Widget* widget) {
  // This is used to handle the case where the native window is closed
  // directly (e.g., Windows context menu close on the title bar).
  // Conceptually this should synchronously call Close(), but the Widget
  // implementation currently does not support this.
  if (GetGlicWidget() == widget) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GlicWindowController::Close,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

// Monitoring the attached browser.
void GlicWindowController::OnWidgetBoundsChanged(views::Widget* widget,
                                                 const gfx::Rect& new_bounds) {
  if (attached_browser_ &&
      attached_browser_->GetBrowserView().GetWidget() == widget) {
    MovePositionToBrowserGlicButton(attached_browser_, false);
  }
}

void GlicWindowController::Toggle(BrowserWindowInterface* bwi) {
  // If `bwi` is non-null, the glic button was clicked on a specific window and
  // glic should be attached to that window. Otherwise glic was invoked from the
  // hotkey or other OS-level entrypoint.
  Browser* new_attached_browser =
      bwi ? bwi->GetBrowserForMigrationOnly() : nullptr;

  // In the case where the user invokes the hotkey, and the most recently used
  // window for the glic profile is active, treat this as if the user clicked
  // the glic button on that window.
  // TODO(392644541): There may be edge cases w.r.t. multi-glic-profile.
  if (!new_attached_browser) {
    Browser* last_active_browser = chrome::FindLastActiveWithProfile(profile_);
    if (last_active_browser && last_active_browser->IsActive()) {
      new_attached_browser = last_active_browser;
    }
  }

  if (state_ == State::kOpen) {
    if (new_attached_browser) {
      if (new_attached_browser == attached_browser_) {
        // Button was clicked on same browser: close.
        // There are three ways for this to happen. Normally the glic window
        // obscures the glic button. Either the user used keyboard navigation to
        // click the glic button, the user clicked the button early and the
        // button click was eventually processed asynchronously after the button
        // was obscured, or the user invokes the glic hotkey while glic is
        // attached to the active window.
        Close();
      } else {
        // Button clicked on a different browser: attach to that one.
        AttachToBrowser(new_attached_browser);
      }
      return;
    }

    // Everything else in this block handles the case where the user invokes the
    // hotkey and the most recently used window from the glic profile is not
    // active.

    // Already attached?
    if (attached_browser_) {
      if (IsActive()) {
        // Hotkey when glic active and attached: close.
        Close();
        return;
      }

      // Hotkey when glic is inactive and attached:
      if (attached_browser_->IsActive()) {
        // Hotkey when glic inactive but attached to active browser: close.
        // Note: this should not be possible, since if the attached browser is
        // active, new_attached_browser must not have been null.
        Close();
      } else {
        // Hotkey when neither attached browser nor glic are active: open
        // detached.
        CloseAndReopenDetached();
      }
      return;
    }

    // Below here: hotkey invoked when glic is already detached and will remain
    // detached.
    if (IsActive()) {
      // Hotkey when glic is open, detached and active: close.
      // TODO(392158646): This causes the browser to activate on Mac.
      Close();
    } else {
      // Glic already open detached but not active: activate.
      GetGlicWidget()->Activate();
    }

  } else if (state_ != State::kClosed) {
    // Currently in the process of showing the widget, allow that to finish.
    return;
  } else {
    Show(new_attached_browser);
  }
}

void GlicWindowController::ShowDetachedForTesting() {
  Show(nullptr);
}

void GlicWindowController::Show(Browser* browser) {
  // At this point State must be kClosed, and all glic window state must be
  // unset.
  CHECK(!attached_browser_);
  state_ = State::kOpenAnimation;

  show_start_time_ = base::TimeTicks::Now();

  if (!contents_) {
    contents_ = std::make_unique<ContentsAndProfileKeepAlive>(profile_, this);
  }

  if (browser) {
    OpenAttached(browser);
  } else {
    OpenDetached();
  }

  // If the web client is already initialized, wait for it to load in parallel.
  if (web_client_) {
    WaitForGlicToLoad();
  } else if (login_page_committed_) {
    // This indicates that we've warmed the web client and it has hit a login
    // page. See LoginPageCommitted.
    ShowFinish();
  }

  NotifyIfPanelStateChanged();
}

gfx::Rect GlicWindowController::GetInitialDetachedBounds() {
  gfx::Size widget_size(kWidgetDefaultWidth, kWidgetTopBarHeight);
  if (glic_size_) {
    widget_size = *glic_size_;
  }
  gfx::Rect initial_rect;
  // Right now this only detects whether the glic widget is summoned from the
  // OS entrypoint and positions itself detached from the browser.
  // TODO(crbug.com/384061064): Add more logic for when the glic window should
  // show up in a detached state.
  gfx::Point top_right_point = GetTopRightPositionForDetachedGlicWindow();
  int padding = 50;
  initial_rect.set_x(top_right_point.x() - widget_size.width() - padding);
  initial_rect.set_y(top_right_point.y() + padding);
  initial_rect.set_size(widget_size);
  return initial_rect;
}

void GlicWindowController::OpenAttached(Browser* browser) {
  GlicButton* glic_button_view = browser ? browser->window()
                                               ->AsBrowserView()
                                               ->tab_strip_region_view()
                                               ->GetGlicButton()
                                         : nullptr;

  // If summoned from the tab strip button. This will always show up attached
  // because it is tied to a views::View object within the current browser
  // window.
  gfx::Point top_right_point =
      GetTopRightPositionForAttachedGlicWindow(glic_button_view);
  gfx::Rect glic_window_widget_initial_rect =
      glic_button_view->GetBoundsInScreen();

  glic_widget_ = CreateGlicWidget(profile_, glic_window_widget_initial_rect);
  glic_widget_observation_.Observe(glic_widget_.get());

  glic_widget_->Show();
  AttachToBrowser(browser);

  // Set target bounds for animation and run the open attached animation.
  gfx::Size widget_size(kWidgetDefaultWidth, kWidgetTopBarHeight);
  if (glic_size_) {
    widget_size = *glic_size_;
  }
  gfx::Rect target_bounds = glic_widget_->GetWindowBoundsInScreen();
  int final_x = top_right_point.x() - widget_size.width();
  target_bounds.set_x(final_x);
  target_bounds.set_width(widget_size.width());
  target_bounds.set_height(widget_size.height());

  // TODO(crbug.com/389982576): Match the background color of the widget with
  // the web client background.
  GetGlicView()->SetBackground(
      views::CreateRoundedRectBackground(SK_ColorBLACK, 12));

  // If there's a browser, then animate.
  AnimateBounds(target_bounds, base::Milliseconds(kAnimationDurationMs),
                base::BindOnce(&GlicWindowController::OpenAnimationFinished,
                               GetWeakPtr()));
}

void GlicWindowController::OpenDetached() {
  gfx::Rect initial_bounds = GetInitialDetachedBounds();

  // Make the widget.
  glic_widget_ = CreateGlicWidget(profile_, initial_bounds);
  glic_widget_observation_.Observe(glic_widget_.get());

  // We skip the showing animation and jump straight to waiting for glic to
  // load.
  GetGlicView()->web_view()->SetWebContents(contents_->web_contents());
  state_ = State::kWaitingForGlicToLoad;
}

// This happens after the web client is initialized. It signals the web client
// that it will be shown, and waits for the response before actually showing the
// widget.
void GlicWindowController::WaitForGlicToLoad() {
  DCHECK(web_client_);
  // Notify the web client that the panel will open, and wait for the response
  // to actually show the window.
  web_client_->PanelWillOpen(
      CreatePanelState(true, attached_browser_),
      base::BindOnce(&GlicWindowController::GlicLoaded, GetWeakPtr()));
}

void GlicWindowController::GlicLoaded() {
  glic_loaded_ = true;
  if (state_ == State::kWaitingForGlicToLoad) {
    ShowFinish();
  }
}

void GlicWindowController::OpenAnimationFinished() {
  if (state_ == State::kOpenAnimation) {
    state_ = State::kWaitingForGlicToLoad;

    GetGlicView()->web_view()->SetWebContents(contents_->web_contents());
    if (glic_loaded_) {
      ShowFinish();
    }
  }
}

void GlicWindowController::ShowFinish() {
  login_page_committed_ = false;
  if (state_ == State::kClosed || state_ == State::kOpen) {
    return;
  }
  state_ = State::kOpen;

  if (!attached_browser_) {
    // Be sure to reparent the widget and set its state first before showing it.
    MaybeCreateHolderWindowAndReparent();
#if BUILDFLAG(IS_MAC)
    // Be careful to not activate, so that in case Chromium isn't the front-most
    // app it's not brought to the front.
    GetGlicWidget()->ShowInactive();
#else
    GetGlicWidget()->Show();
#endif
  }

  if (web_client_ && !show_start_time_.is_null()) {
    base::UmaHistogramCustomTimes(kHistogramGlicPanelPresentationTimeAllModes,
                                  (base::TimeTicks::Now() - show_start_time_),
                                  base::Milliseconds(1), base::Seconds(60), 50);
    show_start_time_ = base::TimeTicks();
  }

  window_event_observer_ =
      std::make_unique<WindowEventObserver>(this, GetGlicView());

  // Set the draggable area to the top bar of the window, by default.
  GetGlicView()->SetDraggableAreas(
      {{0, 0, GetGlicView()->width(), kWidgetTopBarHeight}});
  NotifyIfPanelStateChanged();
}

GlicView* GlicWindowController::GetGlicView() {
  if (!GetGlicWidget()) {
    return nullptr;
  }
  return static_cast<GlicView*>(GetGlicWidget()->GetContentsView());
}

views::Widget* GlicWindowController::GetGlicWidget() {
  return glic_widget_.get();
}

content::WebContents* GlicWindowController::GetWebContents() {
  if (!contents_) {
    return nullptr;
  }
  return contents_->web_contents();
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
  attached_browser_ = nullptr;
  Close();
}

void GlicWindowController::ResizeFinished() {
  window_resize_animation_.reset();
}

void GlicWindowController::Attach() {
  if (!GetGlicWidget()) {
    return;
  }

  // TODO (crbug.com/388917542) Determine which browser to attach to. Currently
  // attaches to the last focused glic-compatible browser.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (!IsBrowserGlicCompatible(browser)) {
      continue;
    }
    AttachToBrowser(browser);
    return;
  }
}

void GlicWindowController::Detach() {
  MaybeCreateHolderWindowAndReparent();
  // TODO (crbug.com/388922182) Determine where to move the window to. Currently
  // moves to the top right of the display.
  gfx::Size screen_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  gfx::Rect widget_bounds = glic_widget_->GetWindowBoundsInScreen();
  widget_bounds.set_origin(
      gfx::Point(screen_size.width() - widget_bounds.width(), 0));
  AnimateBounds(widget_bounds, base::Milliseconds(kAnimationDurationMs),
                base::DoNothing());
}

void GlicWindowController::AttachToBrowser(Browser* browser) {
  CHECK(GetGlicWidget());
  attached_browser_ = browser;
  MovePositionToBrowserGlicButton(browser, true);
  // Close the holder window.
  holder_widget_.reset();

  views::Widget* browser_widget =
      browser->window()->AsBrowserView()->GetWidget();
  CHECK(browser_widget);

  // Makes the glic widget a child view of the given widget's browser.
  // Add observer to new parent.
  attached_browser_widget_observation_.Reset();
  attached_browser_widget_observation_.Observe(browser_widget);
  glic_widget_->Reparent(browser_widget);
  if (!IsActive()) {
    GetGlicWidget()->Activate();
  }

  NotifyIfPanelStateChanged();

  // When attached to a browser window, the glic widget mustn't float and when
  // interacted with must behave like any other widget.
  GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kNormal);
#if BUILDFLAG(IS_MAC)
  GetGlicWidget()->SetActivationIndependence(false);
#endif

  browser_close_subscription_ = browser->RegisterBrowserDidClose(
      base::BindRepeating(&GlicWindowController::AttachedBrowserDidClose,
                          base::Unretained(this)));

  // Trigger custom event for testing.
  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kGlicWidgetAttached, browser->window()
                               ->AsBrowserView()
                               ->tab_strip_region_view()
                               ->GetGlicButton());
}

void GlicWindowController::Resize(const gfx::Size& size,
                                  base::TimeDelta duration,
                                  base::OnceClosure callback) {
  glic_size_ = size;

  // If the glic window is not in the ready state, do nothing for now.
  // TODO(https://crbug.com/379164689): Drive resize animations for error states
  // from the browser. For now, we allow animations during the waiting state.
  if (state_ == State::kOpen || state_ == State::kWaitingForGlicToLoad) {
    AnimateSize(size, duration, std::move(callback));
  } else {
    // If the glic window is closed, or the widget isn't ready (e.g. because
    // it's currently still animating open) immediately post the callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
}

void GlicWindowController::AnimateBounds(const gfx::Rect& target_bounds,
                                         base::TimeDelta duration,
                                         base::OnceClosure callback) {
  CHECK(GetGlicWidget());

  // Stop the current animation if any.
  if (window_resize_animation_) {
    ResizeFinished();
  }

  if (duration < base::Milliseconds(0)) {
    duration = base::Milliseconds(0);
  }

  window_resize_animation_ = std::make_unique<GlicWindowResizeAnimation>(
      this, target_bounds, duration, std::move(callback));
}

void GlicWindowController::AnimateSize(const gfx::Size& target_size,
                                       base::TimeDelta duration,
                                       base::OnceClosure callback) {
  // Maintain the top-right corner.
  gfx::Rect current_bounds = GetGlicWidget()->GetWindowBoundsInScreen();
  int original_top_right = current_bounds.x() + current_bounds.width();
  current_bounds.set_size(target_size);
  current_bounds.set_x(original_top_right - target_size.width());
  AnimateBounds(current_bounds, duration, std::move(callback));
}

std::unique_ptr<views::Widget> GlicWindowController::CreateGlicWidget(
    Profile* profile,
    const gfx::Rect& initial_bounds) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
#if BUILDFLAG(IS_WIN)
  params.dont_show_in_taskbar = true;
  params.force_system_menu_for_frameless = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
#endif
  params.bounds = initial_bounds;
  params.sublevel = ChromeWidgetSublevel::kSublevelHoverable;

  std::unique_ptr<views::Widget> widget =
      std::make_unique<views::Widget>(std::move(params));

  widget->SetContentsView(
      std::make_unique<GlicView>(profile, initial_bounds.size()));

  return widget;
}

gfx::Size GlicWindowController::GetSize() {
  if (!GetGlicWidget()) {
    return gfx::Size();
  }

  return GetGlicWidget()->GetSize();
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
  if (state_ == State::kCloseAnimation || state_ == State::kClosed) {
    return;
  }

  const bool reopen_detached = state_ == State::kClosingToReopenDetached;

  if (attached_browser_) {
    state_ = State::kCloseAnimation;
    GetGlicView()->web_view()->SetWebContents(nullptr);
    GlicButton* glic_button = attached_browser_->window()
                                  ->AsBrowserView()
                                  ->tab_strip_region_view()
                                  ->GetGlicButton();
    AnimateBounds(glic_button->GetBoundsInScreen(),
                  base::Milliseconds(kAnimationDurationMs),
                  base::BindOnce(&GlicWindowController::CloseFinish,
                                 GetWeakPtr(), reopen_detached));
  } else {
    CloseFinish(reopen_detached);
  }
}

void GlicWindowController::CloseFinish(bool reopen_detached) {
  if (state_ == State::kClosed) {
    return;
  }

  state_ = State::kClosed;
  attached_browser_ = nullptr;
  attached_browser_widget_observation_.Reset();
  window_resize_animation_.reset();
  window_event_observer_.reset();
  browser_close_subscription_.reset();
  glic_widget_observation_.Reset();
  glic_widget_.reset();
  NotifyIfPanelStateChanged();

  if (web_client_) {
    // The webview is kept alive by default, no need to use this callback.
    web_client_->PanelWasClosed(base::DoNothing());
  }

  if (reopen_detached) {
    Show(nullptr);
  }
}

void GlicWindowController::CloseAndReopenDetached() {
  if (state_ != State::kOpen) {
    return;
  }

  state_ = State::kClosingToReopenDetached;
  Close();
}

void GlicWindowController::ShowTitleBarContextMenuAt(gfx::Point event_loc) {
#if BUILDFLAG(IS_WIN)
  views::View::ConvertPointToScreen(GetGlicView(), &event_loc);
  event_loc = display::win::ScreenWin::DIPToScreenPoint(event_loc);
  views::ShowSystemMenuAtScreenPixelLocation(views::HWNDForView(GetGlicView()),
                                             event_loc);
#endif  // BUILDFLAG(IS_WIN)
}

void GlicWindowController::HandleWindowDragWithOffset(
    gfx::Vector2d mouse_offset) {
  // This code isn't set up to handle nested run loops. Nested run loops will
  // lead to crashes.
  if (!in_move_loop_) {
    in_move_loop_ = true;
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    GetGlicWidget()->RunMoveLoop(
        mouse_offset, move_loop_source,
        views::Widget::MoveLoopEscapeBehavior::kDontHide);
    in_move_loop_ = false;
    // Check whether `GetGlicWidget()` is in a position to attach to a
    // browser window.
    HandleAttachmentToBrowserWindows(GetGlicWidget());
  }
}

void GlicWindowController::HandleAttachmentToBrowserWindows(
    views::Widget* widget) {
  // This should only ever be called after a move is completed.
  CHECK(!in_move_loop_);

  // The profile must have started off as Glic enabled since a Glic widget is
  // open but it may have been disabled at runtime by policy. In this edge-case,
  // prevent reattaching back to a window (as it no longer has a GlicButton).
  if (!GlicEnabling::IsEnabledForProfile(profile_)) {
    return;
  }

  gfx::Point glic_top_right = widget->GetWindowBoundsInScreen().top_right();
  // Loops through all browsers in activation order with the latest accessed
  // browser first.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (!IsBrowserGlicCompatible(browser)) {
      continue;
    }

    // If the profile is enabled, the Glic button must be available.
    auto* tab_strip_region_view =
        browser->window()->AsBrowserView()->tab_strip_region_view();
    CHECK(tab_strip_region_view);
    CHECK(tab_strip_region_view->GetGlicButton());

    // Define attachment zone as the right of the tab strip. It either is the
    // width of the widget or 1/3 of the tab strip, whichever is smaller.
    gfx::Rect attachment_zone = tab_strip_region_view->GetBoundsInScreen();
    int width = std::min(attachment_zone.width() / 3, kWidgetDefaultWidth);
    attachment_zone.SetByBounds(attachment_zone.right() - width,
                                attachment_zone.y() - kAttachmentBuffer,
                                attachment_zone.right() + kAttachmentBuffer,
                                attachment_zone.bottom());

    // If there is no active drag (i.e. the previous drag has ended)
    // then determine whether the glic window should be attached or detached
    // from the browser window.
    if (attachment_zone.Contains(glic_top_right)) {
      AttachToBrowser(browser);
      return;
    }

    // If farther than the attachment threshold from the current parent
    // widget, reparent under an empty holder widget.
    if (attached_browser_ == browser) {
      MaybeCreateHolderWindowAndReparent();
      return;
    }
  }
}

void GlicWindowController::MovePositionToBrowserGlicButton(Browser* browser,
                                                           bool animate) {
  if (!GetGlicWidget()) {
    return;
  }

  // If the profile's been disabled (e.g. by policy) the window's Glic button
  // will be removed so we can't anchor to it. We could work around this by
  // keeping the button but disabling and making it invisible but this is an
  // edge-case, not sure it's worth the effort.
  if (!GlicEnabling::IsEnabledForProfile(browser->profile())) {
    return;
  }


  TabStripActionContainer* tab_strip_action_container = browser->window()
                                ->AsBrowserView()
                                ->tab_strip_region_view()
                                ->GetTabStripActionContainer();
  CHECK(tab_strip_action_container);

  // TODO(andreaxg): Fix exact attachment position.
  gfx::Rect tab_strip_container_rect = tab_strip_action_container->GetBoundsInScreen();
  gfx::Point top_right = tab_strip_container_rect.top_right();
  int tab_strip_padding = GetLayoutConstant(TAB_STRIP_PADDING);

  gfx::Rect current_bounds = GetGlicWidget()->GetWindowBoundsInScreen();
  gfx::Rect new_bounds = current_bounds;
  new_bounds.set_x(top_right.x() - current_bounds.width() - tab_strip_padding);
  new_bounds.set_y(top_right.y() + tab_strip_padding);
  // Avoid conversions between pixels and DIP on non 1.0 scale factor displays
  // changing widget width and height.
  if (animate) {
    AnimateBounds(new_bounds, base::Milliseconds(kAnimationDurationMs),
                  base::DoNothing());
  } else {
    GetGlicWidget()->SetBounds(new_bounds);
  }
  NotifyIfPanelStateChanged();
}

void GlicWindowController::MaybeCreateHolderWindowAndReparent() {
  attached_browser_ = nullptr;
  attached_browser_widget_observation_.Reset();
  browser_close_subscription_.reset();
  if (!holder_widget_) {
    holder_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = false;
    // Widget name is specified for debug purposes.
    params.name = "HolderWindow";
    params.bounds = glic_widget_->GetWindowBoundsInScreen();
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    holder_widget_->Init(std::move(params));
    holder_widget_->ShowInactive();
  }

  glic_widget_->Reparent(holder_widget_.get());
  NotifyIfPanelStateChanged();

  // When the glic window is in a detached state, elevate its z-order to be
  // always on top. On the Mac, mark it as "activation independent" so that
  // interacting with it does not activate Chrome.
  GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
#if BUILDFLAG(IS_MAC)
  GetGlicWidget()->SetActivationIndependence(true);
#endif
}

bool GlicWindowController::IsBrowserGlicCompatible(Browser* browser) {
  // A browser is not compatible if it:
  // - is not a TYPE_NORMAL browser
  // - is from a glic-disabled profile
  // - is not visible
  // - uses a different Profile from glic
  if (!GlicEnabling::IsEnabledForProfile(browser->profile()) ||
      !browser->is_type_normal() || !browser->window()->IsVisible() ||
      browser->profile() != profile_) {
    return false;
  }
  return true;
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
  if (state_ == State::kClosed || state_ == State::kCloseAnimation) {
    panel_state.kind = mojom::PanelState_Kind::kHidden;
  } else if (attached_browser_) {
    panel_state.kind = mojom::PanelState_Kind::kAttached;
    panel_state.window_id = attached_browser_->session_id().id();
  } else {
    panel_state.kind = mojom::PanelState_Kind::kDetached;
  }
  return panel_state;
}

bool GlicWindowController::IsActive() {
  return IsShowing() && GetGlicWidget() && GetGlicWidget()->IsActive();
}

bool GlicWindowController::IsShowing() const {
  return !(state_ == State::kClosed || state_ == State::kCloseAnimation);
}

bool GlicWindowController::IsAttached() {
  return attached_browser_ != nullptr;
}

base::CallbackListSubscription
GlicWindowController::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  return window_activation_callback_list_.Add(std::move(callback));
}

void GlicWindowController::Preload() {
  if (!contents_) {
    contents_ = std::make_unique<ContentsAndProfileKeepAlive>(profile_, this);
  }
}

bool GlicWindowController::IsWarmed() {
  return !!contents_;
}

base::WeakPtr<GlicWindowController> GlicWindowController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GlicWindowController::Shutdown() {
  // Hide first, then clean up.
  Close();
  contents_.reset();
}

}  // namespace glic
