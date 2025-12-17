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

namespace content {
class RenderFrameHost;
}
namespace gfx {
class Point;
}  // namespace gfx

namespace tabs {
class TabInterface;
}

namespace glic {
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kGlicWidgetAttached);

class GlicWidget;
class GlicKeyedService;
enum class AttachChangeReason;

// MIGRATION IN PROGRESS - WARNING
//
// GlicWindowController is a misleading name!
//
// GlicWindowController exists as a temporary compatibility interface
// implemented by GlicWindowControllerImpl and GlicInstanceCoordinatorImpl.
class GlicWindowController {
 public:
  using StateObserver = PanelStateObserver;
  using PanelStateContext = ::glic::PanelStateContext;
  GlicWindowController(const GlicWindowController&) = delete;
  GlicWindowController& operator=(const GlicWindowController&) = delete;
  GlicWindowController() = default;
  virtual ~GlicWindowController() = default;

  virtual HostManager& host_manager() = 0;
  virtual std::vector<GlicInstance*> GetInstances() = 0;
  virtual GlicInstance* GetInstanceForTab(
      const tabs::TabInterface* tab) const = 0;

  // Show, summon, or activate the panel if needed, or close it if it's already
  // active and prevent_close is false.
  virtual void Toggle(BrowserWindowInterface* bwi,
                      bool prevent_close,
                      mojom::InvocationSource source,
                      std::optional<std::string> prompt_suggestion) = 0;

  // If the panel is opened, but sign-in is required, we provide a sign-in
  // button which closes the panel. This is called after the user signs in to
  // open the panel again.
  virtual void ShowAfterSignIn(base::WeakPtr<Browser> browser) = 0;

  // Destroy the glic panel and its web contents.
  virtual void Shutdown() = 0;

  // Close the panel but keep the glic WebContents alive in the background.
  virtual void Close() = 0;
  // Closes the active embedder of an instance with matching render_frame_host
  // without resetting webcontents.
  virtual void CloseInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) = 0;
  // Closes the active embedder of an instance with matching render_frame_host
  // with resetting webcontents.
  virtual void CloseAndShutdownInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) = 0;

  // Returns wehether or not the glic window is currently showing detached.
  // When True |GetGlicWidget| will return a valid ptr.
  virtual bool IsDetached() const = 0;

  // Returns whether the given browser is showing a glic panel for its active
  // tab.
  virtual bool IsPanelShowingForBrowser(
      const BrowserWindowInterface& bwi) const = 0;

  using WindowActivationChangedCallback =
      base::RepeatingCallback<void(bool active)>;

  // Registers |callback| to be called whenever the window activation changes.
  virtual base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback) = 0;

  // Registers a callback to be run when any instance opens or closes.
  virtual base::CallbackListSubscription AddGlobalShowHideCallback(
      base::RepeatingClosure callback) = 0;

  // Warms the glic web contents.
  virtual void Preload() = 0;

  // Reloads the glic web contents or the FRE's web contents (depending on
  // which is currently visible).
  virtual void Reload(content::RenderFrameHost* render_frame_host) = 0;

  // Returns the widget that backs the glic window.
  virtual GlicWidget* GetGlicWidget() const = 0;

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

  virtual Profile* profile() = 0;

  virtual gfx::Rect GetInitialBounds(Browser* browser) = 0;

  virtual void ShowDetachedForTesting() = 0;
  virtual void SetPreviousPositionForTesting(gfx::Point position) = 0;

  // TODO: Move to GlicInstanceCoordinator.
  using ActiveInstanceChangedCallback =
      base::RepeatingCallback<void(GlicInstance* new_instance)>;
  virtual base::CallbackListSubscription
  AddActiveInstanceChangedCallbackAndNotifyImmediately(
      ActiveInstanceChangedCallback callback) = 0;
  virtual GlicInstance* GetActiveInstance() = 0;

  // Helper function to return whether the kGlicDetached feature is enabled
  // while multi-instance is not.
  static bool AlwaysDetached() {
    return base::FeatureList::IsEnabled(features::kGlicDetached) &&
           !GlicEnabling::IsMultiInstanceEnabled();
  }

  // Same as GlicInstance::AddStateObserver, but applies globally, provides
  // the aggregate panel state or the last active panel state.
  // TODO(cuianthony): Implement for multi-instance and update this
  // documentation.
  virtual void AddGlobalStateObserver(PanelStateObserver* observer) = 0;
  virtual void RemoveGlobalStateObserver(PanelStateObserver* observer) = 0;
};

// This class owns and manages the glic window. This class has the same lifetime
// as the GlicKeyedService, so it exists if and only if the profile exists.
//
// See the |State| enum below for the lifecycle of the window. When the glic
// window is open |attached_browser_| indicates if the window is attached or
// standalone. See |IsAttached|
class GlicWindowControllerInterface : public GlicWindowController,
                                      public GlicInstance {
 public:
  // Returns a WeakPtr to this instance. It can be destroyed at any time if the
  // profile is deleted or if the browser shuts down.
  virtual base::WeakPtr<GlicWindowControllerInterface> GetWeakPtr() = 0;

  // Returns whether or not the glic web contents are loaded (this can also be
  // true if `IsActive()` (i.e., if the contents are loaded in the glic window).
  virtual bool IsWarmed() const = 0;

  virtual void SidePanelShown(BrowserWindowInterface* browser) = 0;
  virtual std::unique_ptr<views::View> CreateViewForSidePanel(
      tabs::TabInterface& tab) = 0;

  // Update the resize state of the widget if it is needed and safe to do so.
  // On Windows make sure that the client area size remains the same even if
  // the widget size changes because the widget is resizable.
  virtual void MaybeSetWidgetCanResize() = 0;

};

}  // namespace glic

namespace base {

template <>
struct ScopedObservationTraits<glic::GlicWindowController,
                               glic::PanelStateObserver> {
  static void AddObserver(glic::GlicWindowController* source,
                          glic::PanelStateObserver* observer) {
    source->AddGlobalStateObserver(observer);
  }
  static void RemoveObserver(glic::GlicWindowController* source,
                             glic::PanelStateObserver* observer) {
    source->RemoveGlobalStateObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_CONTROLLER_H_
