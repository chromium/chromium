// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_controller.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/browser_conditions.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_fre_controller.h"
#include "chrome/browser/glic/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_widget.h"
#include "chrome/browser/glic/glic_window_animator.h"
#include "chrome/browser/glic/scoped_glic_button_indicator.h"
#include "chrome/browser/glic/webui_contents_container.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace glic {

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kGlicWidgetAttached);

namespace {
constexpr static int kDefaultDetachedTopRightDistance = 48;

// Default value for adding a buffer to the attachment zone.
constexpr static int kAttachmentBuffer = 20;
constexpr static int kDetachYDistance = 36;

constexpr static base::TimeDelta kAnimationDuration = base::Milliseconds(300);

constexpr char kHistogramGlicPanelPresentationTime[] =
    "Glic.PanelPresentationTime";

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

GlicButton* GetGlicButton(const Browser& browser) {
  return browser.window()
      ->AsBrowserView()
      ->tab_strip_region_view()
      ->GetGlicButton();
}

gfx::Size GetWidgetInitialSize() {
  return {features::kGlicInitialWidth.Get(),
          features::kGlicInitialHeight.Get()};
}

display::Display GetDisplayForOpeningDetached() {
  // Get the Display for the most recently active browser. If there was no
  // recently active browser, use the primary display.
  Browser* last_active_browser = BrowserList::GetInstance()->GetLastActive();
  if (last_active_browser) {
    std::optional<display::Display> widget_display =
        last_active_browser->GetBrowserView().GetWidget()->GetNearestDisplay();
    if (widget_display) {
      return *widget_display;
    }
  }
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}

}  // namespace

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
      initial_press_loc_ = mouse_location;
    }
    if (event.type() == ui::EventType::kMouseReleased &&
        event.AsMouseEvent()->IsRightMouseButton() &&
        mouse_down_in_draggable_area_) {
      glic_window_controller_->ShowTitleBarContextMenuAt(mouse_location);
    }
    if (event.type() == ui::EventType::kMouseReleased ||
        event.type() == ui::EventType::kMouseExited) {
      mouse_down_in_draggable_area_ = false;
      initial_press_loc_ = gfx::Point();
    }

    // Window should only be dragged if a corresponding mouse drag event was
    // initiated in the draggable area.
    if (mouse_down_in_draggable_area_ &&
        event.type() == ui::EventType::kMouseDragged &&
        glic_window_controller_->ShouldStartDrag(initial_press_loc_,
                                                 mouse_location)) {
      glic_window_controller_->HandleWindowDragWithOffset(
          mouse_location.OffsetFromOrigin());
    }
  }

 private:
  raw_ptr<glic::GlicWindowController> glic_window_controller_;
  raw_ptr<glic::GlicView> glic_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // Tracks whether the mouse is pressed and was initially within a draggable
  // area of the window.
  bool mouse_down_in_draggable_area_;

  // Tracks the initial kMousePressed location of a potential drag.
  gfx::Point initial_press_loc_;
};

// This class observes the view and widget the glic widget anchors to and
// notifies the controller whenever their bounds change.
class GlicWindowController::AnchorObserver : public views::ViewObserver,
                                             public views::WidgetObserver {
 public:
  AnchorObserver(GlicWindowController* controller, views::View* anchor_view)
      : controller_(controller) {
    view_observation_.Observe(anchor_view);
    CHECK(anchor_view->GetWidget());
    widget_observation_.Observe(anchor_view->GetWidget());
  }

 private:
  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* anchor_view) override {
    // This event is fired on entering and exiting mac fullscreen. The anchor
    // view will be moved from the browser view to an overlay widget, or the
    // other way around.
    widget_observation_.Observe(anchor_view->GetWidget());
  }

  void OnViewRemovedFromWidget(views::View* anchor_view) override {
    widget_observation_.Reset();
  }

  void OnViewBoundsChanged(views::View* anchor_view) override {
    CHECK(controller_->attached_browser());
    controller_->MovePositionToBrowserGlicButton(
        *controller_->attached_browser(),
        /*animate=*/false);
  }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* anchor_widget,
                             const gfx::Rect& bounds) override {
    CHECK(controller_->attached_browser());
    controller_->MovePositionToBrowserGlicButton(
        *controller_->attached_browser(),
        /*animate=*/false);
  }
  // No need to observe widget destroy because the observed view will be removed
  // from the widget and notifies this class.

  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  raw_ptr<GlicWindowController> controller_;
};

