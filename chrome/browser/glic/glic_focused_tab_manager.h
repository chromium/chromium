// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

class BrowserWindowInterface;

namespace glic {
// Responsible for managing which tab is considered "focused" and for accessing
// its WebContents. This is an implementation detail of GlicKeyedService and
// others should rely on the interface that GlicKeyedService exposes for
// observing state changes.
class GlicFocusedTabManager : public BrowserListObserver,
                              public content::WebContentsObserver,
                              public GlicWindowController::StateObserver,
                              public views::WidgetObserver {
 public:
  explicit GlicFocusedTabManager(Profile* profile,
                                 GlicWindowController& window_controller);
  ~GlicFocusedTabManager() override;

  GlicFocusedTabManager(const GlicFocusedTabManager&) = delete;
  GlicFocusedTabManager& operator=(const GlicFocusedTabManager&) = delete;

  // Returns the currently focused tab or nullptr if nothing is focused.
  content::WebContents* GetWebContentsForFocusedTab();

  // BrowserListObserver
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  // GlicWindowController::StateObserver
  void PanelStateChanged(const glic::mojom::PanelState& panel_state,
                         Browser*) override;

  // Callback for changes to focused tab. The web contents pointer can be null
  // if no tab is in focus.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const content::WebContents*)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback);

 private:
  // True if the immutable attributes of `browser` are valid for Glic focus.
  // Invalid browsers are never observed.
  bool IsBrowserValid(Browser* browser);

  // True if the mutable attributes of `browser` are valid for Glic focus.
  // Active browsers with invalid state are observed for state changes.
  bool IsBrowserStateValid(Browser* browser);

  // True if `web_contents` is allowed to be focused.
  bool IsValidFocusable(content::WebContents* web_contents);

  // Updates focused tab if a new one is computed. Notifies after debounce
  // threshold if updated or if `force_notify` is true for any call within the
  // duration of the debouncing.
  void MaybeUpdateFocusedTab(bool force_notify = false);

  // Updates focused tab if a new one is computed without debouncing. Prefer
  // `MaybeUpdateFocusedTab` unless debouncing must specifically be avoided.
  void PerformMaybeUpdateFocusedTab(bool force_notfiiy = false);

  // Computes the currently focused tab.
  content::WebContents* ComputeFocusedTab();

  // Computes the currently focusable tab for a given browser.
  content::WebContents* ComputeFocusableTabForBrowser(Browser* browser);

  // Calls all registered focused tab changed callbacks.
  void NotifyFocusedTabChanged();

  // Callback for active tab changes from BrowserWindowInterface.
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);

  // Callback for Glic Window activation changes.
  void OnGlicWindowActivationChanged(bool active);

  // Callback for browser window minimization changes. Required because on Mac
  // OS minimization status defaults to changing after browser's active state
  // because of animation.
  void OnWidgetShowStateChanged(views::Widget* widget) override;

  // Callback for browser window widget being destroyed.
  void OnWidgetDestroyed(views::Widget* widget) override;

  // List of callbacks to be notified when focused tab changed.
  base::RepeatingCallbackList<void(const content::WebContents*)>
      focused_callback_list_;

  // The profile for which to manage focused tabs.
  raw_ptr<Profile> profile_;

  // The Glic window controller.
  raw_ref<GlicWindowController> window_controller_;

  // The currently focused tab (or nullptr if no tab is focused).
  base::WeakPtr<content::WebContents> focused_web_contents_;

  // Callback subscription for listening to changes to active tab for a browser.
  base::CallbackListSubscription browser_subscription_;

  // Callback subscription for listening to changes to the Glic window
  // activation changes.
  base::CallbackListSubscription window_activation_subscription_;

  // WidgetObserver for triggering window minimization/maximization changes.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // One shot time used to debounce focus notifications.
  base::OneShotTimer debouncer_;

  // Cached force_notify state for carrying over across debounces. If any call
  // to MaybeUpdateFocusedTab has a forced notify, this will be set to true
  // until debouncing resolves.
  bool cached_force_notify_ = false;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_
