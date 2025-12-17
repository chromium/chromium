// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_controller_impl.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/browser_ui/scoped_glic_button_indicator.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/application_hotkey_delegate.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/glic/widget/glic_panel_hotkey_delegate.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"
#include "chrome/browser/glic/widget/glic_window_config.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
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

constexpr static base::TimeDelta kAnimationDuration = base::Milliseconds(300);

mojom::PanelState CreatePanelState(bool widget_visible,
                                   Browser* attached_browser) {
  mojom::PanelState panel_state;
  if (!widget_visible) {
    panel_state.kind = mojom::PanelStateKind::kHidden;
  } else if (attached_browser) {
    panel_state.kind = mojom::PanelStateKind::kAttached;
    panel_state.window_id = attached_browser->session_id().id();
  } else {
    panel_state.kind = mojom::PanelStateKind::kDetached;
  }
  return panel_state;
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

GlicWindowControllerImpl::GlicWindowControllerImpl(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* glic_service,
    GlicEnabling* enabling)
    : profile_(profile),
      host_(profile, nullptr, this, glic_service),
      window_finder_(std::make_unique<WindowFinder>()),
      glic_service_(glic_service),
      enabling_(enabling),
      id_(base::Uuid::GenerateRandomV4()) {
  host_manager_ = std::make_unique<HostManager>(profile, GetWeakPtr());
  if (window_config_.ShouldResetOnStart()) {
    previous_position_.reset();
  } else {
    previous_position_ = GetPreviousPositionFromPrefs(profile_->GetPrefs());
  }
  application_hotkey_manager_ =
      MakeApplicationHotkeyManager(weak_ptr_factory_.GetWeakPtr());
  host_.SetDelegate(this);
  host_observation_.Observe(&host());
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
    glic_service_->metrics()->OnGlicWindowOpenInterrupted();
    GlicLoadedAndReadyToDisplay();
  }
}

void GlicWindowControllerImpl::LoginPageCommitted() {
  login_page_committed_ = true;
  if (state_ == State::kWaitingForGlicToLoad && !host().IsReady()) {
    // TODO(crbug.com/388328847): Temporarily allow showing the UI when a login
    // page is reached.
    glic_service_->metrics()->OnGlicWindowOpenInterrupted();
    GlicLoadedAndReadyToDisplay();
  }
}

// Monitoring the glic widget.
void GlicWindowControllerImpl::OnWidgetActivationChanged(views::Widget* widget,
                                                         bool active) {
  if (IsDetached() && GetGlicWidget() != widget) {
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
  if (IsDetached() && GetGlicWidget() == widget) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GlicWindowControllerImpl::Close,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void GlicWindowControllerImpl::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (window_event_observer_->IsDragging() && !AlwaysDetached()) {
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
  if (!IsDetached()) {
    // TODO(crbug.com/439745838): Implement for side panel if needed.
    NOTIMPLEMENTED();
    return;
  }
  glic_service_->metrics()->OnWidgetUserResizeEnded();
  if (GlicWebClientAccess* client = host().GetPrimaryWebClient()) {
    client->ManualResizeChanged(false);
  }

  if (GetGlicView() &&
      !base::FeatureList::IsEnabled(features::kGlicWindowDragRegions)) {
    GetGlicView()->UpdatePrimaryDraggableAreaOnResize();
  }

  if (GetGlicWidget()) {
    glic_size_ = GetGlicWidget()->GetClientAreaBoundsInScreen().size();
    SaveWidgetPosition(/*user_modified=*/true);
  }

  glic_window_animator_->ResetLastTargetSize();
  user_resizing_ = false;
}

void GlicWindowControllerImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!IsDetached()) {
    return;
  }

  MaybeAdjustSizeForDisplay(/*animate=*/false);
  window_event_observer_->AdjustPositionIfNeeded();
}

void GlicWindowControllerImpl::ShowAfterSignIn(base::WeakPtr<Browser> browser) {
  Toggle(browser.get(), true,
         // Prefer the source that triggered the sign-in, but if that's not
         // available, report it as coming from the sign-in flow.
         opening_source_.value_or(mojom::InvocationSource::kAfterSignIn),
         prompt_suggestion_);
}