GlicWindowController::GlicWindowController(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* glic_service,
    GlicEnabling* enabling)
    : profile_(profile),
      fre_controller_(
          std::make_unique<GlicFreController>(profile, identity_manager)),
      window_finder_(std::make_unique<WindowFinder>()),
      glic_service_(glic_service),
      enabling_(enabling) {
  subscriptions_.push_back(enabling_->RegisterEnableChanged(base::BindRepeating(
      &GlicWindowController::EnableChanged, base::Unretained(this))));
}

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

void GlicWindowController::OnWidgetBoundsChanged(views::Widget* widget,
                                                 const gfx::Rect& new_bounds) {
  if (in_move_loop_) {
    // While in a move loop, look for nearby browsers to toggle the drop to
    // attach indicator.
    HandleGlicButtonIndicator();
  }
}

void GlicWindowController::Toggle(BrowserWindowInterface* bwi,
                                  bool prevent_close,
                                  InvocationSource source) {
  // If `bwi` is non-null, the glic button was clicked on a specific window and
  // glic should be attached to that window. Otherwise glic was invoked from the
  // hotkey or other OS-level entrypoint.
  Browser* new_attached_browser =
      bwi ? bwi->GetBrowserForMigrationOnly() : nullptr;

  // Show the FRE if not yet completed, and if we have a browser to use.
  if (fre_controller_->ShouldShowFreDialog()) {
    if (!fre_controller_->CanShowFreDialog(new_attached_browser)) {
      // If the FRE is blocked because it is already showing, we should instead
      // dismiss it. This allows the glic button to be used to toggle the
      // presence of the FRE.
      fre_controller_->DismissFreIfOpenOnActiveTab(new_attached_browser);
      return;
    }
    fre_controller_->ShowFreDialog(new_attached_browser);
    return;
  }

  mojom::PanelState panel_state = ComputePanelState();
  bool is_detached = panel_state.kind == mojom::PanelState_Kind::kDetached;

  // In the case where the user invokes the hotkey, or the status tray glic
  // icon and the most recently used window for the glic profile is active,
  // treat this as if the user clicked the glic button on that window if
  // Chrome is currently in the foreground and we aren't in detached state.
  if (!new_attached_browser && !is_detached) {
    Browser* last_active_browser = BrowserList::GetInstance()->GetLastActive();
    if (last_active_browser &&
        IsBrowserGlicAttachable(profile_, last_active_browser) &&
        IsBrowserInForeground(last_active_browser)) {
      new_attached_browser = last_active_browser;
    }
  }

  auto maybe_close = [this, prevent_close] {
    if (!prevent_close) {
      Close();
    }
  };

  // Pressing the button or the hotkey when the window is open, or waiting to
  // load should close it. The latter is required because otherwise if there
  // were an error loading the backend (or if it just took a long time) then the
  // button/hotkey would become unresponsive.
  //
  // In the future, when the WebUI can send its status back to the controller
  // via mojom, we could explicitly restrict the second case to loading,
  // offline, and error states.
  if (state_ == State::kOpen || state_ == State::kWaitingForGlicToLoad) {
    if (new_attached_browser) {
      if (new_attached_browser == attached_browser_) {
        // Button was clicked on same browser: close.
        // There are three ways for this to happen. Normally the glic window
        // obscures the glic button. Either the user used keyboard navigation to
        // click the glic button, the user clicked the button early and the
        // button click was eventually processed asynchronously after the button
        // was obscured, or the user invokes the glic hotkey while glic is
        // attached to the active window.
        maybe_close();
      } else {
        // Button clicked on a different browser: attach to that one.
        MovePositionToBrowserGlicButton(*new_attached_browser,
                                        /*animate=*/true);
        AttachToBrowser(*new_attached_browser);
      }
      return;
    }

    // Everything else in this block handles the case where the user invokes the
    // hotkey and the most recently used window from the glic profile is not
    // active.

    // Already attached?
    if (attached_browser_) {
      bool should_close = IsActive();
#if BUILDFLAG(IS_WIN)
      // On Windows, clicking the system tray icon de-activates the active
      // window, so fall back to checking if `attached_browser_` is visible for
      // both the hot key and system tray click cases.
      should_close = attached_browser_->window()
                         ->GetNativeWindow()
                         ->GetHost()
                         ->GetNativeWindowOcclusionState() ==
                     aura::Window::OcclusionState::VISIBLE;
#endif  // BUILDFLAG(IS_WIN)

      if (should_close) {
        // Hotkey when glic active and attached: close.
        maybe_close();
        return;
      }

      // Hotkey when glic is inactive and attached:
      if (attached_browser_->IsActive()) {
        // Hotkey when glic inactive but attached to active browser: close.
        // Note: this should not be possible, since if the attached browser is
        // active, new_attached_browser must not have been null.
        maybe_close();
      } else {
        // Hotkey when neither attached browser nor glic are active: open
        // detached.
        CloseAndReopenDetached(source);
      }
      return;
    }

    // Hotkey invoked when glic is already detached.
    maybe_close();

  } else if (state_ != State::kClosed) {
    // Currently in the process of showing the widget, allow that to finish.
    return;
  } else {
    Show(new_attached_browser, source);
  }
}

