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
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_tracker.h"
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
class WebUIContentsContainer;
class GlicWindowAnimator;
class ScopedGlicButtonIndicator;
class GlicFreController;
class GlicButton;
enum class AttachChangeReason;

// This class owns and manages the glic window. This class has the same lifetime
// as the GlicKeyedService, so it exists if and only if the profile exists.
//
// There are 4 states for the glic window:
//   * Closed (aka hidden, invisible)
//   * OpenAnimation (showing an animation built into chrome, independent of the
//     content of the glic window)
//   * Waiting (the open animation has finished, but glic window contents is
//     not yet ready)
//   * Open (aka showing, visible)
// When the glic window is open there is an additional piece of state. The glic
// window is either attached to a Browser* or standalone.
//
class GlicWindowController : public views::WidgetObserver,
                             public ui::AcceleratorTarget {
 public:
  // Observes the state of the glic window.
  class StateObserver : public base::CheckedObserver {
   public:
    virtual void PanelStateChanged(const mojom::PanelState& panel_state,
                                   Browser* attached_browser) = 0;
  };

  // Observes the state of the WebUI hosted in the glic window.
  class WebUiStateObserver : public base::CheckedObserver {
   public:
    virtual void WebUiStateChanged(mojom::WebUiState state) = 0;
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
  void ShowAfterSignIn();

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

  const mojom::PanelState& GetPanelState() const { return panel_state_; }
  void AddStateObserver(StateObserver* observer);
  void RemoveStateObserver(StateObserver* observer);

  const mojom::WebUiState& GetWebUiState() const { return webui_state_; }
  void AddWebUiStateObserver(WebUiStateObserver* observer);
  void RemoveWebUiStateObserver(WebUiStateObserver* observer);

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

  void WebClientInitializeFailed();
  // The webview reached a login page.
  void LoginPageCommitted();
  void SetWebClient(GlicWebClientAccess* web_client);
  GlicWebClientAccess* web_client() const { return web_client_; }

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
  views::Widget* GetGlicWidget();

  // Returns the WebContents hosted in the glic window, or nullptr if none.
  content::WebContents* GetWebContents();

  // Returns the WebContents used for the first-run experience, or nullptr if
  // none.
  content::WebContents* GetFreWebContents();

  // Return the Browser to which the panel is attached, or null if detached.
  Browser* attached_browser() { return attached_browser_; }

  // See class comment for details. Public for testing.
  enum class State {
    kClosed,
    kOpenAnimation,
    kWaitingForGlicToLoad,
    kOpen,
    kDetaching,
    kClosingToReopenDetached,
    kCloseAnimation,
  };
  State state() const { return state_; }

  void ShowDetachedForTesting();

  void WebUiStateChanged(mojom::WebUiState new_state);

  GlicFreController* fre_controller() { return fre_controller_.get(); }

  GlicWindowAnimator* window_animator() { return glic_window_animator_.get(); }

  Profile* profile() { return profile_; }

  // Helper function to get the always detached flag.
  static bool AlwaysDetached();

  bool IsDragging() { return in_move_loop_; }

 private:
  // ui::AcceleratorTarget
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  void AddAccelerators();

  gfx::Rect GetInitialDetachedBounds();

  // Get the default detached bounds relative to browser.
  gfx::Rect GetInitialDetachedBoundsFromBrowser(Browser* browser);

  // Performs initialization for the attached/detached opening flows. Important
  // difference: currently attached has an animation, so we immediately show the
  // widget. Detached does not have an animation, and we wait until glic is
  // ready to show anything.
  void OpenAttached(Browser& browser);

  // Open detached relative to the browser or in the default detached position
  // if browser is a nullptr.
  void OpenDetached(Browser* browser);

  // Creates the glic view, waits for the web client to initialize, and then
  // shows the glic window. If `browser` is non-nullptr then glic will be
  // attached to the browser. Otherwise glic will be detached.
  void Show(Browser* browser, mojom::InvocationSource source);

  // Close the widget and reopen in detached mode.
  void CloseAndReopenDetached(mojom::InvocationSource source);

  void AuthCheckDoneBeforeShow(base::WeakPtr<Browser> browser_for_attachment,
                               AuthController::BeforeShowResult result);
  // This sends a message to glic to get ready to show. This will eventually
  // result in the callback GlicLoaded().
  void WaitForGlicToLoad();
  void GlicLoaded(mojom::OpenPanelInfoPtr open_info);

  // Called when the open animation is finished.
  void OpenAnimationFinished();

  // TODO(crbug.com/391402352): This method is misnamed. It's used to send
  // coordinate showing the window when glic and this class are both ready.
  // However this class already shows the window via animation.
  void ShowFinish();

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

  // Determines the correct position for the glic window when attached to a
  // browser window. The top right of the widget should be placed here.
  gfx::Point GetTopRightPositionForAttachedGlicWindow(GlicButton* glic_button);

  // Reparents the glic widget under 'browser' and runs an animation to move it
  // to its target position.
  void AttachToBrowser(Browser& browser, AttachChangeReason reason);

  // Clamp the mouse drag offsets to keep glic within the visible region.
  gfx::Vector2d GetClampedMouseDragOffset(const gfx::Vector2d& mouse_offset);

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

  // Reparents the glic window to an empty holder Widget when in a detached
  // state. Initializes the holder widget if it hasn't been created yet.
  void MaybeCreateHolderWindowAndReparent(AttachChangeReason reason);

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
  // the minimum and maximum sizes (max height is calculated from
  // `display_height`).
  gfx::Size GetLastRequestedSizeClamped(int display_height) const;

  // Possibly adjusts the size of the window appropriate for the current
  // display workspace, but only if it's different than the current target size.
  void MaybeAdjustSizeForDisplay(bool animate);

  // Create a GlicWidget.
  std::unique_ptr<GlicWidget> CreateGlicWidget(const gfx::Rect& bounds);

  // Warms the web client and sets `contents_`.
  void CreateContents();

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

#if !BUILDFLAG(IS_MAC)
  // Empty holder widget to reparent to when detached.
  std::unique_ptr<views::Widget> holder_widget_;
#endif

  const raw_ptr<Profile> profile_;
  // Keep profile alive as long as the glic web contents. This object should be
  // destroyed when the profile needs to be destroyed.
  std::unique_ptr<WebUIContentsContainer> contents_;

  // Contains the glic webview. In the attached state the parent is set to a
  // browser window. In the detached state the parent is set to holder_widget_.
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

  // This class observes the anchor view in attached mode and moves the glic
  // window to the desired position.
  class AnchorObserver;
  std::unique_ptr<AnchorObserver> anchor_observer_;

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

  // Set to true when glic is ready.
  bool glic_loaded_ = false;

  base::ObserverList<StateObserver> state_observers_;

  mojom::WebUiState webui_state_ = mojom::WebUiState::kUninitialized;
  base::ObserverList<WebUiStateObserver> webui_state_observers_;

  // The invocation source requesting the opening of the web client. Note that
  // this value is retained until it is consumed by the web client. Because
  // opening the glic window may not actually load the client, there's no
  // guarantee that this value is sent to the web client.
  std::optional<mojom::InvocationSource> opening_source_;

  std::unique_ptr<ScopedGlicButtonIndicator> scoped_glic_button_indicator_;

  std::unique_ptr<GlicFreController> fre_controller_;

  std::unique_ptr<WindowFinder> window_finder_;

  raw_ptr<GlicKeyedService> glic_service_;  // Owns this.
  raw_ptr<GlicEnabling> enabling_;

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
