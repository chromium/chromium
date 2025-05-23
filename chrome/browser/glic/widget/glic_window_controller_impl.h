// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/widget/application_hotkey_delegate.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_hotkey_delegate.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class WindowFinder;
namespace gfx {
class Size;
class Point;
}  // namespace gfx

namespace glic {

class GlicEnabling;
class ScopedGlicButtonIndicator;
class GlicButton;

// This class owns and manages the glic window. This class has the same lifetime
// as the GlicKeyedService, so it exists if and only if the profile exists.
//
// See the |State| enum below for the lifecycle of the window. When the glic
// window is open |attached_browser_| indicates if the window is attached or
// standalone. See |IsAttached|
class GlicWindowControllerImpl
    : public GlicWindowController,
      public views::WidgetObserver,
      public Host::Observer,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
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
              mojom::InvocationSource source) override;
  void ShowAfterSignIn(base::WeakPtr<Browser> browser) override;
  void ToggleWhenNotAlwaysDetached(Browser* new_attached_browser,
                                   bool prevent_close,
                                   mojom::InvocationSource source) override;
  void FocusIfOpen() override;
  void Attach() override;
  void Detach() override;
  void Shutdown() override;
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback) override;
  void EnableDragResize(bool enabled) override;
  gfx::Size GetSize() override;
  void SetDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas) override;
  void SetMinimumWidgetSize(const gfx::Size& size) override;
  void Close() override;
  void CloseWithReason(views::Widget::ClosedReason reason) override;
  void ShowTitleBarContextMenuAt(gfx::Point event_loc) override;
  bool ShouldStartDrag(const gfx::Point& initial_press_loc,
                       const gfx::Point& mouse_location) override;
  void HandleWindowDragWithOffset(gfx::Vector2d mouse_offset) override;
  const mojom::PanelState& GetPanelState() const override;

  void AddStateObserver(StateObserver* observer) override;
  void RemoveStateObserver(StateObserver* observer) override;

  bool IsActive() override;
  bool IsShowing() const override;
  bool IsPanelOrFreShowing() const override;
  bool IsAttached() const override;
  bool IsDetached() const override;
  base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback) override;
  void Preload() override;
  void PreloadFre() override;
  void Reload() override;
  bool IsWarmed() const override;
  base::WeakPtr<GlicWindowController> GetWeakPtr() override;

  GlicView* GetGlicView() override;
  base::WeakPtr<views::View> GetGlicViewAsView() override;
  GlicWidget* GetGlicWidget() override;
  content::WebContents* GetFreWebContents() override;

  Browser* attached_browser() override;
  State state() const override;
  GlicFreController* fre_controller() override;
  GlicWindowAnimator* window_animator() override;
  Profile* profile() override;
  bool IsDragging() override;
  gfx::Rect GetInitialBounds(Browser* browser) override;
  void ShowDetachedForTesting() override;
  void SetPreviousPositionForTesting(gfx::Point position) override;

  // views::WidgetObserver implementation, monitoring the glic window widget.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetUserResizeStarted() override;
  void OnWidgetUserResizeEnded() override;

 private:
  Host& host() const;

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

  // Return the default detached bounds which are just below the tab strip
  // button on the active browser.
  std::optional<gfx::Rect> GetInitialDetachedBoundsFromBrowser(
      Browser* browser,
      const gfx::Size& target_size);

  // Return the default detached bounds when there is no active browser. The
  // position is relative to the top right of the current display.
  gfx::Rect GetInitialDetachedBoundsNoBrowser(const gfx::Size& target_size);

  // Return the default bounds when attached to the browser which cover the tab
  // strip button on the active browser.
  gfx::Rect GetInitialAttachedBounds(Browser& browser);

  // Creates the glic view, waits for the web client to initialize, and then
  // shows the glic window. If `browser` is non-nullptr then glic will be
  // attached to the browser. Otherwise glic will be detached.
  void Show(Browser* browser, mojom::InvocationSource source);

  void SetupGlicWidget(Browser* browser);
  void SetupGlicWidgetAccessibilityText();

  // Host::Observer implementation.
  void WebClientInitializeFailed() override;
  void LoginPageCommitted() override;
  void ClientReadyToShow(const mojom::OpenPanelInfo& open_info) override;

  // Called once glic is completely loaded and any animations have finished.
  // This is the end of the opening process and |state_| will be set to kOpen.
  void GlicLoadedAndReadyToDisplay();

  void SetDraggingAreasAndWatchForMouseEvents();

  // Called when the Detach() animation ends.
  void DetachFinished();

  // Save the top-right corner position for re-opening.
  void SaveWidgetPosition();

  // Clear the previous position if the widget would not be on an existing
  // display when shown.
  void MaybeResetPreviousPosition(const gfx::Size& target_size);

  // Determines the correct position for the glic window when attached to a
  // browser window. The top right of the widget should be placed here.
  gfx::Point GetTopRightPositionForAttachedGlicWindow(GlicButton* glic_button);

  // Runs an animation to move glic to its target position.
  // TODO(crbug.com/410629338): Reimplement attachment.
  void AttachToBrowser(Browser& browser, AttachChangeReason reason);

  // Keep part of glic window within the visible region.
  void AdjustPositionIfNeeded();

  // Handles end-of-drag:
  //  - If glic is within attachment distance of a browser window's glic button,
  //    attach the glic window to the button's position.
  //  - If glic is still detached and has moved to a display with a different
  //    work area size, possibly resize the window.
  void OnDragComplete();

  // Finds a browser within attachment distance of glic to toggle the attachment
  // indicator.
  void HandleGlicButtonIndicator();

  // Find and return a browser within attachment distance. Returns nullptr if no
  // browsers are within attachment distance.
  Browser* FindBrowserForAttachment();

  // Updates the position of the glic window to that of the glic button of
  // `browser`'s window. This position change is animated if `animate` is true.
  void MovePositionToBrowserGlicButton(const Browser& browser, bool animate);

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
  bool IsBrowserOccludedAtPoint(Browser* browser, gfx::Point point);

  // Return the last size Resize() was called with, or the default initial size
  // if Resize() hasn't been called. The return value is clamped to fit between
  // the minimum and maximum sizes.
  gfx::Size GetLastRequestedSizeClamped() const;

  // Possibly adjusts the size of the window appropriate for the current
  // display workspace, but only if it's different than the current target size.
  void MaybeAdjustSizeForDisplay(bool animate);

  // Modifies `state_` to the given new state.
  void SetWindowState(State new_state);

  // Returns true of the window is showing and the content is loaded.
  bool IsWindowOpenAndReady();

  // web_modal::WebContentsModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost:
  gfx::Size GetMaximumDialogSize() override;
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) override;
  bool ShouldDialogBoundsConstrainedByHost() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // Observes the glic widget.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      glic_widget_observation_{this};

  // Used for observing closing of the pinned browser.
  std::optional<base::CallbackListSubscription> browser_close_subscription_;

  // List of callbacks to be notified when window activation has changed.
  base::RepeatingCallbackList<void(bool)> window_activation_callback_list_;

  const raw_ptr<Profile> profile_;

  // Contains the glic webview.
  std::unique_ptr<GlicWidget> glic_widget_;

  std::unique_ptr<GlicWindowAnimator> glic_window_animator_;

  // True if we've hit a login page (and have not yet shown).
  bool login_page_committed_ = false;

  // This member contains the last size that glic requested. This should be
  // reset every time glic is closed but is currently cached.
  std::optional<gfx::Size> glic_size_;

  // Whether the widget should be user resizable, kept here in case it's
  // specified before the widget is created.
  bool user_resizable_ = true;

  // Used to monitor key and mouse events from native window.
  class WindowEventObserver;
  std::unique_ptr<WindowEventObserver> window_event_observer_;

  // True while RunMoveLoop() has been called on a widget.
  bool in_move_loop_ = false;

  // This is the last panel state sent to observers. It should only be updated
  // in `NotifyIfPanelStateChanged`.
  mojom::PanelState panel_state_;

  raw_ptr<GlicWebClientAccess> web_client_;

  // Modified only by calling `SetWindowState`.
  State state_ = State::kClosed;

  // If State != kClosed, then the UI must either be associated with a browser
  // window, or standalone. That is tracked by this member.
  raw_ptr<Browser> attached_browser_ = nullptr;

  base::ObserverList<StateObserver> state_observers_;

  // Used by web modals to listens for glic window events, e.g. size change or
  // window close.
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observers_;

  // The announcement should happen the first time focus is lost after the FRE.
  bool do_focus_loss_announcement_ = false;

  // Whether the user is currently drag-resizing the widget.
  bool user_resizing_ = false;

  // The invocation source requesting the opening of the web client. Note that
  // this value is retained until it is consumed by the web client. Because
  // opening the glic window may not actually load the client, there's no
  // guarantee that this value is sent to the web client.
  std::optional<mojom::InvocationSource> opening_source_;

  std::optional<gfx::Point> previous_position_ = std::nullopt;

  std::unique_ptr<ScopedGlicButtonIndicator> scoped_glic_button_indicator_;

  std::unique_ptr<GlicFreController> fre_controller_;

  std::unique_ptr<WindowFinder> window_finder_;

  std::unique_ptr<LocalHotkeyManager> application_hotkey_manager_;
  std::unique_ptr<LocalHotkeyManager> glic_window_hotkey_manager_;

  raw_ptr<GlicKeyedService> glic_service_;  // Owns this.
  raw_ptr<GlicEnabling> enabling_;
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};

  base::WeakPtrFactory<GlicWindowControllerImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_IMPL_H_
