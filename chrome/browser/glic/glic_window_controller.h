// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_web_client_access.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/unique_widget_ptr.h"

class Browser;
namespace gfx {
class Size;
class Point;
}  // namespace gfx

namespace glic {

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kGlicWidgetAttached);

class GlicView;
class WebUIContentsContainer;
class GlicWindowResizeAnimation;
class ScopedGlicButtonIndicator;

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
class GlicWindowController : public views::WidgetObserver {
 public:
  // Observes the state of the glic window.
  class StateObserver : public base::CheckedObserver {
   public:
    virtual void PanelStateChanged(const mojom::PanelState& panel_state) = 0;
  };

  GlicWindowController(const GlicWindowController&) = delete;
  GlicWindowController& operator=(const GlicWindowController&) = delete;

  explicit GlicWindowController(Profile* profile);
  ~GlicWindowController() override;

  // Show, summon, or activate the panel if needed, or close it if it's already
  // active.
  void Toggle(BrowserWindowInterface* browser);

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

  // Returns the current size of the glic window.
  gfx::Size GetSize();

  // Sets the areas of the view from which it should be draggable.
  void SetDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);

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

  // Returns whether the views::Widget associated with the glic window is active
  // (e.g. will receive keyboard events).
  bool IsActive();

  // Returns true if the state is anything other than kClosed.
  bool IsShowing() const;

  // Returns whether or not the glic window is currently attached to a browser.
  bool IsAttached();

  using WindowActivationChangedCallback =
      base::RepeatingCallback<void(bool active)>;

  // Registers |callback| to be called whenever the window activation changes.
  base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback);

  // Warms the glic web contents.
  void Preload();

  // Returns whether or not the glic web contents are loaded (this can also be
  // true if `IsActive()` (i.e., if the contents are loaded in the glic window).
  bool IsWarmed();

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

  // views::WidgetObserver implementation, monitoring the attached browser.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  GlicView* GetGlicView();

  // Called when the programmatic resize has finished. Public for use by
  // GlicWindowResizeAnimation.
  void ResizeFinished();

  // Returns the widget that backs the glic window.
  views::Widget* GetGlicWidget();

  // Returns the WebContents hosted in the glic window, or nullptr if none.
  content::WebContents* GetWebContents();

  // Return the Browser to which the panel is attached, or null if detached.
  Browser* GetAttachedBrowserForTesting() { return attached_browser_; }

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

 private:
  gfx::Rect GetInitialDetachedBounds();

  // Performs initialization for the attached/detached opening flows. Important
  // difference: currently attached has an animation, so we immediately show the
  // widget. Detached does not have an animation, and we wait until glic is
  // ready to show anything.
  void OpenAttached(Browser* browser);
  void OpenDetached();

  // Creates the glic view, waits for the web client to initialize, and then
  // shows the glic window. If `browser` is non-nullptr then glic will be
  // attached to the browser. Otherwise glic will be detached.
  void Show(Browser* browser);

  // Close the widget and reopen in detached mode.
  void CloseAndReopenDetached();

  // This sends a message to glic to get ready to show. This will eventually
  // result in the callback GlicLoaded().
  void WaitForGlicToLoad();
  void GlicLoaded(mojom::WebClientMode starting_mode);

  // Called when the open animation is finished.
  void OpenAnimationFinished();

  // TODO(crbug.com/391402352): This method is misnamed. It's used to send
  // coordinate showing the window when glic and this class are both ready.
  // However this class already shows the window via animation.
  void ShowFinish();

  // Finishes closing off the widget after running the closing animation.
  void CloseFinish(bool reopen_detached);

  // Called when the Detach() animation ends.
  void DetachFinished();

  // Causes an immediate close (eg, for during shutdown).
  void ForceClose();

  // Determines the correct position for the glic window when attached to a
  // browser window.
  gfx::Point GetTopRightPositionForAttachedGlicWindow(
      views::View* glic_button_view);

  // Determines the correct initial position for the glic window when in a
  // detached state.
  gfx::Point GetTopRightPositionForDetachedGlicWindow();

  // Reparents the glic widget under 'browser'.
  void AttachToBrowser(Browser* browser);

  // If glic is within attachment distance of a browser window's glic
  // button, attach the glic window to the button's position.
  void HandleAttachmentToBrowserWindows();

  // Finds a browser within attachment distance of glic to toggle the attachment
  // indicator.
  void HandleGlicButtonIndicator();

  // Find and return a browser within attachment distance. Returns nullptr if no
  // browsers are within attachment distance.
  Browser* FindBrowserForAttachment();

  // Reparents the glic window to an empty holder Widget when in a detached
  // state. Initializes the holder widget if it hasn't been created yet.
  void MaybeCreateHolderWindowAndReparent();

  // Updates the position of the glic window to that of the glic button of
  // `browser`'s window. This position change is animated if `animate` is true.
  void MovePositionToBrowserGlicButton(Browser* browser, bool animate);

  // Checks if 'browser' is compatible with glic.
  bool IsBrowserGlicCompatible(Browser* browser);

  // This method should be called anytime:
  //  * state_ transitions to or from kClosed.
  //  * attached_browser_ changes.
  void NotifyIfPanelStateChanged();
  mojom::PanelState ComputePanelState() const;

  // When the attached browser is closed, this is invoked so we can clean up.
  void AttachedBrowserDidClose(BrowserWindowInterface* browser);

  // Sets target bounds for the widget and creates a WindowResizeAnimation
  // instance to begin a new animation. Blocks any calls to animate if the
  // widget it not yet visible.
  void AnimateBounds(const gfx::Rect& target_bounds,
                     base::TimeDelta duration,
                     base::OnceClosure callback);

  // Convenience method for AnimateBounds that takes a gfx::Size and maintains
  // the top-right corner.
  void AnimateSize(const gfx::Size& target_size,
                   base::TimeDelta duration,
                   base::OnceClosure callback);

  // Creates the glic widget.
  std::unique_ptr<views::Widget> CreateGlicWidget(
      Profile* profile,
      const gfx::Rect& initial_bounds);

  void ResetPresentationTimingState();

  // Observes the widget for the attached browser.
  base::ScopedObservation<views::Widget, WidgetObserver>
      attached_browser_widget_observation_{this};

  // Observes the glic widget.
  base::ScopedObservation<views::Widget, WidgetObserver>
      glic_widget_observation_{this};

  // Used for observing closing of the pinned browser.
  std::optional<base::CallbackListSubscription> browser_close_subscription_;

  // List of callbacks to be notified when window activation has changed.
  base::RepeatingCallbackList<void(bool)> window_activation_callback_list_;

  // Empty holder widget to reparent to when detached.
  std::unique_ptr<views::Widget> holder_widget_;

  const raw_ptr<Profile> profile_;
  // Keep profile alive as long as the glic web contents. This object should be
  // destroyed when the profile needs to be destroyed.
  std::unique_ptr<WebUIContentsContainer> contents_;

  // Contains the glic webview. In the attached state the parent is set to a
  // browser window. In the detached state the parent is set to holder_widget_.
  std::unique_ptr<views::Widget> glic_widget_;

  std::unique_ptr<GlicWindowResizeAnimation> window_resize_animation_;

  // True if we've hit a login page (and have not yet shown).
  bool login_page_committed_ = false;

  // This member contains the last size that glic requested. This should be
  // reset every time glic is closed but is currently cached.
  std::optional<gfx::Size> glic_size_;

  // Used to monitor key and mouse events from native window.
  class WindowEventObserver;
  std::unique_ptr<WindowEventObserver> window_event_observer_;

  // True while RunMoveLoop() has been called on a widget.
  bool in_move_loop_ = false;

  // This is the last panel state sent to observers. It should only be updated
  // in `NotifyIfPanelStateChanged`.
  mojom::PanelState panel_state_;

  raw_ptr<GlicWebClientAccess> web_client_;

  State state_ = State::kClosed;

  // If State != kClosed, then the UI must either be associated with a browser
  // window, or standalone. That is tracked by this member.
  raw_ptr<Browser> attached_browser_ = nullptr;

  // Set to true when glic is ready.
  bool glic_loaded_ = false;

  base::ObserverList<StateObserver> state_observers_;

  // The following two variables are used together for recording metrics and are
  // reset together after the panel show is finished. The timestamp when the
  // glic window starts to be shown.
  base::TimeTicks show_start_time_;
  // Web client's operation modes.
  mojom::WebClientMode starting_mode_;

  std::unique_ptr<ScopedGlicButtonIndicator> scoped_glic_button_indicator_;

  base::WeakPtrFactory<GlicWindowController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
