// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_controller_impl.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/browser_ui/scoped_glic_button_indicator.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
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
#include "ui/base/win/event_creation_utils.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace glic {

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kGlicWidgetAttached);

namespace {

// Default value for adding a buffer to the attachment zone.
constexpr static int kAttachmentBuffer = 20;
constexpr static int kInitialPositionBuffer = 4;
constexpr static int kMaxWidgetSize = 16'384;

constexpr static base::TimeDelta kAnimationDuration = base::Milliseconds(300);

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

// True if |bounds| is an allowed position the Widget can be shown in.
bool IsWidgetLocationAllowed(const gfx::Rect& bounds) {
  const std::vector<display::Display>& displays =
      display::Screen::GetScreen()->GetAllDisplays();

  // Calculate inset corners to allow part of the widget to be off screen.
  std::array<gfx::Point, 4> inset_points = {
      // top-left: Allow 40% on left and |kInitialPositionBuffer| on top.
      gfx::Point(bounds.x() + bounds.width() * .4,
                 bounds.y() + kInitialPositionBuffer),
      // top-right: Allow 40% on right and |kInitialPositionBuffer| on top.
      gfx::Point(bounds.right() - bounds.width() * .4,
                 bounds.y() + kInitialPositionBuffer),
      // bottom-left: Allow 40% on left and 70% on bottom.
      gfx::Point(bounds.x() + bounds.width() * .4,
                 bounds.bottom() - bounds.height() * .7),
      // bottom-right: Allow 40% on right and 70% on bottom.
      gfx::Point(bounds.right() - bounds.width() * .4,
                 bounds.bottom() - bounds.height() * .7),
  };

  // Check that all four points are on an existing display.
  return std::ranges::all_of(inset_points, [&](gfx::Point p) {
    return display::FindDisplayContainingPoint(displays, p) != displays.end();
  });
}

std::optional<int> GetOptionalIntPreference(PrefService* prefs,
                                            std::string_view path) {
  const PrefService::Preference& pref =
      CHECK_DEREF(prefs->FindPreference(path));
  if (pref.IsDefaultValue()) {
    return std::nullopt;
  }
  return pref.GetValue()->GetInt();
}

// Get the previous position or none if the window has not been dragged before.
std::optional<gfx::Point> GetPreviousPositionFromPrefs(PrefService* prefs) {
  if (!prefs) {
    return std::nullopt;
  }

  auto x_pos = GetOptionalIntPreference(prefs, prefs::kGlicPreviousPositionX);
  auto y_pos = GetOptionalIntPreference(prefs, prefs::kGlicPreviousPositionY);

  if (!x_pos.has_value() || !y_pos.has_value()) {
    return std::nullopt;
  }
  return gfx::Point(x_pos.value(), y_pos.value());
}
}  // namespace

// Helper class for observing mouse and key events from native window.
class GlicWindowControllerImpl::WindowEventObserver : public ui::EventObserver {
 public:
  WindowEventObserver(glic::GlicWindowController* glic_window_controller,
                      glic::GlicView* glic_view)
      : glic_window_controller_(glic_window_controller), glic_view_(glic_view) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, glic_view->GetWidget()->GetNativeWindow(),
        {
            ui::EventType::kMousePressed,
            ui::EventType::kMouseReleased,
            ui::EventType::kMouseDragged,
            ui::EventType::kTouchReleased,
            ui::EventType::kTouchPressed,
            ui::EventType::kTouchMoved,
            ui::EventType::kTouchCancelled,
        });
  }

  WindowEventObserver(const WindowEventObserver&) = delete;
  WindowEventObserver& operator=(const WindowEventObserver&) = delete;
  ~WindowEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
#if BUILDFLAG(IS_WIN)
    if (event.IsTouchEvent()) {
      // If we get a touch event, send the corresponding mouse event so that
      // drag drop of the floaty window will work with touch screens. This is a
      // bit hacky; it would be better to have non client hit tests for the
      // draggable area return HT_CAPTION but that requires the web client to
      // set the draggable areas correctly, and not include the buttons in the
      // titlebar. See crbug.com/388000848.

      const ui::TouchEvent* touch_event = event.AsTouchEvent();
      gfx::Point touch_location = touch_event->location();
      auto touch_screen_point =
          views::View::ConvertPointToScreen(glic_view_, touch_location);
      auto* host = glic_view_->GetWidget()->GetNativeWindow()->GetHost();

      host->ConvertDIPToPixels(&touch_screen_point);
      if (event.type() == ui::EventType::kTouchPressed) {
        POINT cursor_location = touch_screen_point.ToPOINT();
        ::SetCursorPos(cursor_location.x, cursor_location.y);
        touch_down_in_draggable_area_ =
            glic_view_->IsPointWithinDraggableArea(touch_location);
        if (touch_down_in_draggable_area_) {
          ui::SendMouseEvent(touch_screen_point, MOUSEEVENTF_LEFTDOWN);
          ui::SendMouseEvent(touch_screen_point, MOUSEEVENTF_MOVE);
        }
      }
      if (!touch_down_in_draggable_area_) {
        // If we're not in a potential touch drag of the window, ignore touch
        // events.
        return;
      }
      if (event.type() == ui::EventType::kTouchCancelled ||
          event.type() == ui::EventType::kTouchReleased) {
        touch_down_in_draggable_area_ = false;
        ui::SendMouseEvent(touch_screen_point, MOUSEEVENTF_LEFTUP);
      }
      if (event.type() == ui::EventType::kTouchMoved) {
        ui::SendMouseEvent(touch_screen_point, MOUSEEVENTF_MOVE);
      }
      return;
    }