void GlicWindowControllerImpl::Toggle(
    BrowserWindowInterface* bwi,
    bool prevent_close,
    mojom::InvocationSource source,
    std::optional<std::string> prompt_suggestion) {
  Browser* new_attached_browser =
      bwi ? bwi->GetBrowserForMigrationOnly() : nullptr;

  if (!AlwaysDetached()) {
    ToggleWhenNotAlwaysDetached(new_attached_browser, prevent_close, source,
                                prompt_suggestion);
    return;
  }

  auto maybe_close = [this, prevent_close] {
    if (!prevent_close) {
      Close();
    }
  };

  // Send a change view request if the current view is different than the
  // source.
  // TODO(crbug.com/437140901): The client may not be connected yet. If not,
  // this request is dropped.
  MaybeSendViewChangeRequest(source);

  // If floaty is closed, open floaty
  if (state_ == State::kClosed) {
    Show(new_attached_browser, source, prompt_suggestion);
    return;
  }

#if BUILDFLAG(IS_WIN)
  // Clicking status tray on Windows makes floaty not active so always close.
  if (source == mojom::InvocationSource::kOsButton) {
    Close();
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  // TODO(crbug.com/438164568): Add handling to always close on the second
  // click of the same source.
  // If floaty is focused and click is not from the Task Icon or Glic
  // Button, close it. If floaty is open and the current view matches the
  // expected view, close it. If floaty is unfocused and open, focus it.
  if ((IsActive() && (source != mojom::InvocationSource::kActorTaskIcon &&
                      source != mojom::InvocationSource::kTopChromeButton)) ||
      (InvocationSourceMatchesCurrentView(source) &&
       !base::FeatureList::IsEnabled(features::kGlicZOrderChanges))) {
    maybe_close();
  } else if (state_ == State::kOpen) {
    // TODO(crbug.com/404601783): Bring focus to the textbox.
    GetGlicWidget()->Activate();
    GetGlicView()->GetWebContents()->Focus();
  }
}

void GlicWindowControllerImpl::MaybeSendViewChangeRequest(
    mojom::InvocationSource source) {
  auto current_view = host().GetPrimaryCurrentView();
  if (source == mojom::InvocationSource::kActorTaskIcon &&
      current_view == mojom::CurrentView::kConversation) {
    MaybeSendActuationViewRequest();
  } else if (source == mojom::InvocationSource::kTopChromeButton &&
             current_view == mojom::CurrentView::kActuation) {
    MaybeSendConversationViewRequest();
  }
}

void GlicWindowControllerImpl::ToggleWhenNotAlwaysDetached(
    Browser* new_attached_browser,
    bool prevent_close,
    mojom::InvocationSource source,
    std::optional<std::string> prompt_suggestion) {
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
  if (state_ == State::kOpen || state_ == State::kWaitingForGlicToLoad ||
      state_ == State::kWaitingForSidePanelToShow) {
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
        AttachToBrowserAndShow(*new_attached_browser,
                               AttachChangeReason::kInit);
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
        // Hotkey when neither attached browser nor glic are active: detach
        // current side panel.
        Detach();
      }
      return;
    }

    // Hotkey invoked when glic is already detached.
    maybe_close();

  } else if (state_ != State::kClosed) {
    // Currently in the process of showing the widget, allow that to finish.
    return;
  } else {
    Show(new_attached_browser, source, prompt_suggestion);
  }
}

void GlicWindowControllerImpl::FocusIfOpen() {
  if (!IsShowing() || IsActive()) {
    return;
  }

  if (IsDetached()) {
    GetGlicWidget()->Activate();
  }
  GetGlicView()->GetWebContents()->Focus();
  return;
}

void GlicWindowControllerImpl::ShowDetachedForTesting() {
  glic::GlicProfileManager::GetInstance()->SetActiveGlic(glic_service_);
  Show(nullptr, mojom::InvocationSource::kOsHotkey, std::nullopt);
}

void GlicWindowControllerImpl::SetPreviousPositionForTesting(
    gfx::Point position) {
  previous_position_ = position;
}

Host& GlicWindowControllerImpl::host() {
  return host_;
}

const InstanceId& GlicWindowControllerImpl::id() const {
  return id_;
}

HostManager& GlicWindowControllerImpl::host_manager() {
  return *host_manager_;
}

std::vector<GlicInstance*> GlicWindowControllerImpl::GetInstances() {
  return {this};
}

GlicInstance* GlicWindowControllerImpl::GetInstanceForTab(
    const tabs::TabInterface* tab) const {
  return const_cast<GlicWindowControllerImpl*>(this);
}

bool GlicWindowControllerImpl::BeforeViewCreated(
    Browser* browser,
    mojom::InvocationSource source,
    std::optional<std::string> prompt_suggestion) {
  if (state_ == State::kWaitingForSidePanelToShow) {
    return false;
  }
  // At this point State must be kClosed, and all glic window state must be
  // unset.
  CHECK(!attached_browser_);
  opening_source_ = source;
  prompt_suggestion_ = prompt_suggestion;
  if (!glic_service_->GetAuthController().CheckAuthBeforeShowSync(
          base::BindOnce(&GlicWindowControllerImpl::ShowAfterSignIn,
                         weak_ptr_factory_.GetWeakPtr(),
                         browser ? browser->AsWeakPtr() : nullptr))) {
    return false;
  }

  SetWindowState(State::kWaitingForGlicToLoad);

  glic_service_->metrics()->OnGlicWindowStartedOpening(/*attached=*/browser,
                                                       source);
  glic_service_->GetAuthController().OnGlicWindowOpened();

  MaybeResetPanelPostionOnShow(source);

  host().CreateContents(/*initially_hidden=*/false);
  host().NotifyWindowIntentToShow();

  glic_panel_hotkey_manager_ =
      MakeGlicWindowHotkeyManager(weak_ptr_factory_.GetWeakPtr());
  return true;
}

void GlicWindowControllerImpl::AfterViewShown() {
  glic_panel_hotkey_manager_->InitializeAccelerators();

  // Notify the web client that the panel will open, and wait for the response
  // to actually show the window. Note that we have to call
  // `NotifyIfPanelStateChanged()` first, so that the host will receive the
  // correct panel state.
  NotifyIfPanelStateChanged();
  Host::PanelWillOpenOptions open_options;
  if (prompt_suggestion_) {
    open_options.prompt_suggestion = prompt_suggestion_.value();
  }
  host().PanelWillOpen(opening_source_.value(), std::move(open_options));
  prompt_suggestion_.reset();

  if (login_page_committed_) {
    // This indicates that we've warmed the web client and it has hit a login
    // page. See LoginPageCommitted.
    GlicLoadedAndReadyToDisplay();
  } else if (IsDetached() && !base::FeatureList::IsEnabled(
                                 features::kGlicHandleDraggingNatively)) {
    // This adds dragging functionality to special case panels (e.g. error,
    // offline, loading).
    window_event_observer_->SetDraggingAreasAndWatchForMouseEvents();
  }
}