void GlicWindowController::ShowDetachedForTesting() {
  glic::GlicProfileManager::GetInstance()->SetActiveGlic(glic_service_);
  Show(nullptr, InvocationSource::kOsHotkey);
}

void GlicWindowController::WebUiStateChanged(mojom::WebUiState new_state) {
  base::UmaHistogramEnumeration("Glic.PanelWebUiState", new_state);
  if (webui_state_ != new_state) {
    // UI State has changed
    webui_state_ = new_state;
    webui_state_observers_.Notify(&WebUiStateObserver::WebUiStateChanged,
                                  webui_state_);
  }
}

void GlicWindowController::Show(Browser* browser, InvocationSource source) {
  // At this point State must be kClosed, and all glic window state must be
  // unset.
  CHECK(!attached_browser_);
  state_ = State::kOpenAnimation;
  glic_service_->metrics()->OnGlicWindowOpen(/*attached=*/browser, source);

  show_start_time_ = base::TimeTicks::Now();

  if (!contents_) {
    contents_ = std::make_unique<WebUIContentsContainer>(profile_, this);
  }
  glic_service_->NotifyWindowIntentToShow();
  glic_service_->GetAuthController().CheckAuthBeforeShow(
      base::BindOnce(&GlicWindowController::AuthCheckDoneBeforeShow,
                     GetWeakPtr(), browser ? browser->AsWeakPtr() : nullptr));
}

void GlicWindowController::AuthCheckDoneBeforeShow(
    base::WeakPtr<Browser> browser_for_attachment,
    AuthController::BeforeShowResult result) {
  switch (result) {
    case AuthController::BeforeShowResult::kShowingReauthSigninPage:
      state_ = State::kClosed;
      return;
    case AuthController::BeforeShowResult::kReady:
    case AuthController::BeforeShowResult::kSyncFailed:
      break;
  }

  // Since this method is called asynchronously, check that the profile wasn't
  // disabled since the request was made.
  if (!GlicEnabling::IsEnabledForProfile(profile_)) {
    state_ = State::kClosed;
    return;
  }

  glic_window_animator_ = std::make_unique<GlicWindowAnimator>(this);
  if (browser_for_attachment) {
    OpenAttached(*browser_for_attachment.get());
  } else {
    OpenDetached();
  }

  // Immediately hook up the WebView to the WebContents.
  GetGlicView()->web_view()->SetWebContents(contents_->web_contents());

  // TODO(sanaakbani): fade this in.
  GetGlicView()->web_view()->SetVisible(false);

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
  display::Display display = GetDisplayForOpeningDetached();
  gfx::Size widget_size = GetLastRequestedSizeClamped(display.size().height());

  // Right now this only detects whether the glic widget is summoned from the
  // OS entrypoint and positions itself detached from the browser.
  // TODO(crbug.com/384061064): Add more logic for when the glic window should
  // show up in a detached state.
  gfx::Point position = display.work_area().top_right();
  position.set_x(position.x() - widget_size.width() -
                 kDefaultDetachedTopRightDistance);
  // Start at y=0. The detached open animation changes this.
  return {position, widget_size};
}

