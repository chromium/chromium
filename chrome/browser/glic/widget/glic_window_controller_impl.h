// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/widget/glic_window_config.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_event_observer.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class SkRegion;
class Browser;
class WindowFinder;
namespace gfx {
class Size;
class Point;
}  // namespace gfx

namespace glic {
class GlicEnabling;
class GlicView;
class GlicWindowAnimator;
class ScopedGlicButtonIndicator;
class GlicInstanceMetrics;

// This class owns and manages the glic window. This class has the same lifetime
// as the GlicKeyedService, so it exists if and only if the profile exists.
//
// See the |State| enum below for the lifecycle of the window. When the glic
// window is open |attached_browser_| indicates if the window is attached or
// standalone. See |IsAttached|
class GlicWindowControllerImpl
    : public display::DisplayObserver,
      public GlicWindowControllerInterface,
      public views::WidgetObserver,
      public Host::EmbedderDelegate,
      public Host::Observer,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost,
      public GlicWindowEventObserver::Delegate,
      public LocalHotkeyManager::Panel {
 public:
  GlicWindowControllerImpl(const GlicWindowControllerImpl&) = delete;
  GlicWindowControllerImpl& operator=(const GlicWindowControllerImpl&) = delete;

  GlicWindowControllerImpl(Profile* profile,
                           signin::IdentityManager* identity_manager,
                           GlicKeyedService* service,
                           GlicEnabling* enabling);
  ~GlicWindowControllerImpl() override;

  // GlicWindowController implementation
  void Toggle(BrowserWindowInterface* browser,
              bool prevent_close,
              mojom::InvocationSource source,
              std::optional<std::string> prompt_suggestion) override;
  void ShowAfterSignIn(base::WeakPtr<Browser> browser) override;
  void FocusIfOpen() override;
  void Shutdown() override;
  void MaybeSetWidgetCanResize() override;
  gfx::Size GetPanelSize() override;
  void Close() override;
  void CloseInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) override;
  void CloseAndShutdownInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) override;

  void AddStateObserver(StateObserver* observer) override;
  void RemoveStateObserver(StateObserver* observer) override;
  void AddGlobalStateObserver(PanelStateObserver* observer) override;
  void RemoveGlobalStateObserver(PanelStateObserver* observer) override;
  void SetDraggableRegion(const SkRegion& draggable_region) override;

  bool IsPanelShowingForBrowser(
      const BrowserWindowInterface& bwi) const override;

  bool IsActive() override;
  bool IsAttached() override;
  bool IsAttached() const;
  bool IsDetached() const override;
  base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback) override;
  base::CallbackListSubscription AddGlobalShowHideCallback(
      base::RepeatingClosure callback) override;
  void Preload() override;
  void Reload(content::RenderFrameHost* render_frame_host) override;
  bool IsWarmed() const override;
  base::WeakPtr<GlicWindowControllerInterface> GetWeakPtr() override;

  // GlicWindowEventObserver::Delegate:
  GlicWindowAnimator* window_animator() override;

  // Handles end-of-drag:
  //  - If glic is within attachment distance of a browser window's glic button,
  //    attach the glic window to the button's position.
  //  - If glic is still detached and has moved to a display with a different
  //    work area size, possibly resize the window.
  void OnDragComplete() override;

  base::WeakPtr<views::View> GetView() override;
  GlicWidget* GetGlicWidget() const override;

  Browser* attached_browser() override;
  State state() const override;
  Profile* profile() override;
  gfx::Rect GetInitialBounds(Browser* browser) override;
  void ShowDetachedForTesting() override;
  void SetPreviousPositionForTesting(gfx::Point position) override;
  std::unique_ptr<views::View> CreateViewForSidePanel(
      tabs::TabInterface& tab) override;
  void SidePanelShown(BrowserWindowInterface* browser) override;

  // views::WidgetObserver implementation, monitoring the glic window widget.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetUserResizeStarted() override;
  void OnWidgetUserResizeEnded() override;

  // Host::EmbedderDelegate implementation
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback) override;
  void SetDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas) override;
  void EnableDragResize(bool enabled) override;
  void Attach() override;
  void Detach() override;
  void ClosePanel() override;
  void SetMinimumWidgetSize(const gfx::Size& size) override;
  bool IsShowing() const override;
  void SwitchConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  void CaptureScreenshot(
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback)
      override;

  // InstanceInterface implementation.
  mojom::PanelState GetPanelState() override;

  // display::DisplayObserver implementation
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // LocalHotkeyManager::Panel:
  bool HasFocus() override;
  bool ActivateBrowser() override;
  void ShowTitleBarContextMenuAt(gfx::Point event_loc) override;

  HostManager& host_manager() override;
  std::vector<GlicInstance*> GetInstances() override;
  GlicInstance* GetInstanceForTab(const tabs::TabInterface* tab) const override;

  // GlicInstance implementation
  Host& host() override;
  const InstanceId& id() const override;
  std::optional<std::string> conversation_id() const override;
  base::TimeTicks GetLastActiveTime() const override;
  base::CallbackListSubscription RegisterStateChange(
      StateChangeCallback callback) override;
  base::CallbackListSubscription
  AddActiveInstanceChangedCallbackAndNotifyImmediately(
      ActiveInstanceChangedCallback callback) override;
  GlicInstance* GetActiveInstance() override;

  // Testing functionality.
  GlicWindowAnimator* GetWindowAnimatorForTesting();
  GlicView* GetGlicViewForTesting() const { return GetGlicView(); }

  glic::GlicInstanceMetrics* instance_metrics() override;

 private:
  void CloseWithReason(views::Widget::ClosedReason reason);
  GlicView* GetGlicView() const;
  void ToggleWhenNotAlwaysDetached(
      Browser* new_attached_browser,
      bool prevent_close,
      mojom::InvocationSource source,
      std::optional<std::string> prompt_suggestion);

  // Sets the floating attributes of the glic window.
  //
  // When set to true, the glic window is set to have a `kFloatingWindow`
  // z-order, and on the Mac is set to be "activation independent" (to allow the
  // user to interact with it without causing Chromium to be activated), and
  // visible on every space (including fullscreen ones).
  //
  // When set to false, the glic window is set to have a `kNormal` z-order, and
  // on the Mac, all special activation and visibility properties are cleared.
  void SetGlicWindowToFloatingMode(bool floating);

  // Check if the panel position should be reset based on `window_config_`.
  // Update `window_config_` that the panel was shown.
  void MaybeResetPanelPostionOnShow(mojom::InvocationSource source);

  // Creates the glic view, waits for the web client to initialize, and then
  // shows the glic window. If `browser` is non-nullptr then glic will be
  // attached to the browser. Otherwise glic will be detached.
  void Show(Browser* browser,
            mojom::InvocationSource source,
            std::optional<std::string> prompt_suggestion);
  // Performs necessary set up and initialization before creating GlicWidget or
  // GlicView. Must be called before it's shown.
  // Returns true if successful and view creation can continue.
  bool BeforeViewCreated(Browser* browser,
                         mojom::InvocationSource source,
                         std::optional<std::string> prompt_suggestion);
  // Additional set up and initialization that runs after Glic is shown.
  void AfterViewShown();
  void SetupAndShowGlicWidget(Browser* browser);
  void SetupGlicWidgetAccessibilityText();

  // Reset all state associated with an open side panel or floating panel and
  // close the panel. Before opening the panel again all state must be reset.
  // No Glic metrics are recorded. This method can safely be called even when
  // the panel is not open.
  void ResetAndHidePanel();

  // Host::Observer implementation.
  void WebClientInitializeFailed() override;
  void LoginPageCommitted() override;
  void ClientReadyToShow(const mojom::OpenPanelInfo& open_info) override;
  void OnViewChanged(mojom::CurrentView view) override;
  void ContextAccessIndicatorChanged(bool enabled) override;

  // Called once glic is completely loaded and any animations have finished.
  // This is the end of the opening process and |state_| will be set to kOpen.
  void GlicLoadedAndReadyToDisplay();

  void SetDraggingAreasAndWatchForMouseEvents();

  // Save the top-right corner position for re-opening.
  void SaveWidgetPosition(bool user_modified);

  // Clear the previous position if the widget would not be on an existing
  // display when shown.
  void MaybeResetPreviousPosition(const gfx::Size& target_size);

  // Perform set up attaching panel to this browser.
  void AttachToBrowser(Browser& browser, AttachChangeReason reason);
  // Like `AttachToBrowser` but also explicitly requests to open the side panel.
  void AttachToBrowserAndShow(Browser& browser, AttachChangeReason reason);

  // Finds a browser within attachment distance of glic to toggle the attachment
  // indicator.
  void HandleGlicButtonIndicator();

  // Find and return a browser within attachment distance. Returns nullptr if no
  // browsers are within attachment distance.
  BrowserWindowInterface* FindBrowserForAttachment();

  // Called when the move animation finishes when attaching.
  void AttachAnimationFinished();

  // This method should be called anytime:
  //  * state_ transitions to or from kClosed.
  //  * attached_browser_ changes.
  void NotifyIfPanelStateChanged();
  mojom::PanelState ComputePanelState() const;

  // When the attached browser is closed, this is invoked so we can clean up.
  void AttachedBrowserDidClose(BrowserWindowInterface* browser);

  // Returns true if a browser is occluded at point in screen coordinates.
  bool IsBrowserOccludedAtPoint(BrowserWindowInterface* browser,
                                gfx::Point point);

  // Possibly adjusts the size of the window appropriate for the current
  // display workspace, but only if it's different than the current target size.
  void MaybeAdjustSizeForDisplay(bool animate);

  // Modifies `state_` to the given new state.
  void SetWindowState(State new_state);

  // Returns true of the window is showing and the content is loaded.
  bool IsWindowOpenAndReady();

  // web_modal::WebContentsModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;

  // web_modal::WebContentsModalDialogHost:
  gfx::Size GetMaximumDialogSize() override;
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) override;
  bool ShouldConstrainDialogBoundsByHost() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // Maybe send a ViewChangeRequest:
  void MaybeSendConversationViewRequest();
  void MaybeSendActuationViewRequest();

  // Maybe send a request to change the view.
  void MaybeSendViewChangeRequest(mojom::InvocationSource source);

  // Check if the invocation source matches the entry point for the given view.
  bool InvocationSourceMatchesCurrentView(mojom::InvocationSource source);

  using StateChangeCallbackList =
      base::RepeatingCallbackList<void(bool, mojom::CurrentView view)>;
  StateChangeCallbackList state_change_callback_list_;

  // Observes the glic widget.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      glic_widget_observation_{this};

  // Observes the display configuration.
  display::ScopedOptionalDisplayObserver display_observer_{this};

  // Used for observing closing of the pinned browser.
  std::optional<base::CallbackListSubscription> browser_close_subscription_;

  // List of callbacks to be notified when window activation has changed.
  base::RepeatingCallbackList<void(bool)> window_activation_callback_list_;

  // Drags the glic window following the current mouse location with the given
  // `mouse_offset` and checks if the glic window is at a position where it
  // could attach to a browser window when a drag ends.
  void HandleWindowDragWithOffset(gfx::Vector2d mouse_offset);

  const raw_ptr<Profile> profile_;
  base::ObserverList<StateObserver> state_observers_;
  Host host_;
  std::unique_ptr<HostManager> host_manager_;

  // Must outlive `glic_widget_`
  std::unique_ptr<views::WidgetDelegate> glic_delegate_;

  // Exists when the glic panel is open and in window mode.
  std::unique_ptr<GlicWidget> glic_widget_;

  // Exists when the glic panel is open and in side panel mode.
  // Owned by the `SidePanelEntry` showing the view.
  raw_ptr<GlicView> glic_view_;

  std::unique_ptr<GlicWindowAnimator> glic_window_animator_;
  std::unique_ptr<GlicWindowEventObserver> window_event_observer_;

  // True if we've hit a login page (and have not yet shown).
  bool login_page_committed_ = false;

  // This member contains the last size that glic requested. This should be
  // reset every time glic is closed but is currently cached.
  std::optional<gfx::Size> glic_size_;

  // Whether the widget should be user resizable, kept here in case it's
  // specified before the widget is created.
  bool user_resizable_ = true;

  // This is the last panel state sent to observers. It should only be updated
  // in `NotifyIfPanelStateChanged`.
  mojom::PanelState panel_state_;

  raw_ptr<GlicWebClientAccess> web_client_;

  // Modified only by calling `SetWindowState`.
  State state_ = State::kClosed;

  // If State != kClosed, then the UI must either be associated with a browser
  // window, or standalone. That is tracked by this member.
  raw_ptr<Browser> attached_browser_ = nullptr;

  // Used by web modals to listens for glic window events, e.g. size change or
  // window close.
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observers_;

  // The announcement should happen the first time focus is lost after the FRE.
  bool do_focus_loss_announcement_ = false;

  // Whether the user is currently drag-resizing the widget.
  bool user_resizing_ = false;

  GlicWindowConfig window_config_;

  // The invocation source requesting the opening of the web client. Note that
  // this value is retained until it is consumed by the web client. Because
  // opening the glic window may not actually load the client, there's no
  // guarantee that this value is sent to the web client.
  std::optional<mojom::InvocationSource> opening_source_;

  // String to be auto-filled in the user input text box as the web client is
  // shown to the user.
  std::optional<std::string> prompt_suggestion_;

  std::optional<gfx::Point> previous_position_ = std::nullopt;

  std::unique_ptr<ScopedGlicButtonIndicator> scoped_glic_button_indicator_;

  std::unique_ptr<WindowFinder> window_finder_;

  std::unique_ptr<LocalHotkeyManager> application_hotkey_manager_;
  std::unique_ptr<LocalHotkeyManager> glic_panel_hotkey_manager_;

  raw_ptr<GlicKeyedService> glic_service_;  // Owns this.
  raw_ptr<GlicEnabling> enabling_;
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};
  const InstanceId id_;

  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;

  base::WeakPtrFactory<GlicWindowControllerImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_IMPL_H_