void GlicWindowControllerImpl::Show(
    Browser* browser,
    mojom::InvocationSource source,
    std::optional<std::string> prompt_suggestion) {
  if (!BeforeViewCreated(browser, source, prompt_suggestion)) {
    return;
  }
  if (browser && !AlwaysDetached()) {
    AttachToBrowserAndShow(*browser, AttachChangeReason::kInit);
  } else {
    SetupAndShowGlicWidget(browser);
    AfterViewShown();
  }
}

std::unique_ptr<views::View> GlicWindowControllerImpl::CreateViewForSidePanel(
    tabs::TabInterface& tab) {
  // GetBrowserForMigrationOnly() is a stop-gap until the rest of the code in
  // GlicWindowController is updated to use BrowserWindowInterface instead of
  // Browser
  auto* browser = tab.GetBrowserWindowInterface()->GetBrowserForMigrationOnly();
  // TODO: Add Invocation source for toolbar button
  if (BeforeViewCreated(browser, mojom::InvocationSource::kThreeDotsMenu,
                        std::nullopt) &&
      browser) {
    AttachToBrowser(*browser, AttachChangeReason::kInit);
  }
  auto glic_view =
      std::make_unique<GlicView>(profile_, GlicWidget::GetInitialSize(),
                                 glic_panel_hotkey_manager_->GetWeakPtr());
  glic_view->SetWebContents(host().webui_contents());
  glic_view->UpdateBackgroundColor();
  glic_view_ = glic_view.get();
  SetWindowState(GlicWindowController::State::kWaitingForSidePanelToShow);
  return glic_view;
}

void GlicWindowControllerImpl::SetupAndShowGlicWidget(Browser* browser) {
  const gfx::Rect initial_bounds = GetInitialBounds(browser);

  auto glic_view =
      std::make_unique<GlicView>(profile_, initial_bounds.size(),
                                 glic_panel_hotkey_manager_->GetWeakPtr());
  glic_delegate_ =
      GlicWidget::CreateWidgetDelegate(std::move(glic_view), user_resizable_);
  glic_widget_ = GlicWidget::Create(glic_delegate_.get(), profile_,
                                    initial_bounds, user_resizable_);

  glic_widget_observation_.Observe(glic_widget_.get());
  SetupGlicWidgetAccessibilityText();

  SetGlicWindowToFloatingMode(true);

  glic_window_animator_ = std::make_unique<GlicWindowAnimator>(
      glic_widget_->GetWeakPtr(),
      base::BindRepeating(&GlicWindowControllerImpl::MaybeSetWidgetCanResize,
                          weak_ptr_factory_.GetWeakPtr()));

  window_event_observer_ = std::make_unique<GlicWindowEventObserver>(
      glic_widget_->GetWeakPtr(), this);

  glic_widget_->Show();

  // This is needed in case of theme difference between OS and chrome.
  GetGlicWidget()->ThemeChanged();

  // This is used to handle the case where the native window is closed
  // directly (e.g., Windows context menu close on the title bar). It fixes the
  // bug where the window position was not restored after closing with the
  // context menu close menu item.
  GetGlicWidget()->MakeCloseSynchronous(base::BindOnce(
      &GlicWindowControllerImpl::CloseWithReason, base::Unretained(this)));

  // Immediately hook up the WebView to the WebContents.
  GetGlicView()->SetWebContents(host().webui_contents());
  GetGlicView()->UpdateBackgroundColor();

  // TODO(crbug.com/439745838): This be needed for sidepanel.
  // Add capability to show web modal dialogs (e.g. Data Controls Dialogs for
  // enterprise users) via constrained_window APIs.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      host().webui_contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      host().webui_contents())
      ->SetDelegate(this);

  std::optional<display::Display> display =
      GetGlicWidget()->GetNearestDisplay();
  glic_service_->metrics()->OnGlicWindowShown(
      browser, display, GetGlicWidget()->GetWindowBoundsInScreen());
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
  gfx::Size target_size = GlicWidget::ClampSize(glic_size_, GetGlicWidget());

  // Reset previous position if it results in an invalid location.
  if (previous_position_.has_value() &&
      !GlicWidget::IsWidgetLocationAllowed(
          {previous_position_.value(), target_size})) {
    previous_position_.reset();
  }
  // Use the previous position if there is one.
  if (previous_position_.has_value()) {
    return {previous_position_.value(), target_size};
  }

  return GlicWidget::GetInitialBounds(browser, target_size);
}

