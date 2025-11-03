// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_H_

#include <variant>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager_interface.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

class BrowserWindowInterface;

namespace glic {

class GlicFocusedBrowserManager;

// Responsible for managing which tab is considered "focused" and for accessing
// its WebContents. This is an implementation detail of GlicKeyedService and
// others should rely on the interface that GlicKeyedService exposes for
// observing state changes.
class GlicFocusedTabManager : public GlicFocusedTabManagerInterface,
                              public content::WebContentsObserver,
                              public TabStripModelObserver {
 public:
  explicit GlicFocusedTabManager(
      GlicFocusedBrowserManager* focused_browser_manager);
  ~GlicFocusedTabManager() override;

  GlicFocusedTabManager(const GlicFocusedTabManager&) = delete;
  GlicFocusedTabManager& operator=(const GlicFocusedTabManager&) = delete;

  // GlicFocusedTabManagerInterface implementation.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override;
  FocusedTabData GetFocusedTabData() override;
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) override;
  bool IsTabFocused(tabs::TabHandle tab_handle) const override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  // TabStripModelObserver
  void OnSplitTabChanged(const SplitTabChange& change) override;


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
      base::RepeatingCallback<void(const FocusedTabData&)>;
  base::CallbackListSubscription
  AddFocusedTabOrCandidateInstanceChangedCallback(
      FocusedTabOrCandidateInstanceChangedCallback callback);

 private:
  // Data provided when there is no focused tab.
  // The browser-side type corresponding to mojom::NoFocusedTabData.
  struct NoFocusedTabData {
    explicit NoFocusedTabData(std::string_view reason,
                              content::WebContents* tab = nullptr);
    NoFocusedTabData();
    ~NoFocusedTabData();
    NoFocusedTabData(const NoFocusedTabData& src);
    NoFocusedTabData& operator=(const NoFocusedTabData& src);
    bool IsSame(const NoFocusedTabData& new_data) const;

    // The active tab that could not be focused, may be null.
    base::WeakPtr<content::WebContents> active_tab;
    // Human readable debug message about why there is no focused tab.
    std::string_view no_focus_reason;
  };

  // Either a focused web contents, or a NoFocusedTabData.
  class FocusedTabDataImpl {
   public:
    explicit FocusedTabDataImpl(base::WeakPtr<content::WebContents> contents);
    explicit FocusedTabDataImpl(const NoFocusedTabData& no_focused_tab_data);
    FocusedTabDataImpl(const FocusedTabDataImpl&);
    ~FocusedTabDataImpl();

    bool is_focus() const {
      return std::holds_alternative<base::WeakPtr<content::WebContents>>(data_);
    }

    // Returns the focused tab web contents. Note that if FocusedTabData
    // represents a valid focus, this can still return nullptr if the web
    // contents has been deleted.
    content::WebContents* focus() const {
      const base::WeakPtr<content::WebContents>* focus = std::get_if<0>(&data_);
      return focus ? focus->get() : nullptr;
    }

    // Returns NoFocusedTabData. Will return nullptr if a valid focus.
    const NoFocusedTabData* no_focus() const { return std::get_if<1>(&data_); }

    // Whether this FocusedTabData is the same as `new_data`. Note that this
    // returns true if both FocusedTabData point to two different invalidated
    // web contents.
    bool IsSame(const FocusedTabDataImpl& new_data) const;

    // Returns the focused web contents, or a human-readable message indicating
    // why there is none.
    base::expected<content::WebContents*, std::string_view> GetFocus() const;

   private:
    std::variant<base::WeakPtr<content::WebContents>, NoFocusedTabData> data_;
  };

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

  void Initialize();

  static FocusedTabDataImpl GetFocusedTabData(
      const GlicFocusedTabManager::FocusedTabState& focused_state);

  // Updates focused tab if a new one is computed. Notifies if updated or if
  // `force_notify` is true.
  void MaybeUpdateFocusedTab(bool force_notify = false);

  // Computes the currently focused tab.
  struct FocusedTabState ComputeFocusedTabState();

  // Calls all registered focused tab changed callbacks.
  void NotifyFocusedTabChanged();

  // Calls all registered focused tab instance changed callbacks.
  void NotifyFocusedTabInstanceChanged(content::WebContents* web_contents);

  // Calls all registered focused tab or candidate instance changed callbacks.
  void NotifyFocusedTabOrCandidateInstanceChanged(
      const FocusedTabData& focused_tab_data);

  // Calls all registered focused tab data changed callbacks.
  void NotifyFocusedTabDataChanged(TabDataChange change);

  // Callback for changes to focused browser.
  void OnFocusedBrowserChanged(BrowserWindowInterface* candidate_browser,
                               BrowserWindowInterface* focused_browser);