#endif  // BUILDFLAG(IS_WIN)

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
          initial_press_loc_.OffsetFromOrigin());
    }
  }

 private:
  raw_ptr<glic::GlicWindowController> glic_window_controller_;
  raw_ptr<glic::GlicView> glic_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // Tracks whether the mouse is pressed and was initially within a draggable
  // area of the window.
  bool mouse_down_in_draggable_area_ = false;

#if BUILDFLAG(IS_WIN)
  // Tracks whether a touch pressed event occurred within the draggable area. If
  // so, subsequent touch events will trigger corresponding mouse events so that
  // window drag works.
  bool touch_down_in_draggable_area_ = false;
#endif  // BUILDFLAG(IS_WIN)

  // Tracks the initial kMousePressed location of a potential drag.
  gfx::Point initial_press_loc_;
};

GlicWindowControllerImpl::GlicWindowControllerImpl(
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
  previous_position_ = GetPreviousPositionFromPrefs(profile_->GetPrefs());
  application_hotkey_manager_ = MakeApplicationHotkeyManager(GetWeakPtr());
  host_observation_.Observe(&glic_service_->host());
}

GlicWindowControllerImpl::~GlicWindowControllerImpl() = default;

void GlicWindowControllerImpl::WebClientInitializeFailed() {
  if (state_ == State::kWaitingForGlicToLoad) {
    // TODO(crbug.com/388328847): The web client failed to initialize. Decide
    // what the fallback behavior is. Additionally, we probably need some kind
    // of timeout and/or loading indicator if loading takes too much time. For
    // now, show the UI anyway, which should be helpful in development.
    LOG(ERROR)
        << "Glic web client failed to initialize, it won't work properly.";
    glic_service_->metrics()->set_show_start_time(base::TimeTicks());
    GlicLoadedAndReadyToDisplay();
  }
}

void GlicWindowControllerImpl::LoginPageCommitted() {
  login_page_committed_ = true;
  if (state_ == State::kWaitingForGlicToLoad && !host().IsReady()) {
    // TODO(crbug.com/388328847): Temporarily allow showing the UI when a login
    // page is reached.
    glic_service_->metrics()->set_show_start_time(base::TimeTicks());
    GlicLoadedAndReadyToDisplay();
  }
}

// Monitoring the glic widget.
void GlicWindowControllerImpl::OnWidgetActivationChanged(views::Widget* widget,
                                                         bool active) {
  if (GetGlicWidget() != widget) {
    return;
  }
  if (!active && do_focus_loss_announcement_) {
    widget->widget_delegate()->SetAccessibleTitle(
        l10n_util::GetStringUTF16(IDS_GLIC_WINDOW_TITLE));
    GetGlicView()->GetViewAccessibility().AnnounceAlert(
        l10n_util::GetStringFUTF16(
            IDS_GLIC_WINDOW_FIRST_FOCUS_LOST_ANNOUNCEMENT,
            LocalHotkeyManager::GetConfigurableAccelerator(
                LocalHotkeyManager::Hotkey::kFocusToggle)
                .GetShortcutText()));
    do_focus_loss_announcement_ = false;
  }
  window_activation_callback_list_.Notify(active);
}

// Monitoring the glic widget.
void GlicWindowControllerImpl::OnWidgetDestroyed(views::Widget* widget) {
  // This is used to handle the case where the native window is closed
  // directly (e.g., Windows context menu close on the title bar).
  // Conceptually this should synchronously call Close(), but the Widget
  // implementation currently does not support this.
  if (GetGlicWidget() == widget) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GlicWindowControllerImpl::Close,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void GlicWindowControllerImpl::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (in_move_loop_ && !AlwaysDetached()) {
    // While in a move loop, look for nearby browsers to toggle the drop to
    // attach indicator.
    HandleGlicButtonIndicator();
  }

  modal_dialog_host_observers_.Notify(
      &web_modal::ModalDialogHostObserver::OnPositionRequiresUpdate);
}

void GlicWindowControllerImpl::OnWidgetUserResizeStarted() {
  user_resizing_ = true;
  glic_service_->metrics()->OnWidgetUserResizeStarted();
  if (GlicWebClientAccess* client = host().GetPrimaryWebClient()) {
    client->ManualResizeChanged(true);
  }
}

void GlicWindowControllerImpl::OnWidgetUserResizeEnded() {
  glic_service_->metrics()->OnWidgetUserResizeEnded();
  if (GlicWebClientAccess* client = host().GetPrimaryWebClient()) {
    client->ManualResizeChanged(false);
  }

  if (GetGlicView()) {
    GetGlicView()->UpdatePrimaryDraggableAreaOnResize();
  }

  if (GetGlicWidget()) {
    glic_size_ = GetGlicWidget()->GetSize();
  }

  glic_window_animator_->ResetLastTargetSize();
  user_resizing_ = false;
}