void GlicWindowControllerImpl::MaybeResetPanelPostionOnShow(
    mojom::InvocationSource source) {
  if (source == mojom::InvocationSource::kTopChromeButton &&
      window_config_.ShouldResetOnOpen()) {
    previous_position_.reset();
    base::RecordAction(
        base::UserMetricsAction("Glic.Widget.ResetPositionOnOpen"));
  }
  if (window_config_.ShouldResetOnNewSession()) {
    previous_position_.reset();
  }
  if (window_config_.ShouldResetSizeAndLocationOnShow()) {
    previous_position_.reset();
    gfx::Size initial_size = GlicWidget::GetInitialSize();
    // Keep the old height if it is larger than the initial size.
    if (glic_size_.has_value() &&
        glic_size_->height() > initial_size.height()) {
      initial_size.set_height((glic_size_->height()));
    }
    glic_size_ = initial_size;
  }
  window_config_.SetLastOpenTime();
}

void GlicWindowControllerImpl::ClientReadyToShow(
    const mojom::OpenPanelInfo& open_info) {
  DVLOG(1) << "Glic client ready to show " << open_info.web_client_mode;
  glic_service_->metrics()->OnGlicWindowOpenAndReady();
  if (open_info.panelSize.has_value()) {
    Resize(*open_info.panelSize, open_info.resizeDuration, base::DoNothing());
  }
  EnableDragResize(open_info.can_user_resize);

  if (state_ == State::kWaitingForGlicToLoad) {
    GlicLoadedAndReadyToDisplay();
  }
}

void GlicWindowControllerImpl::OnViewChanged(mojom::CurrentView view) {
  state_change_callback_list_.Notify(IsShowing(), view);
}

void GlicWindowControllerImpl::ContextAccessIndicatorChanged(bool enabled) {
  glic_service_->SetContextAccessIndicator(enabled && IsShowing());
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

  if (!base::FeatureList::IsEnabled(features::kGlicHandleDraggingNatively)) {
    window_event_observer_->SetDraggingAreasAndWatchForMouseEvents();
  }

  NotifyIfPanelStateChanged();
}

GlicView* GlicWindowControllerImpl::GetGlicView() const {
  if (!IsShowing()) {
    return nullptr;
  }

  if (glic_view_) {
    return glic_view_;
  }
  if (IsDetached()) {
    return GetGlicWidget()->GetGlicView();
  }
  return nullptr;
}

base::WeakPtr<views::View> GlicWindowControllerImpl::GetView() {
  if (auto* view = GetGlicView()) {
    return view->GetWeakPtr();
  }
  return nullptr;
}

GlicWindowAnimator* GlicWindowControllerImpl::window_animator() {
  return glic_window_animator_.get();
}

GlicWidget* GlicWindowControllerImpl::GetGlicWidget() const {
  return glic_widget_.get();
}

void GlicWindowControllerImpl::AttachedBrowserDidClose(
    BrowserWindowInterface* browser) {
  Close();
}

void GlicWindowControllerImpl::Attach() {
  if (!GetGlicWidget()) {
    return;
  }

  BrowserWindowInterface* browser = glic::FindBrowserForAttachment(profile_);
  if (!browser) {
    return;
  }
  if (AlwaysDetached()) {
    return;
  }
  AttachToBrowserAndShow(*browser->GetBrowserForMigrationOnly(),
                         AttachChangeReason::kMenu);
}

void GlicWindowControllerImpl::Detach() {
  if (state_ != State::kOpen || !attached_browser_ || AlwaysDetached()) {
    return;
  }
  SetWindowState(State::kDetaching);

  // Close the existing side panel.
  auto current_browser = attached_browser_;
  ResetAndHidePanel();

  // Open the panel detached.
  SetupAndShowGlicWidget(current_browser);
  if (!base::FeatureList::IsEnabled(features::kGlicHandleDraggingNatively)) {
    window_event_observer_->SetDraggingAreasAndWatchForMouseEvents();
  }

  SetWindowState(State::kOpen);
  NotifyIfPanelStateChanged();
}

void GlicWindowControllerImpl::AttachToBrowser(Browser& browser,
                                               AttachChangeReason reason) {
  CHECK(!AlwaysDetached());
  glic_service_->metrics()->OnAttachedToBrowser(reason);

  ResetAndHidePanel();

  attached_browser_ = &browser;
  user_resizing_ = true;
  browser_close_subscription_ = attached_browser_->RegisterBrowserDidClose(
      base::BindRepeating(&GlicWindowControllerImpl::AttachedBrowserDidClose,
                          base::Unretained(this)));
}

void GlicWindowControllerImpl::AttachToBrowserAndShow(
    Browser& browser,
    AttachChangeReason reason) {
  AttachToBrowser(browser, reason);
  SetWindowState(GlicWindowController::State::kWaitingForSidePanelToShow);
  browser.GetFeatures().side_panel_ui()->Show(SidePanelEntry::Id::kGlic);
}

void GlicWindowControllerImpl::SidePanelShown(BrowserWindowInterface* browser) {
  SetWindowState(State::kOpen);
  NotifyIfPanelStateChanged();

  // Trigger custom event for testing.
  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kGlicWidgetAttached, GlicButton::FromBrowser(browser));
  AfterViewShown();
}