  // Callback for active tab changes from BrowserWindowInterface.
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);

  // Callback for tab data changes to focused tab.
  void FocusedTabDataChanged(TabDataChange change);

  FocusedTabData ImplToPublic(FocusedTabDataImpl impl);

  // List of callbacks to be notified when focused tab changed.
  base::RepeatingCallbackList<void(const FocusedTabData&)>
      focused_callback_list_;

  // List of callbacks to be notified when focused tab instance changed.
  base::RepeatingCallbackList<void(content::WebContents*)>
      focused_instance_callback_list_;

  // List of callbacks to be notified when focused tab or candidate instances
  // changed.
  base::RepeatingCallbackList<void(const FocusedTabData&)>
      focused_or_candidate_instance_callback_list_;

  // List of callbacks to be notified when focused tab data changed.
  base::RepeatingCallbackList<void(const glic::mojom::TabData*)>
      focused_data_callback_list_;

  // Manages which browser window is considered "focused".
  raw_ptr<GlicFocusedBrowserManager> focused_browser_manager_;

  // The currently focused tab data.
  FocusedTabDataImpl focused_tab_data_{NoFocusedTabData()};

  // `TabDataObserver` for the currently focused tab (if one exists).
  std::unique_ptr<TabDataObserver> focused_tab_data_observer_;

  // Callback subscription for listening to changes to the focused browser.
  base::CallbackListSubscription focused_browser_subscription_;

  // The last known focused tab state.
  struct FocusedTabState focused_tab_state_;

  // Callback subscription for listening to changes to the Glic window
  // activation changes.
  base::CallbackListSubscription window_activation_subscription_;

  // The focused browser and subscriptions related to it.
  // These should be updated together.
  base::WeakPtr<BrowserWindowInterface> subscribed_browser_;
  base::CallbackListSubscription active_tab_subscription_;
};

// Applies the proxy pattern to focused tab manager to inject pinning as the
// source of truth for context-sharing.
//
// Behaves just like the default detached focused tab manager above, with one
// caveat: if the would-be focused tab is not currently pinned for sharing, then
// it is returned as the focus candidate instead of the focused tab.
//
// Useful for multi-instance, where we want the signal approximating "active
// tab", but without turning on actual context sharing (controlled by pinning).
class GlicPinAwareDetachedFocusedTabManager
    : public GlicFocusedTabManagerInterface {
 public:
  explicit GlicPinAwareDetachedFocusedTabManager(
      GlicSharingManager* sharing_manager,
      GlicFocusedBrowserManager* focused_browser_manager);
  ~GlicPinAwareDetachedFocusedTabManager() override;

  // GlicFocusedTabManagerInterface implementation.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override;
  FocusedTabData GetFocusedTabData() override;
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) override;
  bool IsTabFocused(tabs::TabHandle tab_handle) const override;

 private:
  // Returns the focused_tab_data unless there is a focused tab that isn't also
  // pinned -- in which case it moves the focused tab to the candidate.
  FocusedTabData GetPinAwareFocusedTabData(
      const FocusedTabData& focused_tab_data);

  // Callback for our real focused tab manager changes to be proxied.
  void OnFocusedTabChanged(const FocusedTabData& focused_tab_data);

  // Callback for our real focused tab manager data changes to be proxied.
  void OnFocusedTabDataChanged(const glic::mojom::TabData* focused_tab_data);

  // Callback for pinning status changes.
  void OnTabPinningStatusChanged(tabs::TabInterface* tab, bool status);

  // Register internal subscription callbacks.
  void InitializeSubscriptions();

  // Notifies subscribers of a change to the focused tab.
  void NotifyFocusedTabChanged(const FocusedTabData& focused_tab);

  // Notifies subscribers of a change to the focused tab data.
  void NotifyFocusedTabDataChanged(
      const glic::mojom::TabData* focused_tab_data);

  // Subscription for changes to focused tab.
  base::CallbackListSubscription focused_tab_changed_subscription_;

  // Subscription for changes to focused tab data.
  base::CallbackListSubscription focused_tab_data_changed_subscription_;

  // Subscription for changes to tab pinning status.
  base::CallbackListSubscription tab_pinning_status_changed_subscription_;

  // List of callbacks to fire when the focused tab changes.
  base::RepeatingCallbackList<void(const FocusedTabData&)>
      focused_tab_changed_callback_list_;

  // List of callbacks to fire when the focused tab data changes.
  base::RepeatingCallbackList<void(const glic::mojom::TabData*)>
      focused_tab_data_changed_callback_list_;

  // Source of truth for pinned tabs.
  // TODO(crbug.com/452150693): Split up the sharing manager interface so we can
  // specify just the pinning portion here.
  raw_ptr<GlicSharingManager> sharing_manager_;

  // Proxied focused tab manager.
  GlicFocusedTabManager focused_tab_manager_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_H_
