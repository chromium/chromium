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
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

class Browser;
namespace gfx {
class Size;
class Point;
}  // namespace gfx

namespace tabs {
class TabInterface;
}

namespace glic {
// Distance the detached window should be from the top and the right of the
// display when opened unassociated to a browser.
inline constexpr static int kDefaultDetachedTopRightDistance = 48;

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kGlicWidgetAttached);

class GlicWidget;
class GlicKeyedService;
class GlicView;
class GlicWindowAnimator;
class Host;
enum class AttachChangeReason;

// This class owns and manages the glic window. This class has the same lifetime
// as the GlicKeyedService, so it exists if and only if the profile exists.
//
// See the |State| enum below for the lifecycle of the window. When the glic
// window is open |attached_browser_| indicates if the window is attached or
// standalone. See |IsAttached|
class GlicWindowController {
 public:
  struct PanelStateContext {
    raw_ptr<Browser> attached_browser = nullptr;
    raw_ptr<views::Widget> glic_widget = nullptr;
  };

  // Observes the state of the glic panel.
  class StateObserver : public base::CheckedObserver {
   public:
    virtual void PanelStateChanged(const mojom::PanelState& panel_state,
                                   const PanelStateContext& context) = 0;
  };

  GlicWindowController(const GlicWindowController&) = delete;
  GlicWindowController& operator=(const GlicWindowController&) = delete;
  GlicWindowController() = default;
  virtual ~GlicWindowController() = default;

  virtual HostManager& host_manager() = 0;

  virtual std::vector<Host*> GetHosts() = 0;
  virtual Host* GetHostForTab(tabs::TabInterface* tab) = 0;
  // TODO(refactor): remove GetHosts and GetHostForTab and use these instead.
  virtual std::vector<GlicInstance*> GetInstances() = 0;
  virtual GlicInstance* GetInstanceForTab(tabs::TabInterface* tab) = 0;

  // Show, summon, or activate the panel if needed, or close it if it's already
  // active and prevent_close is false.
  virtual void Toggle(BrowserWindowInterface* bwi,
                      bool prevent_close,
                      mojom::InvocationSource source) = 0;

  // If the panel is opened, but sign-in is required, we provide a sign-in
  // button which closes the panel. This is called after the user signs in to
  // open the panel again.
  virtual void ShowAfterSignIn(base::WeakPtr<Browser> browser) = 0;

  // Handle Toggle when AlwaysDetached is true.
  virtual void ToggleWhenNotAlwaysDetached(Browser* new_attached_browser,
                                           bool prevent_close,
                                           mojom::InvocationSource source) = 0;

  virtual void FocusIfOpen() = 0;

  // Destroy the glic panel and its web contents.
  virtual void Shutdown() = 0;

  // Update the resize state of the widget if it is needed and safe to do so.
  // On Windows make sure that the client area size remains the same even if
  // the widget size changes because the widget is resizable.
  virtual void MaybeSetWidgetCanResize() = 0;

  // Returns the current size of the glic window.
  virtual gfx::Size GetSize() = 0;

  // Close the panel but keep the glic WebContents alive in the background.
  virtual void Close() = 0;

  // Used when the native window is closed directly.
  virtual void CloseWithReason(views::Widget::ClosedReason reason) = 0;

  // Activates the browser window that the glic panel is associated with. If
  // the panel is not attached to a browser, it will attempt to activate the
  // last active browser. Returns true if a browser was successfully
  // activated.
  virtual bool ActivateBrowser() = 0;

  // Displays a context menu when the user right clicks on the title bar.
  // This is probably Windows only.
  virtual void ShowTitleBarContextMenuAt(gfx::Point event_loc) = 0;

  // Returns true if the mouse has been dragged more than a minimum distance
  // from `initial_press_loc`, so a mouse down followed by a move of less than
  // the minimum number of pixels doesn't start a window drag.
  virtual bool ShouldStartDrag(const gfx::Point& initial_press_loc,
                               const gfx::Point& mouse_location) = 0;

  virtual const mojom::PanelState& GetPanelState() const = 0;
  virtual bool IsShowing() const = 0;

  virtual void AddStateObserver(StateObserver* observer) = 0;
  virtual void RemoveStateObserver(StateObserver* observer) = 0;

  // Returns whether the views::Widget associated with the glic window is active
  // (e.g. will receive keyboard events).
  virtual bool IsActive() = 0;

  // Returns whether or not the glic window is currently attached to a browser.
  virtual bool IsAttached() const = 0;

  // Returns wehether or not the glic window is currently showing detached.
  // When True |GetGlicWidget| will return a valid ptr.
  virtual bool IsDetached() const = 0;

  using WindowActivationChangedCallback =
      base::RepeatingCallback<void(bool active)>;

  // Registers |callback| to be called whenever the window activation changes.
  virtual base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback) = 0;

  // Warms the glic web contents.
  virtual void Preload() = 0;

  // Reloads the glic web contents or the FRE's web contents (depending on
  // which is currently visible).
  virtual void Reload() = 0;

  // Returns whether or not the glic web contents are loaded (this can also be
  // true if `IsActive()` (i.e., if the contents are loaded in the glic window).
  virtual bool IsWarmed() const = 0;

  // Returns a WeakPtr to this instance. It can be destroyed at any time if the
  // profile is deleted or if the browser shuts down.
  virtual base::WeakPtr<GlicWindowController> GetWeakPtr() = 0;

  virtual GlicView* GetGlicView() const = 0;

  virtual base::WeakPtr<views::View> GetGlicViewAsView() = 0;

  // Returns the widget that backs the glic window.
  virtual GlicWidget* GetGlicWidget() const = 0;

  virtual gfx::NativeWindow GetHostNativeWindow() = 0;

  // Return the Browser to which the panel is attached, or null if detached.
  virtual Browser* attached_browser() = 0;

  // Possible states for the glic window. Public for testing.
  //   * Closed (aka hidden, invisible)
  //   * Waiting for glic to load (the open animation has finished, but the
  //     glic window contents is not yet ready)
  //   * Open (aka showing, visible)
  //   * Detaching - the panel should not be considered open since the view
  //     might not exist.
  //   * Waiting for side panel - in the process of setting up side panel to
  //   show.
  enum class State {
    kClosed,
    kWaitingForGlicToLoad,
    kOpen,
    kDetaching,
    kWaitingForSidePanelToShow,
  };
  virtual State state() const = 0;

  virtual GlicWindowAnimator* window_animator() = 0;

  virtual Profile* profile() = 0;

  virtual gfx::Rect GetInitialBounds(Browser* browser) = 0;

  virtual void ShowDetachedForTesting() = 0;
  virtual void SetPreviousPositionForTesting(gfx::Point position) = 0;
  virtual std::unique_ptr<views::View> CreateViewForSidePanel(
      tabs::TabInterface& tab) = 0;

  virtual void SidePanelShown(BrowserWindowInterface* browser) = 0;
  // Helper function to get the always detached flag.
  static bool AlwaysDetached() {
    return base::FeatureList::IsEnabled(features::kGlicDetached);
  }

  // Register for this callback to detect changes to the floaty.
  using FloatyStateChangeCallback =
      base::RepeatingCallback<void(State, mojom::CurrentView view)>;
  virtual base::CallbackListSubscription RegisterFloatyStateChange(
      FloatyStateChangeCallback callback) = 0;
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