void GlicWindowControllerImpl::Resize(const gfx::Size& size,
                                      base::TimeDelta duration,
                                      base::OnceClosure callback) {
  glic_size_ = size;
  glic_service_->metrics()->OnGlicWindowResize();

  const bool in_resizable_state =
      IsDetached() &&
      (state_ == State::kOpen || state_ == State::kWaitingForGlicToLoad);

  // TODO(https://crbug.com/379164689): Drive resize animations for error states
  // from the browser. For now, we allow animations during the waiting state.
  // TODO(https://crbug.com/392668958): If the widget is ready and asks for a
  // resize before the opening animation is finished, we will stop the current
  // animation and resize to the final size. Investigate a smoother way to
  // animate this transition.
  if (in_resizable_state && !user_resizing_) {
    glic_window_animator_->AnimateSize(
        GlicWidget::ClampSize(glic_size_, GetGlicWidget()), duration,
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
  if (!IsDetached()) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kGlicZOrderChanges)) {
    // Drag-resizability implies text mode, which isn't floating, while
    // non-resizability implies audio mode, which is floating.
    SetGlicWindowToFloatingMode(!enabled);
  }

  MaybeSetWidgetCanResize();
  GetGlicView()->UpdateBackgroundColor();
  glic_window_animator_->MaybeAnimateToTargetSize();
}