void GlicWindowControllerImpl::ShowAfterSignIn(base::WeakPtr<Browser> browser) {
  Toggle(browser.get(), true,
         // Prefer the source that triggered the sign-in, but if that's not
         // available, report it as coming from the sign-in flow.
         opening_source_.value_or(mojom::InvocationSource::kAfterSignIn));
}

void GlicWindowControllerImpl::Toggle(BrowserWindowInterface* bwi,
                                      bool prevent_close,
                                      mojom::InvocationSource source) {
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

  if (!AlwaysDetached()) {
    ToggleWhenNotAlwaysDetached(new_attached_browser, prevent_close, source);
    return;
  }

  auto maybe_close = [this, prevent_close] {
    if (!prevent_close) {
      Close();
    }
  };
  // If floaty is closed, open floaty
  if (state_ == State::kClosed) {
    Show(new_attached_browser, source);
    return;
  }

#if BUILDFLAG(IS_WIN)
  // Clicking status tray on Windows makes floaty not active so always close.
  if (source == mojom::InvocationSource::kOsButton) {
    Close();
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  // If floaty is focused or the source is the top button, close it.
  // If floaty is unfocused and open, focus it.
  if (IsActive() ||
      (source == mojom::InvocationSource::kTopChromeButton &&
       !base::FeatureList::IsEnabled(features::kGlicZOrderChanges))) {
    maybe_close();
  } else if (state_ == State::kOpen) {
    // TODO(crbug.com/404601783): Bring focus to the textbox.
    GetGlicWidget()->Activate();
    GetGlicView()->GetWebContents()->Focus();
  }
}

void GlicWindowControllerImpl::ToggleWhenNotAlwaysDetached(
    Browser* new_attached_browser,
    bool prevent_close,
    mojom::InvocationSource source) {
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
        AttachToBrowser(*new_attached_browser, AttachChangeReason::kInit);
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

void GlicWindowControllerImpl::FocusIfOpen() {
  if (IsShowing() && !IsActive()) {
    GetGlicWidget()->Activate();
    GetGlicView()->GetWebContents()->Focus();
  }
}

void GlicWindowControllerImpl::ShowDetachedForTesting() {
  glic::GlicProfileManager::GetInstance()->SetActiveGlic(glic_service_);
  Show(nullptr, mojom::InvocationSource::kOsHotkey);
}

void GlicWindowControllerImpl::SetPreviousPositionForTesting(
    gfx::Point position) {
  previous_position_ = position;
}

Host& GlicWindowControllerImpl::host() const {
  return glic_service_->host();
}

void GlicWindowControllerImpl::Show(Browser* browser,
                                    mojom::InvocationSource source) {
  // At this point State must be kClosed, and all glic window state must be
  // unset.
  CHECK(!attached_browser_);
  opening_source_ = source;
  if (!glic_service_->GetAuthController().CheckAuthBeforeShowSync(
          base::BindOnce(&GlicWindowControllerImpl::ShowAfterSignIn,
                         weak_ptr_factory_.GetWeakPtr(),
                         browser ? browser->AsWeakPtr() : nullptr))) {
    return;
  }

  glic_window_animator_ = std::make_unique<GlicWindowAnimator>(this);
  SetWindowState(State::kWaitingForGlicToLoad);

  glic_service_->metrics()->OnGlicWindowOpen(/*attached=*/browser, source);
  glic_service_->GetAuthController().OnGlicWindowOpened();

  glic_service_->metrics()->set_show_start_time(base::TimeTicks::Now());

  host().CreateContents();
  host().NotifyWindowIntentToShow();

  SetupGlicWidget(browser);

  // Notify the web client that the panel will open, and wait for the response
  // to actually show the window. Note that we have to call
  // `NotifyIfPanelStateChanged()` first, so that the host will receive the
  // correct panel state.
  NotifyIfPanelStateChanged();
  host().PanelWillOpen(source);

  if (login_page_committed_) {
    // This indicates that we've warmed the web client and it has hit a login
    // page. See LoginPageCommitted.
    GlicLoadedAndReadyToDisplay();
  } else {
    // This adds dragging functionality to special case panels (e.g. error,
    // offline, loading).
    SetDraggingAreasAndWatchForMouseEvents();
  }
  glic_service_->metrics()->OnGlicWindowShown();
}

void GlicWindowControllerImpl::SetupGlicWidget(Browser* browser) {
  auto initial_bounds = GetInitialBounds(browser);
  glic_window_hotkey_manager_ = MakeGlicWindowHotkeyManager(GetWeakPtr());
  glic_widget_ = GlicWidget::Create(profile_, initial_bounds,
                                    glic_window_hotkey_manager_->GetWeakPtr(),
                                    user_resizable_);
  glic_widget_observation_.Observe(glic_widget_.get());
  SetupGlicWidgetAccessibilityText();
  glic_window_hotkey_manager_->InitializeAccelerators();

  if (AlwaysDetached()) {
    SetGlicWindowToFloatingMode(true);
  } else if (!browser) {
    // Detached window when no always detach. Be sure to set its state first
    // before showing it.
    // TODO(crbug.com/410629338): Reimplement attachment.
  }

  glic_widget_->Show();

  // This is needed in case of theme difference between OS and chrome.
  GetGlicWidget()->ThemeChanged();

  if (browser && !AlwaysDetached()) {
    // Attached window if needed.
    AttachToBrowser(*browser, AttachChangeReason::kInit);
  }

  // This is used to handle the case where the native window is closed
  // directly (e.g., Windows context menu close on the title bar). It fixes the
  // bug where the window position was not restored after closing with the
  // context menu close menu item.
  GetGlicWidget()->MakeCloseSynchronous(base::BindOnce(
      &GlicWindowControllerImpl::CloseWithReason, base::Unretained(this)));

  // Immediately hook up the WebView to the WebContents.
  GetGlicView()->SetWebContents(host().webui_contents());
  GetGlicView()->UpdateBackgroundColor();

  // Add capability to show web modal dialogs (e.g. Data Controls Dialogs for
  // enterprise users) via constrained_window APIs.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      host().webui_contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      host().webui_contents())
      ->SetDelegate(this);
}

void GlicWindowControllerImpl::SetupGlicWidgetAccessibilityText() {
  auto* widget_delegate = glic_widget_->widget_delegate();
  if (opening_source_ == mojom::InvocationSource::kFre) {
    widget_delegate->SetAccessibleTitle(l10n_util::GetStringFUTF16(
        IDS_GLIC_WINDOW_TITLE_FIRST_LOAD,
        LocalHotkeyManager::GetConfigurableAccelerator(
            LocalHotkeyManager::Hotkey::kFocusToggle)
            .GetShortcutText()));
    do_focus_loss_announcement_ = true;
  } else {
    widget_delegate->SetAccessibleTitle(
        l10n_util::GetStringUTF16(IDS_GLIC_WINDOW_TITLE));
  }
}

void GlicWindowControllerImpl::SetGlicWindowToFloatingMode(bool floating) {
  GetGlicWidget()->SetZOrderLevel(floating ? ui::ZOrderLevel::kFloatingWindow
                                           : ui::ZOrderLevel::kNormal);
#if BUILDFLAG(IS_MAC)
  GetGlicWidget()->SetActivationIndependence(floating);
  GetGlicWidget()->SetVisibleOnAllWorkspaces(floating);
  GetGlicWidget()->SetCanAppearInExistingFullscreenSpaces(floating);
#endif
}

gfx::Rect GlicWindowControllerImpl::GetInitialBounds(Browser* browser) {
  if (browser && !AlwaysDetached()) {
    return GetInitialAttachedBounds(*browser);
  }
  gfx::Size target_size = GetLastRequestedSizeClamped();

  // Reset previous position if it results in an invalid location.
  if (previous_position_.has_value() &&
      !IsWidgetLocationAllowed({previous_position_.value(), target_size})) {
    previous_position_.reset();
  }
  // Use the previous position if there is one.
  if (previous_position_.has_value()) {
    return {previous_position_.value(), target_size};
  }

  std::optional<gfx::Rect> bounds_with_browser =
      GetInitialDetachedBoundsFromBrowser(browser, target_size);
  return bounds_with_browser.value_or(
      GetInitialDetachedBoundsNoBrowser(target_size));
}

gfx::Rect GlicWindowControllerImpl::GetInitialDetachedBoundsNoBrowser(
    const gfx::Size& target_size) {
  // Get the default position offset equal distances from the top right corner
  // of the work area (which excludes system UI such as the taskbar).
  display::Display display = GetDisplayForOpeningDetached();
  gfx::Point top_right = display.work_area().top_right();
  int initial_x =
      top_right.x() - target_size.width() - kDefaultDetachedTopRightDistance;
  int initial_y = top_right.y() + kDefaultDetachedTopRightDistance;
  return {{initial_x, initial_y}, target_size};
}

gfx::Rect GlicWindowControllerImpl::GetInitialAttachedBounds(Browser& browser) {
  GlicButton* glic_button = GetGlicButton(browser);
  CHECK(glic_button);

  // If summoned from the tab strip button. This will always show up attached
  // because it is tied to a views::View object within the current browser
  // window.
  gfx::Rect glic_window_widget_initial_rect = glic_button->GetBoundsWithInset();

  // Ensure that we start with a non-empty rect (see DCHECK in
  // NativeWidgetNSWindowBridge::SetInitialBounds).
  if (glic_window_widget_initial_rect.IsEmpty()) {
    glic_window_widget_initial_rect.set_width(
        std::max(glic_window_widget_initial_rect.width(), 1));
    glic_window_widget_initial_rect.set_height(
        std::max(glic_window_widget_initial_rect.height(), 1));
  }
  return glic_window_widget_initial_rect;
}

std::optional<gfx::Rect>
GlicWindowControllerImpl::GetInitialDetachedBoundsFromBrowser(
    Browser* browser,
    const gfx::Size& target_size) {
  if (!browser) {
    return std::nullopt;
  }

  // Set the origin so the top right of glic meets the bottom left of the glic
  // button.
  GlicButton* glic_button = GetGlicButton(*browser);
  CHECK(glic_button);
  gfx::Rect glic_button_inset_bounds = glic_button->GetBoundsWithInset();

  gfx::Point origin(glic_button_inset_bounds.x() - target_size.width() -
                        kInitialPositionBuffer,
                    glic_button_inset_bounds.bottom() + kInitialPositionBuffer);
  gfx::Rect bounds = {origin, target_size};

  return IsWidgetLocationAllowed(bounds) ? std::make_optional(bounds)
                                         : std::nullopt;
}

void GlicWindowControllerImpl::ClientReadyToShow(
    const mojom::OpenPanelInfo& open_info) {
  DVLOG(1) << "Glic client ready to show " << open_info.web_client_mode;
  glic_service_->metrics()->set_starting_mode(open_info.web_client_mode);
  glic_service_->metrics()->OnGlicWindowOpenAndReady();
  if (open_info.panelSize.has_value()) {
    Resize(*open_info.panelSize, open_info.resizeDuration, base::DoNothing());
  }
  EnableDragResize(open_info.can_user_resize);

  if (state_ == State::kWaitingForGlicToLoad) {
    GlicLoadedAndReadyToDisplay();
  }
}

void GlicWindowControllerImpl::GlicLoadedAndReadyToDisplay() {
  login_page_committed_ = false;
  if (state_ == State::kClosed || state_ == State::kOpen) {
    return;
  }

  // Update the background color after showing the webview so the transition
  // isn't visible. This will be the widget background color the user sees next
  // time.
  GetGlicView()->UpdateBackgroundColor();

  // In the case that the open animation was skipped, the web view should still
  // be visible now.
  SetWindowState(State::kOpen);

  // Whenever the glic window is shown, it should have focus. The following line
  // of code appears to be necessary but not sufficient and there are still some
  // edge cases.
  // TODO(crbug.com/390637019): Fully fix and remove this comment.
  GetGlicView()->GetWebContents()->Focus();

  SetDraggingAreasAndWatchForMouseEvents();
  NotifyIfPanelStateChanged();
}

void GlicWindowControllerImpl::SetDraggingAreasAndWatchForMouseEvents() {
  if (window_event_observer_) {
    return;
  }

  window_event_observer_ =
      std::make_unique<WindowEventObserver>(this, GetGlicView());

  // Set the draggable area to the top bar of the window, by default.
  GetGlicView()->SetDraggableAreas(
      {{0, 0, GetGlicView()->width(), GlicWidget::GetInitialSize().height()}});
}

GlicView* GlicWindowControllerImpl::GetGlicView() {
  if (!GetGlicWidget()) {
    return nullptr;
  }
  return static_cast<GlicView*>(GetGlicWidget()->GetContentsView());
}

base::WeakPtr<views::View> GlicWindowControllerImpl::GetGlicViewAsView() {
  if (auto* view = GetGlicView()) {
    return view->GetWeakPtr();
  }
  return nullptr;
}

GlicWidget* GlicWindowControllerImpl::GetGlicWidget() {
  return glic_widget_.get();
}

content::WebContents* GlicWindowControllerImpl::GetFreWebContents() {
  return fre_controller_->GetWebContents();
}

gfx::Point GlicWindowControllerImpl::GetTopRightPositionForAttachedGlicWindow(
    GlicButton* glic_button) {
  // The widget should be placed so its top right corner matches the visible top
  // right corner of the glic button.
  return glic_button->GetBoundsWithInset().top_right();
}

void GlicWindowControllerImpl::AttachedBrowserDidClose(
    BrowserWindowInterface* browser) {
  Close();
}

void GlicWindowControllerImpl::Attach() {
  if (!GetGlicWidget()) {
    return;
  }

  Browser* browser = glic::FindBrowserForAttachment(profile_);
  if (!browser) {
    return;
  }
  MovePositionToBrowserGlicButton(*browser, /*animate=*/true);
  if (AlwaysDetached()) {
    return;
  }
  AttachToBrowser(*browser, AttachChangeReason::kMenu);
}

void GlicWindowControllerImpl::Detach() {
  if (state_ != State::kOpen || !attached_browser_) {
    return;
  }

  if (!AlwaysDetached()) {
    // TODO(crbug.com/410629338): Reimplement attachment.
  }

  NOTIMPLEMENTED();
}

void GlicWindowControllerImpl::DetachFinished() {
  SetWindowState(State::kOpen);
}

void GlicWindowControllerImpl::AttachToBrowser(Browser& browser,
                                               AttachChangeReason reason) {
  CHECK(!AlwaysDetached());
  CHECK(GetGlicWidget());
  attached_browser_ = &browser;

  glic_service_->metrics()->OnAttachedToBrowser(reason);

  BrowserView* browser_view = browser.window()->AsBrowserView();
  CHECK(browser_view);

  if (!IsActive()) {
    GetGlicWidget()->Activate();
  }

  NotifyIfPanelStateChanged();

  SetGlicWindowToFloatingMode(false);

  browser_close_subscription_ = browser.RegisterBrowserDidClose(
      base::BindRepeating(&GlicWindowControllerImpl::AttachedBrowserDidClose,
                          base::Unretained(this)));

  // Trigger custom event for testing.
  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kGlicWidgetAttached, GetGlicButton(browser));
}

void GlicWindowControllerImpl::Resize(const gfx::Size& size,
                                      base::TimeDelta duration,
                                      base::OnceClosure callback) {
  glic_size_ = size;
  glic_service_->metrics()->OnGlicWindowResize();

  const bool in_resizable_state =
      state_ == State::kOpen || state_ == State::kWaitingForGlicToLoad;

  // TODO(https://crbug.com/379164689): Drive resize animations for error states
  // from the browser. For now, we allow animations during the waiting state.
  // TODO(https://crbug.com/392668958): If the widget is ready and asks for a
  // resize before the opening animation is finished, we will stop the current
  // animation and resize to the final size. Investigate a smoother way to
  // animate this transition.
  if (in_resizable_state && !user_resizing_) {
    glic_window_animator_->AnimateSize(GetLastRequestedSizeClamped(), duration,
                                       std::move(callback));
  } else {
    // If the glic window is closed, or the widget isn't ready (e.g. because
    // it's currently still animating closed) immediately post the callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
}

void GlicWindowControllerImpl::EnableDragResize(bool enabled) {
  user_resizable_ = enabled;
  if (!GetGlicWidget()) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kGlicZOrderChanges)) {
    // Drag-resizability implies text mode, which isn't floating, while
    // non-resizability implies audio mode, which is floating.
    SetGlicWindowToFloatingMode(!enabled);
  }

  GetGlicWidget()->widget_delegate()->SetCanResize(enabled);
  GetGlicView()->UpdateBackgroundColor();
  glic_window_animator_->MaybeAnimateToTargetSize();
}

