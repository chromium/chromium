// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SINGLE_BROWSER_FOCUSED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SINGLE_BROWSER_FOCUSED_TAB_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager_interface.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/ui/browser_window.h"

class BrowserWindowInterface;

namespace glic {

// Focused tab manager implementation tied to a single browser window.
class GlicSingleBrowserFocusedTabManager
    : public GlicFocusedTabManagerInterface {
 public:
  explicit GlicSingleBrowserFocusedTabManager(
      BrowserWindowInterface* browser_interface);
  ~GlicSingleBrowserFocusedTabManager() override;

  GlicSingleBrowserFocusedTabManager(
      const GlicSingleBrowserFocusedTabManager&) = delete;
  GlicSingleBrowserFocusedTabManager& operator=(
      const GlicSingleBrowserFocusedTabManager&) = delete;

  // GlicFocusedTabManagerInterface Implementation.
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
  // Const implementation to be used in `IsTabFocused`
  FocusedTabData GetFocusedTabData() const;

  // Callback for active tab changes from BrowserWindowInterface.
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);

  // Callback for tab data changes to focused tab.
  void FocusedTabDataChanged(TabDataChange change);

  // Calls all registered focused tab changed callbacks.
  void NotifyFocusedTabChanged(const FocusedTabData& focused_tab_data);

  // Calls all registered focused tab data changed callbacks.
  void NotifyFocusedTabDataChanged(TabDataChange change);

  // List of callbacks to be notified when focused tab changed.
  base::RepeatingCallbackList<void(const FocusedTabData&)>
      focused_callback_list_;

  // List of callbacks to be notified when focused tab data changed.
  base::RepeatingCallbackList<void(const glic::mojom::TabData*)>
      focused_data_callback_list_;

  // Active tab changed subscription.
  base::CallbackListSubscription active_tab_subscription_;

  // `TabDataObserver` for the currently focused tab (if one exists).
  std::unique_ptr<TabDataObserver> focused_tab_data_observer_;

  raw_ptr<BrowserWindowInterface> browser_interface_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SINGLE_BROWSER_FOCUSED_TAB_MANAGER_H_