void GlicWindowControllerImpl::MaybeSetWidgetCanResize() {
  if (!IsDetached()) {
    return;
  }
  if (GetGlicWidget()->widget_delegate()->CanResize() == user_resizable_ ||
      glic_window_animator_->IsAnimating()) {
    // If the resize state is already correct or the widget is animating do not
    // update the resize state.
    return;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows when resize is enabled there is an invisible border added
  // around the client area. We need to make the widget larger or smaller to
  // keep the visible client area the same size.
  gfx::Rect previous_client_bounds =
      GetGlicWidget()->GetClientAreaBoundsInScreen();
#endif  // BUILDFLAG(IS_WIN)

  // Update resize state on widget delegate.
  GetGlicWidget()->widget_delegate()->SetCanResize(user_resizable_);

#if BUILDFLAG(IS_WIN)
  if (user_resizable_) {
    // Resizable so the widget area is larger than the client area.
    gfx::Rect new_widget_bounds =
        GetGlicWidget()->VisibleToWidgetBounds(previous_client_bounds);
    GetGlicWidget()->SetBoundsConstrained(new_widget_bounds);
  } else {
    // Not resizable so the client and widget areas are the same.
    GetGlicWidget()->SetBoundsConstrained(previous_client_bounds);
  }
#endif  // BUILDFLAG(IS_WIN)
}

gfx::Size GlicWindowControllerImpl::GetPanelSize() {
  if (IsDetached()) {
    return GetGlicWidget()->GetSize();
  }
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(attached_browser_);
  CHECK(browser_view->contents_height_side_panel());
  // This returns the size of the entire side panel (including content,
  // heading, and surrounding padding).
  return browser_view->contents_height_side_panel()->size();
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
  if (!IsDetached()) {
    return;
  }

  GetGlicWidget()->SetMinimumSize(size);
}

void GlicWindowControllerImpl::CloseWithReason(
    views::Widget::ClosedReason reason) {
  Close();
}

bool GlicWindowControllerImpl::ActivateBrowser() {
  if (IsAttached()) {
    attached_browser()->window()->Activate();
    return true;
  }

  if (auto* const last_active_bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
    last_active_bwi->GetWindow()->Activate();
    return true;
  }

  return false;
}

void GlicWindowControllerImpl::CloseInstanceWithFrame(
    content::RenderFrameHost* render_frame_host) {
  NOTREACHED();
}

void GlicWindowControllerImpl::Close() {
  if (state_ == State::kClosed || state_ == State::kDetaching) {
    return;
  }
  window_config_.SetLastCloseTime();

  if (IsDetached()) {
    std::optional<display::Display> display =
        GetGlicWidget()->GetNearestDisplay();
    BrowserWindowInterface* const last_active_bwi =
        GetLastActiveBrowserWindowInterfaceWithAnyProfile();
    Browser* const last_active_browser =
        last_active_bwi ? last_active_bwi->GetBrowserForMigrationOnly()
                        : nullptr;
    glic_service_->metrics()->OnGlicWindowClose(
        last_active_browser, display,
        GetGlicWidget()->GetWindowBoundsInScreen());
  }
  base::UmaHistogramEnumeration("Glic.PanelWebUiState.FinishState2",
                                host().GetPrimaryWebUiState());

  ResetAndHidePanel();

  SetWindowState(State::kClosed);
  glic_panel_hotkey_manager_.reset();
  user_resizing_ = false;
  window_activation_callback_list_.Notify(false);
  NotifyIfPanelStateChanged();

  host().PanelWasClosed();
  if (base::FeatureList::IsEnabled(features::kGlicUnloadOnClose)) {
    host().Shutdown();
  }
}

void GlicWindowControllerImpl::CloseAndShutdownInstanceWithFrame(
    content::RenderFrameHost* render_frame_host) {
  NOTREACHED();
}

void GlicWindowControllerImpl::ClosePanel() {
  Close();
  if (screenshot_capturer_) {
    screenshot_capturer_->CloseScreenPicker();
  }
}

void GlicWindowControllerImpl::ResetAndHidePanel() {
  if (IsDetached()) {
    SaveWidgetPosition(/*user_modified=*/false);

    modal_dialog_host_observers_.Notify(
        &web_modal::ModalDialogHostObserver::OnHostDestroying);
    web_modal::WebContentsModalDialogManager::FromWebContents(
        host().webui_contents())
        ->SetDelegate(nullptr);
  } else if (IsAttached()) {
    // Closing the side panel destroys its WebView which hides its webcontents.
    // This creates a race where the webcontents might be hidden when showing
    // again after the side panel closes. Prevent this by unsetting the
    // webcontents first.
    if (glic_view_) {
      glic_view_->SetWebContents(nullptr);
    }

    attached_browser_->GetFeatures().side_panel_ui()->Close(
        SidePanelEntry::PanelType::kContent);
  }

  // The following state is always safe to reset regardless of if the panel is
  // detached, attached or currently closed.

  // Floating Panel State
  window_event_observer_.reset();
  glic_window_animator_.reset();
  glic_widget_observation_.Reset();
  glic_widget_.reset();
  glic_delegate_.reset();
  scoped_glic_button_indicator_.reset();

  // Attached Side Panel State.
  attached_browser_ = nullptr;
  glic_view_ = nullptr;
  browser_close_subscription_.reset();
}

void GlicWindowControllerImpl::SaveWidgetPosition(bool user_modified) {
  if (!IsDetached() || !GetGlicWidget()->IsVisible()) {
    return;
  }
  if (window_config_.ShouldSetPostionOnDrag() && !user_modified &&
      !previous_position_.has_value()) {
    profile_->GetPrefs()->ClearPref(prefs::kGlicPreviousPositionX);
    profile_->GetPrefs()->ClearPref(prefs::kGlicPreviousPositionY);
    return;
  }
  previous_position_ = GetGlicWidget()->GetClientAreaBoundsInScreen().origin();
  profile_->GetPrefs()->SetInteger(prefs::kGlicPreviousPositionX,
                                   previous_position_->x());
  profile_->GetPrefs()->SetInteger(prefs::kGlicPreviousPositionY,
                                   previous_position_->y());
}

void GlicWindowControllerImpl::ShowTitleBarContextMenuAt(gfx::Point event_loc) {
#if BUILDFLAG(IS_WIN)
  views::View::ConvertPointToScreen(GetGlicView(), &event_loc);
  event_loc = display::win::GetScreenWin()->DIPToScreenPoint(event_loc);
  views::ShowSystemMenuAtScreenPixelLocation(views::HWNDForView(GetGlicView()),
                                             event_loc);
#endif  // BUILDFLAG(IS_WIN)
}

mojom::PanelState GlicWindowControllerImpl::GetPanelState() {
  return panel_state_;
}

bool GlicWindowControllerImpl::IsPanelShowingForBrowser(
    const BrowserWindowInterface& bwi) const {
  return IsShowing();
}

void GlicWindowControllerImpl::OnDragComplete() {
  if (AlwaysDetached()) {
    // Do not handle attachment.
    return;
  }
  BrowserWindowInterface* browser = FindBrowserForAttachment();
  // No browser within attachment range.
  if (!browser) {
    return;
  }
  // Attach to the found browser.
  AttachToBrowser(*browser->GetBrowserForMigrationOnly(),
                  AttachChangeReason::kDrag);
}

void GlicWindowControllerImpl::HandleGlicButtonIndicator() {
  BrowserWindowInterface* browser = FindBrowserForAttachment();
  // No browser within attachment range so reset indicators
  if (!browser) {
    scoped_glic_button_indicator_.reset();
    return;
  }
  GlicButton* glic_button = GlicButton::FromBrowser(browser);
  // If there isn't an existing scoped indicator for this button, create one.
  if (!scoped_glic_button_indicator_ ||
      scoped_glic_button_indicator_->GetGlicButton() != glic_button) {
    // Bring the browser to the front.
    browser->GetBrowserForMigrationOnly()
        ->GetBrowserView()
        .GetWidget()
        ->Activate();
    scoped_glic_button_indicator_ =
        std::make_unique<ScopedGlicButtonIndicator>(glic_button);
  }
}

BrowserWindowInterface* GlicWindowControllerImpl::FindBrowserForAttachment() {
  // The profile must have started off as Glic enabled since a Glic widget is
  // open but it may have been disabled at runtime by policy. In this edge-case,
  // prevent reattaching back to a window (as it no longer has a GlicButton).
  if (!GlicEnabling::IsEnabledForProfile(profile_)) {
    return nullptr;
  }
  if (!IsDetached()) {
    return nullptr;
  }

  gfx::Point glic_top_right =
      GetGlicWidget()->GetWindowBoundsInScreen().top_right();
  // Loops through all browsers in activation order with the latest accessed
  // browser first.
  BrowserWindowInterface* browser_for_attachment = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (!IsBrowserGlicAttachable(profile_, browser)) {
          return true;  // continue iterating
        }

        auto* tab_strip_view = browser->GetBrowserForMigrationOnly()
                                   ->GetBrowserView()
                                   .tab_strip_view();
        CHECK(tab_strip_view);

        // If the profile is enabled, the Glic button must be available.
        glic::GlicButton* glic_button =
            BrowserElementsViews::From(browser)->GetViewAs<glic::GlicButton>(
                kGlicButtonElementId);
        CHECK(glic_button);

        // Define attachment zone as the right of the tab strip. It either is
        // the width of the widget or 1/3 of the tab strip, whichever is
        // smaller.
        gfx::Rect attachment_zone = tab_strip_view->GetBoundsInScreen();
        int width = std::min(attachment_zone.width() / 3,
                             GlicWidget::GetInitialSize().width());
        attachment_zone.SetByBounds(attachment_zone.right() - width,
                                    attachment_zone.y() - kAttachmentBuffer,
                                    attachment_zone.right() + kAttachmentBuffer,
                                    attachment_zone.bottom());

        // If both the left center of the attachment zone and glic button right
        // center are occluded, don't consider for attachment.
        if (IsBrowserOccludedAtPoint(browser, attachment_zone.left_center()) &&
            IsBrowserOccludedAtPoint(
                browser, glic_button->GetBoundsInScreen().right_center())) {
          return true;  // continue iterating
        }

        if (attachment_zone.Contains(glic_top_right)) {
          browser_for_attachment = browser;
          return false;  // stop iterating
        }

        return true;  // continue iterating
      });

  return browser_for_attachment;
}

