// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_H_

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
#include "chrome/browser/glic/widget/glic_modal_manager.h"
#include "chrome/browser/glic/widget/glic_window_hotkey_delegate.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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

// Distance the detached window should be from the top and the right of the
// display when opened unassociated to a browser.
inline constexpr static int kDefaultDetachedTopRightDistance = 48;

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kGlicWidgetAttached);

class GlicEnabling;
class GlicWidget;
class GlicKeyedService;
class GlicView;
class GlicWindowAnimator;
class ScopedGlicButtonIndicator;
class GlicFreController;
class GlicButton;
class Host;
enum class AttachChangeReason;
class GlicModalManager;

// This class owns and manages the glic window. This class has the same lifetime
// as the GlicKeyedService, so it exists if and only if the profile exists.
//
// See the |State| enum below for the lifecycle of the window. When the glic
// window is open |attached_browser_| indicates if the window is attached or
// standalone. See |IsAttached|
class GlicWindowController : public views::WidgetObserver,
                             public Host::Observer,
                             public Host::Delegate {
 public:
  // Observes the state of the glic window.
  class StateObserver : public base::CheckedObserver {
   public:
    virtual void PanelStateChanged(const mojom::PanelState& panel_state,
                                   Browser* attached_browser) = 0;
  };

  GlicWindowController(const GlicWindowController&) = delete;
  GlicWindowController& operator=(const GlicWindowController&) = delete;

  GlicWindowController(Profile* profile,
                       signin::IdentityManager* identity_manager,
                       GlicKeyedService* service,
                       GlicEnabling* enabling);
  ~GlicWindowController() override;

  // Show, summon, or activate the panel if needed, or close it if it's already
  // active and prevent_close is false.
  void Toggle(BrowserWindowInterface* browser,
              bool prevent_close,
              mojom::InvocationSource source);

  // If the panel is opened, but sign-in is required, we provide a sign-in
  // button which closes the panel. This is called after the user signs in to
  // open the panel again.
  void ShowAfterSignIn(base::WeakPtr<Browser> browser);

  // Handle Toggle when AlwaysDetached is true.
  void ToggleWhenNotAlwaysDetached(Browser* new_attached_browser,
                                   bool prevent_close,
                                   mojom::InvocationSource source);

  void FocusIfOpen();

  // Attaches glic to the last focused Chrome window.
  void Attach();

  // Detaches glic if attached and moves it to the top right of the current
  // display.
  void Detach();

  // Destroy the glic panel and its web contents.
  void Shutdown();

  // Sets the size of the glic window to the specified dimensions. Callback runs
  // when the animation finishes or is destroyed, or soon if the window
  // doesn't exist yet. In this last case `size` will be used for the initial
  // size when creating the widget later.
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback);

  // Allows the user to manually resize the widget by dragging. If the widget
  // hasn't been created yet, apply this setting when it is created. No effect
  // if the widget doesn't exist or the feature flag is disabled.
  void EnableDragResize(bool enabled);

  // Returns the current size of the glic window.
  gfx::Size GetSize();

  // Sets the areas of the view from which it should be draggable.
  void SetDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);

  // Sets the minimum widget size that the widget will allow the user to resize
  // to.
  void SetMinimumWidgetSize(const gfx::Size& size);

  // Close the panel but keep the glic WebContents alive in the background.
  void Close();

  // Used when the native window is closed directly.
  void CloseWithReason(views::Widget::ClosedReason reason);

  // Sets the audio ducking status.  Returns true if the operation succeeded.
  bool SetAudioDucking(bool enabled);

  // Displays a context menu when the user right clicks on the title bar.
  // This is probably Windows only.
  void ShowTitleBarContextMenuAt(gfx::Point event_loc);

  // Returns true if the mouse has been dragged more than a minimum distance
  // from `initial_press_loc`, so a mouse down followed by a move of less than
  // the minimum number of pixels doesn't start a window drag.
  bool ShouldStartDrag(const gfx::Point& initial_press_loc,
                       const gfx::Point& mouse_location);

  // Drags the glic window following the current mouse location with the given
  // `mouse_offset` and checks if the glic window is at a position where it
  // could attach to a browser window when a drag ends.
  void HandleWindowDragWithOffset(gfx::Vector2d mouse_offset);

  // Host::Delegate implementation.
  const mojom::PanelState& GetPanelState() const override;

  void AddStateObserver(StateObserver* observer);
  void RemoveStateObserver(StateObserver* observer);

  // Returns whether the views::Widget associated with the glic window is active
  // (e.g. will receive keyboard events).
  bool IsActive();

  // Returns true if the state is anything other than kClosed.
  // Virtual for testing.
  virtual bool IsShowing() const;

  // Returns true if either the glic panel or the FRE are showing.
  virtual bool IsPanelOrFreShowing() const;

  // Returns whether or not the glic window is currently attached to a browser.
  // Virtual for testing.
  virtual bool IsAttached() const;

  // Returns wehether or not the glic window is currently showing detached.
  bool IsDetached() const;

  using WindowActivationChangedCallback =
      base::RepeatingCallback<void(bool active)>;

  // Registers |callback| to be called whenever the window activation changes.
  base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback);

  // Warms the glic web contents.
  void Preload();

  // Warms the fre web contents.
  void PreloadFre();

  // Reloads the glic web contents or the FRE's web contents (depending on
  // which is currently visible).
  void Reload();

  // Returns whether or not the glic web contents are loaded (this can also be
  // true if `IsActive()` (i.e., if the contents are loaded in the glic window).
  bool IsWarmed() const;

  // Returns a WeakPtr to this instance. It can be destroyed at any time if the
  // profile is deleted or if the browser shuts down.
  base::WeakPtr<GlicWindowController> GetWeakPtr();

  // views::WidgetObserver implementation, monitoring the glic window widget.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetUserResizeStarted() override;
  void OnWidgetUserResizeEnded() override;

  GlicView* GetGlicView();

  // Called when the programmatic resize has finished. Public for use by
  // GlicWindowResizeAnimation.
  void ResizeFinished();

  // Returns the widget that backs the glic window.
  GlicWidget* GetGlicWidget();

  // Returns the WebContents used for the first-run experience, or nullptr if
  // none.
  content::WebContents* GetFreWebContents();

  // Return the Browser to which the panel is attached, or null if detached.
  Browser* attached_browser() { return attached_browser_; }

  // Possible states for the glic window. Public for testing.
  //   * Closed (aka hidden, invisible)
  //   * OpenAnimation (showing an animation built into chrome, independent of
  //     the content of the glic window)
  //   * Waiting for glic to load (the open animation has finished, but the
  //     glic window contents is not yet ready)
  //   * Open (aka showing, visible)
  //   * CloseAnimation
  //   * Detaching
  //   * ClosingToReopenDetached
  enum class State {
    kClosed,
    kWaitingForGlicToLoad,
    kOpen,
    kDetaching,
    kClosingToReopenDetached,
    kCloseAnimation,
  };
  State state() const { return state_; }

  void ShowDetachedForTesting();

  GlicFreController* fre_controller() { return fre_controller_.get(); }

  GlicWindowAnimator* window_animator() { return glic_window_animator_.get(); }

  Profile* profile() { return profile_; }

  // Helper function to get the always detached flag.
  static bool AlwaysDetached();

  bool IsDragging() { return in_move_loop_; }

  void ShowGlicModal(std::u16string label);

 private:
  FRIEND_TEST_ALL_PREFIXES(GlicWindowControllerUiTest, TestInitialBounds);
  FRIEND_TEST_ALL_PREFIXES(GlicWindowControllerWithPreviousPostionUiTest,
                           TestInitialBounds);

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

  gfx::Rect GetInitialBounds(Browser* browser);

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

  // Close the widget and reopen in detached mode.
  void CloseAndReopenDetached(mojom::InvocationSource source);

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

  // Internal closing implementation. reopen_detached_source must be set
  // if and only if the internal state is kClosingToReopenDetached.
  void CloseInternal(
      std::optional<mojom::InvocationSource> reopen_detached_source);

  // Finishes closing off the widget after running the closing animation.
  void CloseFinish(
      bool reopen_detached,
      std::optional<mojom::InvocationSource> reopen_detached_source);

  // Called when the Detach() animation ends.
  void DetachFinished();

  // Causes an immediate close (eg, for during shutdown).
  void ForceClose();

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

  std::unique_ptr<GlicModalManager> glic_modal_manager_;

  std::unique_ptr<LocalHotkeyManager> application_hotkey_manager_;
  std::unique_ptr<LocalHotkeyManager> glic_window_hotkey_manager_;

  raw_ptr<GlicKeyedService> glic_service_;  // Owns this.
  raw_ptr<GlicEnabling> enabling_;
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};

  base::WeakPtrFactory<GlicWindowController> weak_ptr_factory_{this};
};

}  // namespace glic

namespace base {

template <>
struct ScopedObservationTraits<glic::GlicWindowController,
                               glic::GlicWindowController::StateObserver> {
  static void AddObserver(glic::GlicWindowController* source,
                          glic::GlicWindowController::StateObserver* observer) {
    source->AddStateObserver(observer);
  }
  static void RemoveObserver(
      glic::GlicWindowController* source,
      glic::GlicWindowController::StateObserver* observer) {
    source->RemoveStateObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_H_