gfx::Size GlicWindowControllerImpl::GetSize() {
  if (!GetGlicWidget()) {
    return gfx::Size();
  }

  return GetGlicWidget()->GetSize();
}

void GlicWindowControllerImpl::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  GlicView* glic_view = GetGlicView();
  if (!glic_view) {
    return;
  }

  glic_view->SetDraggableAreas(draggable_areas);
}

void GlicWindowControllerImpl::SetMinimumWidgetSize(const gfx::Size& size) {
  if (!GetGlicWidget()) {
    return;
  }

  glic_widget_->SetMinimumSize(size);
}

void GlicWindowControllerImpl::CloseWithReason(
    views::Widget::ClosedReason reason) {
  Close();
}

void GlicWindowControllerImpl::Close() {
  if (state_ == State::kClosed) {
    return;
  }

  // Save the widge position on close so we can restore in the same position.
  SaveWidgetPosition();

  glic_window_animator_.reset();
  glic_service_->metrics()->OnGlicWindowClose();
  base::UmaHistogramEnumeration("Glic.PanelWebUiState.FinishState2",
                                host().GetPrimaryWebUiState());

  SetWindowState(State::kClosed);
  attached_browser_ = nullptr;
  window_event_observer_.reset();
  browser_close_subscription_.reset();
  glic_window_hotkey_manager_.reset();
  glic_widget_observation_.Reset();
  glic_widget_.reset();
  scoped_glic_button_indicator_.reset();
  user_resizing_ = false;
  NotifyIfPanelStateChanged();
  window_activation_callback_list_.Notify(false);

  modal_dialog_host_observers_.Notify(
      &web_modal::ModalDialogHostObserver::OnHostDestroying);
  web_modal::WebContentsModalDialogManager::FromWebContents(
      host().webui_contents())
      ->SetDelegate(nullptr);

  host().PanelWasClosed();
  if (base::FeatureList::IsEnabled(features::kGlicUnloadOnClose)) {
    host().Shutdown();
  }
}

