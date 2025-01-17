// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_controller.h"

#include "base/check.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_resize_animation.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
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
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace glic {

namespace {
// Default value for how close the top-right corner of the glic window must be
// to a browser's glic button to attach to said browser.
constexpr static int kAttachmentDistanceThreshold = 50;

constexpr static int kWidgetWidth = 400;
constexpr static int kWidgetHeight = 800;
constexpr static int kWidgetTopBarHeight = 80;

class ContentsAndProfileKeepAlive : public content::WebContentsDelegate {
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

GlicWindowController::GlicWindowController(Profile* profile)
    : profile_(profile) {}

GlicWindowController::~GlicWindowController() = default;

void GlicWindowController::WebClientInitializeFailed() {
  if (will_show_) {
    // TODO(crbug.com/388328847): The web client failed to initialize. Decide
    // what the fallback behavior is. Additionally, we probably need some kind
    // of timeout and/or loading indicator if loading takes too much time. For
    // now, show the UI anyway, which should be helpful in development.
    LOG(ERROR)
        << "Glic web client failed to initialize, it won't work properly.";
    ShowFinish();
  }
}

void GlicWindowController::LoginPageCommitted() {
  if (will_show_ && !web_client_) {
    // TODO(crbug.com/388328847): Temporarily allow showing the UI when a login
    // page is reached.
    ShowFinish();
  }
}

void GlicWindowController::SetWebClient(GlicWebClientAccess* web_client) {
  web_client_ = web_client;
  if (will_show_) {
    if (web_client_) {
      ShowPhase2();
    } else {
      // TODO(crbug.com/388328847): The web client could disconnect without a
      // WebClientInitializeFailed() call, for example, if the renderer crashes.
      // Determine the correct behavior in this case.
      LOG(ERROR) << "Glic web client disconnected before showing the window.";
      ShowFinish();
    }
  }
}

void GlicWindowController::Show(views::View* glic_button_view) {
  // TODO(crbug.com/379943498): If a glic window already exists, handle showing
  // by bringing to front or activating.
  if (glic_window_widget_ || will_show_) {
    return;
  }
  int padding;
  gfx::Point top_right_point;
  will_show_ = true;
  if (!glic_button_view) {
    // Right now this only detects whether the glic widget is summoned from the
    // OS entrypoint and positions itself detached from the browser.
    // TODO(crbug.com/384061064): Add more logic for when the glic window should
    // show up in a detached state.
    top_right_point = GetTopRightPositionForDetachedGlicWindow();
    padding = 50;
    button_widget_for_browser_attachment_ = nullptr;
  } else {
    // If summoned from the tab strip button. This will always show up attached
    // because it is tied to a views::View object within the current browser
    // window.
    top_right_point =
        GetTopRightPositionForAttachedGlicWindow(glic_button_view);
    padding = GetLayoutConstant(TAB_STRIP_PADDING);
    button_widget_for_browser_attachment_ =
        glic_button_view->GetWidget()->GetWeakPtr();
  }

  glic_window_widget_ = glic::GlicView::CreateWidget(
      profile_, {top_right_point.x() - kWidgetWidth - padding,
                 top_right_point.y() + padding, kWidgetWidth, kWidgetHeight});

  GlicView* glic_view = GlicView::FromWidget(*glic_window_widget_);
  if (!contents_) {
    contents_ = std::make_unique<ContentsAndProfileKeepAlive>(profile_, this);
  }
  glic_view->web_view()->SetWebContents(contents_->web_contents());

  glic_window_widget_->AddObserver(this);
  glic_widget_observer_ =
      std::make_unique<GlicWidgetObserver>(this, glic_window_widget_.get());

  // If the web client is already initialized, go to phase 2. Otherwise, wait
  // for the web client to initialize.
  if (web_client_) {
    ShowPhase2();
  }
}

// Phase 2 of showing the widget. This happens after the web client is
// initialized. It signals the web client that it will be shown, and waits for
// the response before actually showing the widget.
void GlicWindowController::ShowPhase2() {
  DCHECK(web_client_);
  // Notify the web client that the panel will open, and wait for the response
  // to actually show the window.
  web_client_->PanelWillOpen(
      CreatePanelState(
          true,
          button_widget_for_browser_attachment_
              ? chrome::FindBrowserWithWindow(
                    button_widget_for_browser_attachment_->GetNativeWindow())
              : nullptr),
      base::BindOnce(&GlicWindowController::ShowFinish, GetWeakPtr()));
}

void GlicWindowController::ShowFinish() {
  will_show_ = false;
  if (!glic_window_widget_ || glic_window_widget_->IsVisible()) {
    return;
  }

  if (button_widget_for_browser_attachment_) {
    glic_window_widget_->Show();
    Browser* browser = chrome::FindBrowserWithWindow(
        button_widget_for_browser_attachment_->GetNativeWindow());
    AttachToBrowser(browser);
  } else {
    // Be sure to reparent the widget and set its state first before showing it.
    MaybeCreateHolderWindowAndReparent();
#if BUILDFLAG(IS_MAC)
    // Be careful to not activate, so that in case Chromium isn't the front-most
    // app it's not brought to the front.
    glic_window_widget_->ShowInactive();
#else
    glic_window_widget_->Show();
#endif
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

void GlicWindowController::ResizeFinished() {
  window_resize_animation_.reset();
}

void GlicWindowController::Attach() {
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
  gfx::Rect bounds = glic_window_widget_->GetWindowBoundsInScreen();
  bounds.set_origin(gfx::Point(screen_size.width() - bounds.width(), 0));
  GetGlicView()->AnimateFrameBounds(bounds);
}

void GlicWindowController::AttachToBrowser(Browser* browser) {
  MovePositionToBrowserGlicButton(browser, true);
  // Close holder window if existing.
  if (holder_widget_) {
    holder_widget_->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    holder_widget_.reset();
  }
  views::Widget* browser_widget =
      browser->window()->AsBrowserView()->GetWidget();
  // Makes the glic widget a child view of the given widget's browser.
  if (browser_widget && glic_window_widget_ && browser) {
    // Add observer to new parent.
    attached_target_widget_observer_.SetAttachedTargetWidget(browser_widget);
    attached_browser_ = browser->AsWeakPtr();
    views::Widget::ReparentNativeView(glic_window_widget_->GetNativeView(),
                                      browser_widget->GetNativeView());
    NotifyIfPanelStateChanged();

    // When attached to a browser window, the glic widget mustn't float and when
    // interacted with must behave like any other widget.
    glic_window_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
#if BUILDFLAG(IS_MAC)
    glic_window_widget_->SetActivationIndependence(false);
#endif
    browser_close_subscription_ = browser->RegisterBrowserDidClose(
        base::BindRepeating(&GlicWindowController::AttachedBrowserDidClose,
                            base::Unretained(this)));
  }
}

bool GlicWindowController::Resize(const gfx::Size& size) {
  if (!glic_window_widget_) {
    return false;
  }
  // TODO(iwells): Set animation duration based on value set by client.
  window_resize_animation_ = std::make_unique<GlicWindowResizeAnimation>(
      glic_window_widget_.get(), size, /*duration=*/base::Milliseconds(0),
      base::BindOnce(&GlicWindowController::ResizeFinished, GetWeakPtr()));
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
  window_resize_animation_.reset();
  glic_widget_observer_.reset();
  window_event_observer_.reset();
  browser_close_subscription_.reset();
  glic_window_widget_->RemoveObserver(this);
  glic_window_widget_.reset();
  will_show_ = false;
  NotifyIfPanelStateChanged();

  if (web_client_) {
    // The webview is kept alive by default, no need to use this callback.
    web_client_->PanelWasClosed(base::DoNothing());
  }
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
    glic_window_widget_->RunMoveLoop(
        mouse_offset, move_loop_source,
        views::Widget::MoveLoopEscapeBehavior::kDontHide);
    in_move_loop_ = false;
    // Check whether `glic_window_widget_` is in a position to attach to a
    // browser window.
    HandleAttachmentToBrowserWindows(glic_window_widget_.get());
  }
}

void GlicWindowController::HandleAttachmentToBrowserWindows(
    views::Widget* widget) {
  // This should only ever be called after a move is completed.
  CHECK(!in_move_loop_);

  content::BrowserContext* glic_browser_context =
      GetGlicView()->web_view()->GetBrowserContext();

  // The profile must have started off as Glic enabled since a Glic widget is
  // open but it may have been disabled at runtime by policy. In this edge-case,
  // prevent reattaching back to a window (as it no longer has a GlicButton).
  if (!GlicEnabling::IsEnabledForProfile(
          Profile::FromBrowserContext(glic_browser_context))) {
    return;
  }
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

    gfx::Rect glic_button_rect =
        tab_strip_region_view->GetGlicButton()->GetBoundsInScreen();

    float corner_distance = (glic_button_rect.CenterPoint() -
                             widget->GetWindowBoundsInScreen().top_right())
                                .Length();
    // If there is no active drag (i.e. the previous drag has ended)
    // then determine whether the glic window should be attached or detached
    // from the browser window.
    if (corner_distance < kAttachmentDistanceThreshold) {
      AttachToBrowser(browser);
      return;
    }

    if (glic_window_widget_->parent() ==
        browser->window()->AsBrowserView()->GetWidget()) {
      // If farther than the attachment threshold from the current parent
      // widget, reparent under an empty holder widget.
      MaybeCreateHolderWindowAndReparent();
      return;
    }
  }
}

void GlicWindowController::MovePositionToBrowserGlicButton(Browser* browser,
                                                           bool animate) {
  if (!glic_window_widget_) {
    return;
  }

  // If the profile's been disabled (e.g. by policy) the window's Glic button
  // will be removed so we can't anchor to it. We could work around this by
  // keeping the button but disabling and making it invisible but this is an
  // edge-case, not sure it's worth the effort.
  if (!GlicEnabling::IsEnabledForProfile(browser->profile())) {
    return;
  }

  GlicButton* glic_button = browser->window()
                                ->AsBrowserView()
                                ->tab_strip_region_view()
                                ->GetGlicButton();
  CHECK(glic_button);

  attached_browser_ = browser->AsWeakPtr();
  gfx::Rect glic_rect = glic_window_widget_->GetWindowBoundsInScreen();
  // TODO(andreaxg): Fix exact attachment position.
  gfx::Rect glic_button_rect = glic_button->GetBoundsInScreen();
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
    holder_widget_->Init(std::move(params));
    holder_widget_->ShowInactive();
  }
  views::Widget::ReparentNativeView(glic_window_widget_->GetNativeView(),
                                    holder_widget_->GetNativeView());
  NotifyIfPanelStateChanged();

  // When the glic window is in a detached state, elevate its z-order to be
  // always on top. On the Mac, mark it as "activation independent" so that
  // interacting with it does not activate Chrome.
  glic_window_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
#if BUILDFLAG(IS_MAC)
  glic_window_widget_->SetActivationIndependence(true);
#endif
}

bool GlicWindowController::IsBrowserGlicCompatible(Browser* browser) {
  views::Widget* window_widget =
      browser->window()->AsBrowserView()->GetWidget();
  // A browser is not compatible if it:
  // - is from a glic-disabled profile
  // - is not visible
  // - is a glic-owned widget
  // - uses a different BrowserContext from glic
  if (!GlicEnabling::IsEnabledForProfile(browser->profile()) ||
      !browser->window()->IsVisible() ||
      window_widget == glic_window_widget_.get() ||
      window_widget == holder_widget_.get() ||
      browser->GetWebView()->GetBrowserContext() !=
          GetGlicView()->web_view()->GetBrowserContext()) {
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
  if (!glic_window_widget_visible_) {
    panel_state.kind = mojom::PanelState_Kind::kHidden;
  } else if (attached_browser_) {
    panel_state.kind = mojom::PanelState_Kind::kAttached;
    panel_state.window_id = attached_browser_->session_id().id();
  } else {
    panel_state.kind = mojom::PanelState_Kind::kDetached;
  }
  return panel_state;
}

void GlicWindowController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  // Store visibility locally because calling glic_window_widget_->IsVisible()
  // at this point returns the old value.
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

void GlicWindowController::Shutdown() {
  // Hide first, then clean up.
  Close();
  contents_.reset();
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