void GlicWindowControllerImpl::AddStateObserver(StateObserver* observer) {
  state_observers_.AddObserver(observer);
}

void GlicWindowControllerImpl::RemoveStateObserver(StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void GlicWindowControllerImpl::AddGlobalStateObserver(
    PanelStateObserver* observer) {
  AddStateObserver(observer);
}

void GlicWindowControllerImpl::RemoveGlobalStateObserver(
    PanelStateObserver* observer) {
  RemoveStateObserver(observer);
}

void GlicWindowControllerImpl::NotifyIfPanelStateChanged() {
  auto new_state = ComputePanelState();
  if (new_state != panel_state_) {
    panel_state_ = new_state;
    state_observers_.Notify(&StateObserver::PanelStateChanged, panel_state_,
                            PanelStateContext{
                                .attached_browser = attached_browser_,
                                .glic_widget = GetGlicWidget(),
                            });
  }
}

mojom::PanelState GlicWindowControllerImpl::ComputePanelState() const {
  return CreatePanelState(IsShowing(), attached_browser_);
}

bool GlicWindowControllerImpl::IsActive() {
  if (IsAttached()) {
    auto* browser_view =
        BrowserView::GetBrowserViewForBrowser(attached_browser_);
    DCHECK(browser_view->contents_height_side_panel());
    return browser_view->contents_height_side_panel()->HasFocus();
  }
  return IsDetached() && GetGlicWidget()->IsActive();
}

bool GlicWindowControllerImpl::HasFocus() {
  return IsActive();
}

bool GlicWindowControllerImpl::IsShowing() const {
  return !(state_ == State::kClosed);
}

void GlicWindowControllerImpl::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  std::move(callback).Run(mojom::SwitchConversationErrorReason::kUnknown);
}

void GlicWindowControllerImpl::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  if (!GetGlicWidget()) {
    std::move(callback).Run(mojom::CaptureScreenshotResult::NewErrorReason(
        mojom::CaptureScreenshotErrorReason::kUnknown));
    return;
  }
  if (!screenshot_capturer_) {
    screenshot_capturer_ = std::make_unique<GlicScreenshotCapturer>();
  }
  screenshot_capturer_->CaptureScreenshot(GetGlicWidget()->GetNativeWindow(),
                                          std::move(callback));
}

bool GlicWindowControllerImpl::IsAttached() const {
  return IsShowing() && attached_browser_;
}

bool GlicWindowControllerImpl::IsAttached() {
  return const_cast<const GlicWindowControllerImpl*>(this)->IsAttached();
}

bool GlicWindowControllerImpl::IsDetached() const {
  return IsShowing() && glic_widget_;
}