void GlicWindowControllerImpl::SaveWidgetPosition() {
  if (GetGlicWidget() && GetGlicWidget()->IsVisible()) {
    previous_position_ =
        GetGlicWidget()->GetClientAreaBoundsInScreen().origin();
    profile_->GetPrefs()->SetInteger(prefs::kGlicPreviousPositionX,
                                     previous_position_->x());
    profile_->GetPrefs()->SetInteger(prefs::kGlicPreviousPositionY,
                                     previous_position_->y());
  }
}

void GlicWindowControllerImpl::ShowTitleBarContextMenuAt(gfx::Point event_loc) {
#if BUILDFLAG(IS_WIN)
  views::View::ConvertPointToScreen(GetGlicView(), &event_loc);
  event_loc = display::win::GetScreenWin()->DIPToScreenPoint(event_loc);
  views::ShowSystemMenuAtScreenPixelLocation(views::HWNDForView(GetGlicView()),
                                             event_loc);
#endif  // BUILDFLAG(IS_WIN)
}

bool GlicWindowControllerImpl::ShouldStartDrag(
    const gfx::Point& initial_press_loc,
    const gfx::Point& mouse_location) {
  // Determine if the mouse has moved beyond a minimum elasticity distance
  // in any direction from the starting point.
  static const int kMinimumDragDistance = 10;
  int x_offset = abs(mouse_location.x() - initial_press_loc.x());
  int y_offset = abs(mouse_location.y() - initial_press_loc.y());
  return sqrt(pow(static_cast<float>(x_offset), 2) +
              pow(static_cast<float>(y_offset), 2)) > kMinimumDragDistance;
}