void GlicWindowController::OpenAttached(Browser& browser) {
  GlicButton* glic_button = GetGlicButton(browser);
  CHECK(glic_button);

  // If summoned from the tab strip button. This will always show up attached
  // because it is tied to a views::View object within the current browser
  // window.
  gfx::Rect glic_window_widget_initial_rect = glic_button->GetBoundsWithInset();

  glic_widget_ = GlicWidget::Create(profile_, glic_window_widget_initial_rect);
  glic_widget_observation_.Observe(glic_widget_.get());

  glic_widget_->Show();
  AttachToBrowser(browser);

  // Set target size for animation and run the open attached animation.
  gfx::Size widget_size =
      GetLastRequestedSizeClamped(glic_widget_->GetDisplay().size().height());

  glic_window_animator_->RunOpenAttachedAnimation(
      glic_button, widget_size,
      base::BindOnce(&GlicWindowController::OpenAnimationFinished,
                     GetWeakPtr()));
}

void GlicWindowController::OpenDetached() {
  gfx::Rect initial_bounds = GetInitialDetachedBounds();

  // Make the widget.
  glic_widget_ = GlicWidget::Create(profile_, initial_bounds);
  glic_widget_observation_.Observe(glic_widget_.get());

  // Be sure to reparent the widget and set its state first before showing it.
  MaybeCreateHolderWindowAndReparent();
  GetGlicWidget()->Show();

  glic_window_animator_->RunOpenDetachedAnimation(base::BindOnce(
      &GlicWindowController::OpenAnimationFinished, GetWeakPtr()));
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

void GlicWindowController::GlicLoaded(mojom::OpenPanelInfoPtr open_info) {
  // TODO: Use `starting_mode` to log latency metrics.
  DVLOG(1) << "GlicLoaded with " << open_info->web_client_mode;
  starting_mode_ = open_info->web_client_mode;
  if (open_info->panelSize.has_value()) {
    Resize(*open_info->panelSize, open_info->resizeDuration, base::DoNothing());
  }

  glic_loaded_ = true;
  if (state_ == State::kWaitingForGlicToLoad) {
    ShowFinish();
  }
}

void GlicWindowController::OpenAnimationFinished() {
  if (state_ == State::kOpenAnimation) {
    state_ = State::kWaitingForGlicToLoad;

    // Note: this logic may never be called if state_ != kOpenAnimation when the
    // open animation is finished (or cancelled).
    // TODO(sanaakbani): fade this in.
    GetGlicView()->web_view()->SetVisible(true);

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
  // TODO(sanaakbani): fade this in.
  GetGlicView()->SetVisible(true);
  state_ = State::kOpen;

  // Record the presentation time of showing the glic panel in an UMA histogram.
  if (web_client_ && !show_start_time_.is_null()) {
    std::string input_mode;
    if (starting_mode_ == mojom::WebClientMode::kText) {
      input_mode = ".Text";
    } else if (starting_mode_ == mojom::WebClientMode::kAudio) {
      input_mode = ".Audio";
    }
    base::TimeDelta presentation_time =
        base::TimeTicks::Now() - show_start_time_;
    base::UmaHistogramCustomTimes(
        base::StrCat({kHistogramGlicPanelPresentationTime, ".All"}),
        presentation_time, base::Milliseconds(1), base::Seconds(60), 50);
    if (starting_mode_ != mojom::WebClientMode::kUnknown) {
      base::UmaHistogramCustomTimes(
          base::StrCat({kHistogramGlicPanelPresentationTime, input_mode}),
          presentation_time, base::Milliseconds(1), base::Seconds(60), 50);
    }
    ResetPresentationTimingState();
  }

  // Whenever the glic window is shown, it should have focus. The following line
  // of code appears to be necessary but not sufficient and there are still some
  // edge cases.
  // TODO(crbug.com/390637019): Fully fix and remove this comment.
  GetGlicView()->web_view()->GetWebContents()->Focus();

  window_event_observer_ =
      std::make_unique<WindowEventObserver>(this, GetGlicView());

  // Set the draggable area to the top bar of the window, by default.
  GetGlicView()->SetDraggableAreas(
      {{0, 0, GetGlicView()->width(), GetWidgetInitialSize().height()}});
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

content::WebContents* GlicWindowController::GetFreWebContents() {
  return fre_controller_->GetWebContents();
}

gfx::Point GlicWindowController::GetTopRightPositionForAttachedGlicWindow(
    GlicButton* glic_button) {
  // The widget should be placed so its top right corner matches the visible top
  // right corner of the glic button.
  return glic_button->GetBoundsWithInset().top_right();
}

void GlicWindowController::AttachedBrowserDidClose(
    BrowserWindowInterface* browser) {
  ForceClose();
}

void GlicWindowController::Attach() {
  if (!GetGlicWidget()) {
    return;
  }

  Browser* browser = glic::FindBrowserForAttachment(profile_);
  if (!browser) {
    return;
  }
  MovePositionToBrowserGlicButton(*browser, /*animate=*/true);
  AttachToBrowser(*browser);
}

void GlicWindowController::Detach() {
  if (state_ != State::kOpen || !attached_browser_) {
    return;
  }
  state_ = State::kDetaching;
  MaybeCreateHolderWindowAndReparent();

  // Move down a little bit when detaching.
  gfx::Point new_position = glic_widget_->GetWindowBoundsInScreen().origin();
  new_position.set_y(new_position.y() + kDetachYDistance);

  glic_window_animator_->AnimatePosition(
      new_position, kAnimationDuration,
      base::BindOnce(&GlicWindowController::DetachFinished, GetWeakPtr()));
}

void GlicWindowController::DetachFinished() {
  state_ = State::kOpen;
}

void GlicWindowController::AttachToBrowser(Browser& browser) {
  CHECK(GetGlicWidget());
  attached_browser_ = &browser;

  // TODO(crbug.com/395734073): Investigate reparenting to a holder widget on
  // Windows
#if !BUILDFLAG(IS_MAC)
  // Close the holder window.
  holder_widget_.reset();
#endif

  BrowserView* browser_view = browser.window()->AsBrowserView();
  CHECK(browser_view);
  // Although the glic widget is conceptually anchored to the glic button, we
  // intentionally observe its parent view, the tab strip region, for bounds
  // changes. This is because views bounds changed events when its *local*
  // bounds change. When the tab strip resizes, the glic button's local bounds
  // (relative to the tab strip) typically remain constant.
  views::View* anchor_view = browser_view->tab_strip_region_view();
  anchor_observer_ = std::make_unique<AnchorObserver>(this, anchor_view);

  // Makes the glic widget a child view of the anchor view's widget, which is
  // different from the browser widget in mac immersive fullscreen.
  glic_widget_->Reparent(anchor_view->GetWidget());
  if (!IsActive()) {
    GetGlicWidget()->Activate();
  }

  NotifyIfPanelStateChanged();

  // When attached to a browser window, the glic widget mustn't float and when
  // interacted with must behave like any other widget.
  GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kNormal);
#if BUILDFLAG(IS_MAC)
  GetGlicWidget()->SetActivationIndependence(false);
  GetGlicWidget()->SetVisibleOnAllWorkspaces(false);
#endif

  browser_close_subscription_ = browser.RegisterBrowserDidClose(
      base::BindRepeating(&GlicWindowController::AttachedBrowserDidClose,
                          base::Unretained(this)));

  // Trigger custom event for testing.
  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kGlicWidgetAttached, GetGlicButton(browser));
}

void GlicWindowController::Resize(const gfx::Size& size,
                                  base::TimeDelta duration,
                                  base::OnceClosure callback) {
  glic_size_ = size;

  // TODO(https://crbug.com/379164689): Drive resize animations for error states
  // from the browser. For now, we allow animations during the waiting state.
  // TOOD(https://crbug.com/392668958): If the widget is ready and asks for a
  // resize before the opening animation is finished, we will stop the current
  // animation and resize to the final size. Investigate a smoother way to
  // animate this transition.
  if (state_ == State::kOpen || state_ == State::kWaitingForGlicToLoad ||
      state_ == State::kOpenAnimation || state_ == State::kDetaching) {
    glic_window_animator_->AnimateSize(
        GetLastRequestedSizeClamped(glic_widget_->GetDisplay().size().height()),
        duration, std::move(callback));
  } else {
    // If the glic window is closed, or the widget isn't ready (e.g. because
    // it's currently still animating closed) immediately post the callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
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

  // The webview should be faded out instead.
  if (GetGlicView()) {
    GetGlicView()->web_view()->SetWebContents(nullptr);
  }

  if (attached_browser_) {
    state_ = State::kCloseAnimation;
    GlicButton* glic_button = GetGlicButton(*attached_browser_);
    glic_window_animator_->RunCloseAnimation(
        glic_button,
        base::BindOnce(&GlicWindowController::CloseFinish, GetWeakPtr(),
                       reopen_detached, closing_to_reopen_detached_source_));
  } else {
    CloseFinish(reopen_detached, closing_to_reopen_detached_source_);
  }
}

void GlicWindowController::CloseFinish(
    bool reopen_detached,
    std::optional<InvocationSource> reopen_detached_source) {
  if (state_ == State::kClosed) {
    return;
  }
  glic_window_animator_.reset();
  glic_service_->metrics()->OnGlicWindowClose();
  base::UmaHistogramEnumeration("Glic.PanelWebUiState.FinishState2",
                                webui_state_);

  state_ = State::kClosed;
  attached_browser_ = nullptr;
  anchor_observer_.reset();
  window_event_observer_.reset();
  browser_close_subscription_.reset();
  glic_widget_observation_.Reset();
  glic_widget_.reset();
  scoped_glic_button_indicator_.reset();
  NotifyIfPanelStateChanged();

  if (web_client_) {
    // The webview is kept alive by default, no need to use this callback.
    web_client_->PanelWasClosed(base::DoNothing());
  }

  if (reopen_detached) {
    Show(nullptr, *reopen_detached_source);
  }
}

void GlicWindowController::ForceClose() {
  CloseFinish(/*reopen_detached=*/false,
              /*reopen_detached_source=*/std::nullopt);
}

void GlicWindowController::CloseAndReopenDetached(InvocationSource source) {
  if (state_ != State::kOpen) {
    return;
  }

  state_ = State::kClosingToReopenDetached;
  closing_to_reopen_detached_source_ = source;
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

bool GlicWindowController::ShouldStartDrag(const gfx::Point& initial_press_loc,
                                           const gfx::Point& mouse_location) {
  // Determine if the mouse has moved beyond a minimum elasticity distance
  // in any direction from the starting point.
  static const int kMinimumDragDistance = 10;
  int x_offset = abs(mouse_location.x() - initial_press_loc.x());
  int y_offset = abs(mouse_location.y() - initial_press_loc.y());
  return sqrt(pow(static_cast<float>(x_offset), 2) +
              pow(static_cast<float>(y_offset), 2)) > kMinimumDragDistance;
}

void GlicWindowController::HandleWindowDragWithOffset(
    gfx::Vector2d mouse_offset) {
  // This code isn't set up to handle nested run loops. Nested run loops will
  // lead to crashes.
  if (!in_move_loop_) {
    in_move_loop_ = true;
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    // Set glic to a floating z-order while dragging so browsers brought into
    // focus by HandleGlicButtonIndicator won't show in front of glic.
    GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
#if BUILDFLAG(IS_MAC)
    // Mac: Make the glic widget a top-level widget before starting drag.
    glic_widget_->Reparent(nullptr);
#endif
    GetGlicWidget()->RunMoveLoop(
        mouse_offset, move_loop_source,
        views::Widget::MoveLoopEscapeBehavior::kDontHide);
    in_move_loop_ = false;
    // set glic z-order back to normal after drag is done.
    GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kNormal);
    scoped_glic_button_indicator_.reset();
    // Check whether `GetGlicWidget()` is in a position to attach to a
    // browser window.
    OnDragComplete();
  }
}

void GlicWindowController::OnDragComplete() {
  Browser* browser = FindBrowserForAttachment();
  // No browser within attachment range so maybe reparent under an empty holder
  // widget.
  if (!browser) {
    MaybeAdjustSizeForDisplay(/*animate=*/true);
    MaybeCreateHolderWindowAndReparent();
    return;
  }
  // Attach to the found browser.
  MovePositionToBrowserGlicButton(*browser, /*animate=*/true);
  AttachToBrowser(*browser);
}

void GlicWindowController::HandleGlicButtonIndicator() {
  Browser* browser = FindBrowserForAttachment();
  // No browser within attachment range so reset indicators
  if (!browser) {
    scoped_glic_button_indicator_.reset();
    return;
  }
  GlicButton* glic_button = GetGlicButton(*browser);
  // If there isn't an existing scoped indicator for this button, create one.
  if (!scoped_glic_button_indicator_ ||
      scoped_glic_button_indicator_->GetGlicButton() != glic_button) {
    // Bring the browser to the front.
    browser->GetBrowserView().GetWidget()->Activate();
    scoped_glic_button_indicator_ =
        std::make_unique<ScopedGlicButtonIndicator>(glic_button);
  }
}

Browser* GlicWindowController::FindBrowserForAttachment() {
  // The profile must have started off as Glic enabled since a Glic widget is
  // open but it may have been disabled at runtime by policy. In this edge-case,
  // prevent reattaching back to a window (as it no longer has a GlicButton).
  if (!GlicEnabling::IsEnabledForProfile(profile_)) {
    return nullptr;
  }

  gfx::Point glic_top_right =
      GetGlicWidget()->GetWindowBoundsInScreen().top_right();
  // Loops through all browsers in activation order with the latest accessed
  // browser first.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (!IsBrowserGlicAttachable(profile_, browser)) {
      continue;
    }

    // If the profile is enabled, the Glic button must be available.
    auto* tab_strip_region_view =
        browser->GetBrowserView().tab_strip_region_view();
    CHECK(tab_strip_region_view);
    CHECK(tab_strip_region_view->GetGlicButton());

    // Define attachment zone as the right of the tab strip. It either is the
    // width of the widget or 1/3 of the tab strip, whichever is smaller.
    gfx::Rect attachment_zone = tab_strip_region_view->GetBoundsInScreen();
    int width =
        std::min(attachment_zone.width() / 3, GetWidgetInitialSize().width());
    attachment_zone.SetByBounds(attachment_zone.right() - width,
                                attachment_zone.y() - kAttachmentBuffer,
                                attachment_zone.right() + kAttachmentBuffer,
                                attachment_zone.bottom());

    // If both the left center of the attachment zone and glic button right
    // center are occluded, don't consider for attachment.
    if (IsBrowserOccludedAtPoint(browser, attachment_zone.left_center()) &&
        IsBrowserOccludedAtPoint(browser, tab_strip_region_view->GetGlicButton()
                                              ->GetBoundsInScreen()
                                              .right_center())) {
      continue;
    }

    if (attachment_zone.Contains(glic_top_right)) {
      return browser;
    }
  }
  // No browser found near glic.
  return nullptr;
}