base::CallbackListSubscription
GlicWindowControllerImpl::AddWindowActivationChangedCallback(
    WindowActivationChangedCallback callback) {
  return window_activation_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicWindowControllerImpl::AddGlobalShowHideCallback(
    base::RepeatingClosure callback) {
  return RegisterStateChange(
      base::BindRepeating([](base::RepeatingClosure callback, bool,
                             mojom::CurrentView) { callback.Run(); },
                          std::move(callback)));
}

void GlicWindowControllerImpl::Preload() {
  if (!host().contents_container()) {
    host().CreateContents(/*initially_hidden=*/true);
    host().webui_contents()->Resize(GetInitialBounds(nullptr));
  }
}

void GlicWindowControllerImpl::Reload(
    content::RenderFrameHost* render_frame_host) {
  if (host().IsWebContentPresentAndMatches(render_frame_host)) {
    host().Reload();
  }
}

bool GlicWindowControllerImpl::IsWarmed() const {
  return const_cast<Host&>(host_).contents_container();
}

base::WeakPtr<GlicWindowControllerInterface>
GlicWindowControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GlicWindowControllerImpl::Shutdown() {
  // Hide first, then clean up (but do not animate).
  Close();
  window_activation_callback_list_.Notify(false);
}

bool GlicWindowControllerImpl::IsBrowserOccludedAtPoint(
    BrowserWindowInterface* browser,
    gfx::Point point) {
  std::set<gfx::NativeWindow> exclude = {
      GetGlicView()->GetWidget()->GetNativeWindow()};
  gfx::NativeWindow window =
      window_finder_->GetLocalProcessWindowAtPoint(point, exclude);
  if (browser->GetBrowserForMigrationOnly()
          ->GetBrowserView()
          .GetWidget()
          ->GetNativeWindow() != window) {
    return true;
  }
  return false;
}

void GlicWindowControllerImpl::MaybeAdjustSizeForDisplay(bool animate) {
  if (!IsDetached()) {
    return;
  }
  const auto target_size = GlicWidget::ClampSize(glic_size_, GetGlicWidget());
  if (target_size != glic_window_animator_->GetCurrentTargetBounds().size()) {
    glic_window_animator_->AnimateSize(
        target_size, animate ? kAnimationDuration : base::Milliseconds(0),
        base::DoNothing());
  }
}

std::optional<std::string> GlicWindowControllerImpl::conversation_id() const {
  return std::nullopt;
}

base::TimeTicks GlicWindowControllerImpl::GetLastActiveTime() const {
  return base::TimeTicks();
}

base::CallbackListSubscription GlicWindowControllerImpl::RegisterStateChange(
    StateChangeCallback callback) {
  return state_change_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicWindowControllerImpl::AddActiveInstanceChangedCallbackAndNotifyImmediately(
    ActiveInstanceChangedCallback callback) {
  NOTREACHED();
}
GlicInstance* GlicWindowControllerImpl::GetActiveInstance() {
  NOTREACHED();
}

void GlicWindowControllerImpl::SetWindowState(State new_state) {
  if (state_ == new_state) {
    return;
  }
  state_ = new_state;

  glic_service_->SetContextAccessIndicator(
      IsShowing() && host().IsContextAccessIndicatorEnabled());

  if (auto* actor_keyed_service = actor::ActorKeyedService::Get(profile_)) {
    // Show toast if floaty is closed.
    BrowserWindowInterface* const last_active_bwi =
        GetLastActiveBrowserWindowInterfaceWithAnyProfile();
    if (state_ == State::kClosed) {
      actor_keyed_service->GetActorUiStateManager()->MaybeShowToast(
          last_active_bwi);
    }
  }

  state_change_callback_list_.Notify(IsShowing(),
                                     host_.GetPrimaryCurrentView());

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

Profile* GlicWindowControllerImpl::profile() {
  return profile_;
}

GlicWindowAnimator* GlicWindowControllerImpl::GetWindowAnimatorForTesting() {
  return glic_window_animator_.get();
}

// Return the Browser to which the panel is attached, or null if detached.
Browser* GlicWindowControllerImpl::attached_browser() {
  return attached_browser_;
}

web_modal::WebContentsModalDialogHost*
GlicWindowControllerImpl::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  return this;
}

gfx::Size GlicWindowControllerImpl::GetMaximumDialogSize() {
  if (IsDetached()) {
    return GetGlicWidget()->GetClientAreaBoundsInScreen().size();
  } else if (IsAttached()) {
    // TODO(crbug.com/439745838): Get side panel height
    NOTIMPLEMENTED();
  }
  return gfx::Size();
}

gfx::NativeView GlicWindowControllerImpl::GetHostView() const {
  if (IsDetached()) {
    return GetGlicWidget()->GetNativeView();
  } else if (IsAttached()) {
    // TODO(crbug.com/439745838): Maybe get Native View for side panel if
    // needed.
    NOTIMPLEMENTED();
  }
  return gfx::NativeView();
}

gfx::Point GlicWindowControllerImpl::GetDialogPosition(
    const gfx::Size& dialog_size) {
  if (IsDetached()) {
    gfx::Rect client_area_bounds =
        GetGlicWidget()->GetClientAreaBoundsInScreen();
    return gfx::Point((client_area_bounds.width() - dialog_size.width()) / 2,
                      0);
  } else if (IsAttached()) {
    // TODO(crbug.com/439745838): Maybe implement for side panel if needed.
    NOTIMPLEMENTED();
  }
  return gfx::Point();
}

bool GlicWindowControllerImpl::ShouldConstrainDialogBoundsByHost() {
  // Allows web modal dialogs to extend beyond the boundary of glic window.
  // These web modals are usually larger than the glic window.
  return false;
}

void GlicWindowControllerImpl::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  if (!IsDetached()) {
    return;
  }
  modal_dialog_host_observers_.AddObserver(observer);
}

void GlicWindowControllerImpl::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  if (!IsDetached()) {
    return;
  }
  modal_dialog_host_observers_.RemoveObserver(observer);
}

void GlicWindowControllerImpl::MaybeSendConversationViewRequest() {
  auto request = mojom::ViewChangeRequest::New(
      mojom::ViewChangeRequestDetails::NewConversation(
          mojom::ViewChangeRequestConversation::New()));
  host().SendViewChangeRequest(std::move(request));
}

void GlicWindowControllerImpl::MaybeSendActuationViewRequest() {
  auto request = mojom::ViewChangeRequest::New(
      mojom::ViewChangeRequestDetails::NewActuation(
          mojom::ViewChangeRequestActuation::New()));
  host().SendViewChangeRequest(std::move(request));
}

bool GlicWindowControllerImpl::InvocationSourceMatchesCurrentView(
    mojom::InvocationSource source) {
  auto current_view = host().GetPrimaryCurrentView();
  return (source == mojom::InvocationSource::kActorTaskIcon &&
          current_view == mojom::CurrentView::kActuation) ||
         (source == mojom::InvocationSource::kTopChromeButton &&
          current_view == mojom::CurrentView::kConversation);
}

glic::GlicInstanceMetrics* GlicWindowControllerImpl::instance_metrics() {
  return nullptr;
}

}  // namespace glic