void GlicWindowControllerImpl::HandleWindowDragWithOffset(
    gfx::Vector2d mouse_offset) {
  // This code isn't set up to handle nested run loops. Nested run loops will
  // lead to crashes.
  if (!in_move_loop_) {
    in_move_loop_ = true;
#if BUILDFLAG(IS_MAC)
    GetGlicWidget()->SetCapture(nullptr);
#endif
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    if (!AlwaysDetached()) {
      // Set glic to a floating z-order while dragging so browsers brought into
      // focus by HandleGlicButtonIndicator won't show in front of glic.
      GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
    }
    GetGlicWidget()->RunMoveLoop(
        mouse_offset, move_loop_source,
        views::Widget::MoveLoopEscapeBehavior::kDontHide);
    in_move_loop_ = false;
    scoped_glic_button_indicator_.reset();

    // Only handle positioning if glic wasn't closed during the drag.
    if (state_ == State::kClosed) {
      return;
    }
    // Dragging stops animations. This makes sure we honor the last resize
    // request.
    glic_window_animator_->MaybeAnimateToTargetSize();

    AdjustPositionIfNeeded();
    SaveWidgetPosition();

    if (!AlwaysDetached()) {
      // set glic z-order back to normal after drag is done.
      GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kNormal);
      // Check whether `GetGlicWidget()` is in a position to attach to a
      // browser window.
      OnDragComplete();
    }
  }
}