void GlicWindowController::MovePositionToBrowserGlicButton(
    const Browser& browser,
    bool animate) {
  if (!GetGlicWidget()) {
    return;
  }

  // If the profile's been disabled (e.g. by policy) the window's Glic button
  // will be removed so we can't anchor to it. We could work around this by
  // keeping the button but disabling and making it invisible but this is an
  // edge-case, not sure it's worth the effort.
  if (!GlicEnabling::IsEnabledForProfile(browser.profile())) {
    return;
  }

  // This will stack with a move animation below.
  MaybeAdjustSizeForDisplay(animate);

  GlicButton* glic_button = GetGlicButton(browser);
  CHECK(glic_button);
  gfx::Point top_right = GetTopRightPositionForAttachedGlicWindow(glic_button);
  gfx::Point target_position(
      top_right.x() - GetGlicWidget()->GetWindowBoundsInScreen().width(),
      top_right.y());

  // Avoid conversions between pixels and DIP on non 1.0 scale factor displays
  // changing widget width and height.
  base::TimeDelta duration =
      animate ? kAnimationDuration : base::Milliseconds(0);
  glic_window_animator_->AnimatePosition(target_position, duration,
                                         base::DoNothing());
  NotifyIfPanelStateChanged();
}

void GlicWindowController::MaybeCreateHolderWindowAndReparent() {
  attached_browser_ = nullptr;
  anchor_observer_.reset();
  browser_close_subscription_.reset();

// TODO(crbug.com/395734073): Investigate reparenting to a holder widget on
// Windows
#if !BUILDFLAG(IS_MAC)
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
#else  // BUILDFLAG(IS_MAC)
  // Mac: Make the glic widget a top-level widget
  glic_widget_->Reparent(nullptr);
#endif
  NotifyIfPanelStateChanged();

  // When the glic window is in a detached state, elevate its z-order to be
  // always on top. On the Mac, mark it as "activation independent" so that
  // interacting with it does not activate Chrome.
  GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
#if BUILDFLAG(IS_MAC)
  GetGlicWidget()->SetActivationIndependence(true);
  GetGlicWidget()->SetVisibleOnAllWorkspaces(true);
#endif
}

