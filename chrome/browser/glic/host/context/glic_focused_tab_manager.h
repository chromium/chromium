// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
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

  // Returns the currently focused tab data or an error reason stating why one
  // was not available. This may also contain a tab candidate along with details
  // as to why it cannot be focused. Virtual for testing.
  virtual FocusedTabData GetFocusedTabData();

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  // GlicWindowController::StateObserver
  void PanelStateChanged(const glic::mojom::PanelState& panel_state,
                         Browser*) override;

  // Callback for changes to focused tab. If no tab is in focus an error reason
  // is returned indicating why and maybe a tab candidate with details as to
  // why it cannot be focused.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(FocusedTabData)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback);

  // Callback for changes to the `WebContents` comprising the focused tab. Only
  // fired when the `WebContents` for the focused tab changes to/from nullptr or
  // to different `WebContents` instance.
  using FocusedTabInstanceChangedCallback =
      base::RepeatingCallback<void(content::WebContents*)>;
  base::CallbackListSubscription AddFocusedTabInstanceChangedCallback(
      FocusedTabInstanceChangedCallback callback);

  // Callback for changes to either the focused tab or the focused tab candidate
  // instances. If no tab is in focus an error reason is returned indicating
  // why and maybe a tab candidate with details as to why it cannot be focused.
  using FocusedTabOrCandidateInstanceChangedCallback =
      base::RepeatingCallback<void(FocusedTabData)>;
  base::CallbackListSubscription
  AddFocusedTabOrCandidateInstanceChangedCallback(
      FocusedTabOrCandidateInstanceChangedCallback callback);

  // Callback for changes to the tab data representation of the focused tab.
  // This includes any event that changes tab data -- e.g. favicon/title change
  // events (where the container does not change), as well as container changed
  // events.
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback);

 private:
  // Internal state for tracking focused tab. If a "candidate" browser/tab
  // exists, but not a corresponding "focused" browser/tab it means that one or
  // more temporary state conditions precluded the candidate from becoming
  // focused. If no candidate exists, it means that one or more permanent
  // conditions precluded the browser/tab from even being considered a
  // candidate.
  // Note: We use WeakPtrs because at times we intentionally delay sending
  // events for debouncing, but that means we know we might be holding a dead
  // pointer.
  struct FocusedTabState {
    FocusedTabState();
    ~FocusedTabState();
    FocusedTabState(const FocusedTabState& src);
    FocusedTabState& operator=(const FocusedTabState& src);

    bool IsSame(const FocusedTabState& other) const;

    base::WeakPtr<BrowserWindowInterface> candidate_browser;
    base::WeakPtr<BrowserWindowInterface> focused_browser;
    base::WeakPtr<content::WebContents> candidate_tab;
    base::WeakPtr<content::WebContents> focused_tab;
  };

  // Returns whether `a` and `b` both point to the same object.
  // Note that if both `a` and `b` are invalidated, this returns true, even if
  // the object they once pointed to is different. For our purposes, this is OK.
  template <typename T>
  static bool IsWeakPtrSame(base::WeakPtr<T> a, base::WeakPtr<T> b) {
    return std::make_pair(a.get(), a.WasInvalidated()) ==
           std::make_pair(b.get(), b.WasInvalidated());
  }

  static FocusedTabData GetFocusedTabData(
      const GlicFocusedTabManager::FocusedTabState& focused_state);

  // True if the immutable attributes of `browser` are valid for Glic focus.
  // Invalid browsers are never observed.
  bool IsBrowserValid(BrowserWindowInterface* browser_interface);

  // True if the mutable attributes of `browser` are valid for Glic focus.
  // Active browsers with invalid state are observed for state changes.
  bool IsBrowserStateValid(BrowserWindowInterface* browser_interface);

  // True if the immutable attributes of `web_contents` are valid for Glic
  // focus.
  bool IsTabValid(content::WebContents* web_contents);

  // True if the mutable attributes of `web_contents` are valid for Glic focus.
  bool IsTabStateValid(content::WebContents* web_contents);

  // Observes the active tab for `browser` if valid.
  void MaybeObserveActiveTab(BrowserWindowInterface* browser_interface);

  // Updates focused tab if a new one is computed. Notifies if updated or if
  // `force_notify` is true (for any call within the duration of the optional
  // debouncing).
  void MaybeUpdateFocusedTab(bool force_notify = false, bool debounce = false);

  // Updates focused tab if a new one is computed without debouncing. Use
  // `MaybeUpdateFocusedTab` instead of calling this directly.
  void PerformMaybeUpdateFocusedTab(bool force_notify = false);

  // Computes the currently focused tab.
  struct FocusedTabState ComputeFocusedTabState();

  // Computes the current browser candidate for focus (if any).
  BrowserWindowInterface* ComputeBrowserCandidate();

  // Computes the current tab candidate for focus (if any) for a given browser.
  content::WebContents* ComputeTabCandidate(
      BrowserWindowInterface* browser_interface);

  // Calls all registered focused tab changed callbacks.
  void NotifyFocusedTabChanged();

  // Calls all registered focused tab instance changed callbacks.
  void NotifyFocusedTabInstanceChanged(content::WebContents* web_contents);

  // Calls all registered focused tab or candidate instance changed callbacks.
  void NotifyFocusedTabOrCandidateInstanceChanged(
      FocusedTabData focused_tab_data);

  // Calls all registered focused tab data changed callbacks.
  void NotifyFocusedTabDataChanged(glic::mojom::TabDataPtr tab_data);

  // Callback for active browser changes from BrowserWindowInterface.
  void OnBrowserBecameActive(BrowserWindowInterface* browser_interface);
  void OnBrowserBecameInactive(BrowserWindowInterface* browser_interface);

  // Callback for active tab changes from BrowserWindowInterface.
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);

  // Callback for Glic Window activation changes.
  void OnGlicWindowActivationChanged(bool active);

  // Callback for browser window minimization changes. Required because on Mac
  // OS minimization status defaults to changing after browser's active state
  // because of animation.
  void OnWidgetShowStateChanged(views::Widget* widget) override;

  // Callback for browser window visibility changes (e.g. cmd+h on Mac).
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // Callback for visibility on screen changes (e.g. Spaces on Mac).
  void OnWidgetVisibilityOnScreenChanged(views::Widget* widget,
                                         bool visible) override;

  // Callback for browser window widget being destroyed.
  void OnWidgetDestroyed(views::Widget* widget) override;

  // Callback for tab data changes to focused tab.
  void FocusedTabDataChanged(glic::mojom::TabDataPtr tab_data);

  // List of callbacks to be notified when focused tab changed.
  base::RepeatingCallbackList<void(FocusedTabData)> focused_callback_list_;

  // List of callbacks to be notified when focused tab instance changed.
  base::RepeatingCallbackList<void(content::WebContents*)>
      focused_instance_callback_list_;

  // List of callbacks to be notified when focused tab or candidate instances
  // changed.
  base::RepeatingCallbackList<void(FocusedTabData)>
      focused_or_candidate_instance_callback_list_;

  // List of callbacks to be notified when focused tab data changed.
  base::RepeatingCallbackList<void(const glic::mojom::TabData*)>
      focused_data_callback_list_;

  // The profile for which to manage focused tabs.
  raw_ptr<Profile> profile_;

  // The Glic window controller.
  raw_ref<GlicWindowController> window_controller_;

  // The currently focused tab data.
  FocusedTabData focused_tab_data_;

  // `TabDataObserver` for the currently focused tab (if one exists).
  std::unique_ptr<TabDataObserver> focused_tab_data_observer_;

  // The last known focused tab state.
  struct FocusedTabState focused_tab_state_;

  // Callback subscription for listening to changes to the Glic window
  // activation changes.
  base::CallbackListSubscription window_activation_subscription_;

  // Callback subscription for listening to changes from compliant browsers.
  std::map<BrowserWindowInterface*, std::vector<base::CallbackListSubscription>>
      browser_subscriptions_;

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
#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_H_