const mojom::PanelState& GlicWindowControllerImpl::GetPanelState() const {
  return panel_state_;
}

void GlicWindowControllerImpl::AdjustPositionIfNeeded() {
  // Always have at least `kMinimumVisible` px visible from glic window in
  // both vertical and horizontal directions.
  constexpr int kMinimumVisible = 40;
  const auto widget_size = GetGlicWidget()->GetSize();
  const int horizontal_buffer = widget_size.width() - kMinimumVisible;
  const int vertical_buffer = widget_size.height() - kMinimumVisible;

  // Adjust bounds of visible area screen to allow part of glic to go off
  // screen.
  auto workarea = GetGlicWidget()->GetWorkAreaBoundsInScreen();
  workarea.Outset(gfx::Outsets::VH(vertical_buffer, horizontal_buffer));

  auto rect = GetGlicWidget()->GetRestoredBounds();
  rect.AdjustToFit(workarea);
  GetGlicWidget()->SetBounds(rect);
}

void GlicWindowControllerImpl::OnDragComplete() {
  Browser* browser = FindBrowserForAttachment();
  // No browser within attachment range.
  if (!browser) {
    return;
  }
  // Attach to the found browser.
  MovePositionToBrowserGlicButton(*browser, /*animate=*/true);
  AttachToBrowser(*browser, AttachChangeReason::kDrag);
}