void GlicWindowController::AddStateObserver(StateObserver* observer) {
  state_observers_.AddObserver(observer);
}

void GlicWindowController::RemoveStateObserver(StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void GlicWindowController::AddWebUiStateObserver(WebUiStateObserver* observer) {
  webui_state_observers_.AddObserver(observer);
}

void GlicWindowController::RemoveWebUiStateObserver(
    WebUiStateObserver* observer) {
  webui_state_observers_.RemoveObserver(observer);
}

void GlicWindowController::NotifyIfPanelStateChanged() {
  auto new_state = ComputePanelState();
  if (new_state != panel_state_) {
    panel_state_ = new_state;
    state_observers_.Notify(&StateObserver::PanelStateChanged, panel_state_,
                            attached_browser_);
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
    contents_ = std::make_unique<WebUIContentsContainer>(profile_, this);
  }
}

void GlicWindowController::Reload() {
  if (GetFreWebContents()) {
    GetFreWebContents()->ReloadFocusedFrame();
  }
  if (contents_) {
    contents_->web_contents()->ReloadFocusedFrame();
  }
}

bool GlicWindowController::IsWarmed() {
  return !!contents_;
}

base::WeakPtr<GlicWindowController> GlicWindowController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GlicWindowController::Shutdown() {
  // Hide first, then clean up (but do not animate).
  ForceClose();
  contents_.reset();
  fre_controller_->Shutdown();
  window_activation_callback_list_.Notify(false);
}

void GlicWindowController::ResetPresentationTimingState() {
  show_start_time_ = base::TimeTicks();
  starting_mode_ = mojom::WebClientMode::kUnknown;
}

bool GlicWindowController::IsBrowserOccludedAtPoint(Browser* browser,
                                                    gfx::Point point) {
  std::set<gfx::NativeWindow> exclude = {
      GetGlicView()->GetWidget()->GetNativeWindow()};
  gfx::NativeWindow window =
      window_finder_->GetLocalProcessWindowAtPoint(point, exclude);
  if (browser->GetBrowserView().GetWidget()->GetNativeWindow() != window) {
    return true;
  }
  return false;
}

void GlicWindowController::EnableChanged() {
  // IsReadyForProfile can change at runtime for a few reasons, including
  // pausing the profile. For now, we just close the window in all cases.
  // Later this may be relaxed to just check for IsEnabled(), if we add new UX
  // to handle the various reasons glic is not ready.
  // See crbug.com/398909522.
  if (!enabling_->IsReadyForProfile(profile_)) {
    CloseFinish(/*reopen_detached=*/false, std::nullopt);
  }
}

gfx::Size GlicWindowController::GetLastRequestedSizeClamped(
    int display_height) const {
  gfx::Size min = GetWidgetInitialSize();
  gfx::Size max(
      min.width(),
      display_height * features::kGlicMaxHeightPercentOfScreen.Get() / 100);

  gfx::Size result = glic_size_ ? *glic_size_ : min;

  result.SetToMax(min);
  result.SetToMin(max);
  return result;
}

void GlicWindowController::MaybeAdjustSizeForDisplay(bool animate) {
  if (state_ == State::kOpen || state_ == State::kWaitingForGlicToLoad ||
      state_ == State::kOpenAnimation || state_ == State::kDetaching) {
    const auto target_size = GetLastRequestedSizeClamped(
        glic_widget_->GetWorkAreaBoundsInScreen().height());
    if (target_size != glic_window_animator_->GetCurrentTargetBounds().size()) {
      glic_window_animator_->AnimateSize(
          target_size, animate ? kAnimationDuration : base::Milliseconds(0),
          base::DoNothing());
    }
  }
}

}  // namespace glic