void GlicWindowControllerImpl::HandleGlicButtonIndicator() {
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

Browser* GlicWindowControllerImpl::FindBrowserForAttachment() {
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
    int width = std::min(attachment_zone.width() / 3,
                         GlicWidget::GetInitialSize().width());
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

void GlicWindowControllerImpl::MovePositionToBrowserGlicButton(
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

  if (AlwaysDetached() && !IsActive()) {
    GetGlicWidget()->Activate();
  }

  // This will stack with a move animation below.
  MaybeAdjustSizeForDisplay(animate);

  GlicButton* glic_button = GetGlicButton(browser);
  CHECK(glic_button);
  gfx::Point window_target_top_left;
  if (AlwaysDetached()) {
    gfx::Point button_bottom_left =
        glic_button->GetBoundsWithInset().bottom_left();
    window_target_top_left =
        gfx::Point(button_bottom_left.x() -
                       GetGlicWidget()->GetWindowBoundsInScreen().width(),
                   button_bottom_left.y());
  } else {
    gfx::Point top_right =
        GetTopRightPositionForAttachedGlicWindow(glic_button);
    window_target_top_left = gfx::Point(
        top_right.x() - GetGlicWidget()->GetWindowBoundsInScreen().width(),
        top_right.y());
  }

  // Avoid conversions between pixels and DIP on non 1.0 scale factor displays
  // changing widget width and height.
  base::TimeDelta duration =
      animate ? kAnimationDuration : base::Milliseconds(0);
  glic_window_animator_->AnimatePosition(window_target_top_left, duration,
                                         base::DoNothing());
  NotifyIfPanelStateChanged();
}

void GlicWindowControllerImpl::AddStateObserver(StateObserver* observer) {
  state_observers_.AddObserver(observer);
}

void GlicWindowControllerImpl::RemoveStateObserver(StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void GlicWindowControllerImpl::NotifyIfPanelStateChanged() {
  auto new_state = ComputePanelState();
  if (new_state != panel_state_) {
    panel_state_ = new_state;
    state_observers_.Notify(&StateObserver::PanelStateChanged, panel_state_,
                            attached_browser_);
  }
}

mojom::PanelState GlicWindowControllerImpl::ComputePanelState() const {
  return CreatePanelState(IsShowing(), attached_browser_);
}

bool GlicWindowControllerImpl::IsActive() {
  return IsShowing() && GetGlicWidget() && GetGlicWidget()->IsActive();
}

bool GlicWindowControllerImpl::IsShowing() const {
  return !(state_ == State::kClosed);
}

bool GlicWindowControllerImpl::IsPanelOrFreShowing() const {
  return IsShowing() || fre_controller_->IsShowingDialog();
}

bool GlicWindowControllerImpl::IsAttached() const {
  return attached_browser_ != nullptr;
}

bool GlicWindowControllerImpl::IsDetached() const {
  return IsShowing() && !IsAttached();
}

base::CallbackListSubscription
GlicWindowControllerImpl::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  return window_activation_callback_list_.Add(std::move(callback));
}

void GlicWindowControllerImpl::Preload() {
  if (!host().contents_container()) {
    host().CreateContents();
    host().webui_contents()->Resize(GetInitialBounds(nullptr));
  }
}

void GlicWindowControllerImpl::PreloadFre() {
  if (fre_controller_->ShouldShowFreDialog()) {
    fre_controller_->TryPreload();
  }
}

void GlicWindowControllerImpl::Reload() {
  if (GetFreWebContents()) {
    GetFreWebContents()->ReloadFocusedFrame();
  }
  if (auto* webui_contents = host().webui_contents()) {
    webui_contents->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                           /*check_for_repost=*/false);
  }
}

bool GlicWindowControllerImpl::IsWarmed() const {
  return !!host().contents_container();
}

base::WeakPtr<GlicWindowController> GlicWindowControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GlicWindowControllerImpl::Shutdown() {
  // Hide first, then clean up (but do not animate).
  Close();
  fre_controller_->Shutdown();
  window_activation_callback_list_.Notify(false);
}

bool GlicWindowControllerImpl::IsBrowserOccludedAtPoint(Browser* browser,
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

gfx::Size GlicWindowControllerImpl::GetLastRequestedSizeClamped() const {
  gfx::Size min = GlicWidget::GetInitialSize();
  if (glic_widget_) {
    gfx::Size widget_min = glic_widget_->GetMinimumSize();
    if (!widget_min.IsEmpty()) {
      min = widget_min;
    }
  }

  gfx::Size max(kMaxWidgetSize, kMaxWidgetSize);
  gfx::Size result = glic_size_.value_or(min);

  result.SetToMax(min);
  result.SetToMin(max);
  return result;
}

void GlicWindowControllerImpl::MaybeAdjustSizeForDisplay(bool animate) {
  if (state_ == State::kOpen || state_ == State::kWaitingForGlicToLoad) {
    const auto target_size = GetLastRequestedSizeClamped();
    if (target_size != glic_window_animator_->GetCurrentTargetBounds().size()) {
      glic_window_animator_->AnimateSize(
          target_size, animate ? kAnimationDuration : base::Milliseconds(0),
          base::DoNothing());
    }
  }
}

void GlicWindowControllerImpl::SetWindowState(State new_state) {
  if (state_ == new_state) {
    return;
  }
  state_ = new_state;

  if (IsWindowOpenAndReady()) {
    glic_service_->metrics()->OnGlicWindowOpenAndReady();
  }
}

bool GlicWindowControllerImpl::IsWindowOpenAndReady() {
  return host().IsReady() && state_ == State::kOpen;
}

GlicWindowController::State GlicWindowControllerImpl::state() const {
  return state_;
}

bool GlicWindowControllerImpl::IsDragging() {
  return in_move_loop_;
}

Profile* GlicWindowControllerImpl::profile() {
  return profile_;
}

GlicWindowAnimator* GlicWindowControllerImpl::window_animator() {
  return glic_window_animator_.get();
}

GlicFreController* GlicWindowControllerImpl::fre_controller() {
  return fre_controller_.get();
}

// Return the Browser to which the panel is attached, or null if detached.
Browser* GlicWindowControllerImpl::attached_browser() {
  return attached_browser_;
}

web_modal::WebContentsModalDialogHost*
GlicWindowControllerImpl::GetWebContentsModalDialogHost() {
  return this;
}

gfx::Size GlicWindowControllerImpl::GetMaximumDialogSize() {
  if (!glic_widget_) {
    return gfx::Size();
  }
  return glic_widget_->GetClientAreaBoundsInScreen().size();
}

gfx::NativeView GlicWindowControllerImpl::GetHostView() const {
  if (!glic_widget_) {
    return gfx::NativeView();
  }
  return glic_widget_->GetNativeView();
}

gfx::Point GlicWindowControllerImpl::GetDialogPosition(
    const gfx::Size& dialog_size) {
  if (!glic_widget_) {
    return gfx::Point();
  }
  gfx::Rect client_area_bounds = glic_widget_->GetClientAreaBoundsInScreen();
  return gfx::Point((client_area_bounds.width() - dialog_size.width()) / 2, 0);
}

bool GlicWindowControllerImpl::ShouldDialogBoundsConstrainedByHost() {
  // Allows web modal dialogs to extend beyond the boundary of glic window.
  // These web modals are usually larger than the glic window.
  return false;
}

void GlicWindowControllerImpl::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observers_.AddObserver(observer);
}

void GlicWindowControllerImpl::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observers_.RemoveObserver(observer);
}

}  // namespace glic
