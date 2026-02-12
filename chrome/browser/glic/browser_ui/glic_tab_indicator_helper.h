// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_INDICATOR_HELPER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_INDICATOR_HELPER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace glic {

class GlicKeyedService;

// This class is partially responsible for updating tab indicators for glic
// focus. TODO(crbug.com/393557651): Simplify TabRendererData design, at which
// point this class can be fully responsible.
//
// Tab UI reflects feature state (such as the glic focused tab), but is stored
// on a per-index basis rather than a per-tab basis. This means that the UI
// needs to be updated either when the feature state changes, OR when the tab is
// moved around (either in the same tabstrip, or into a new tabstrip). The
// latter is currently handled by
// BrowserTabStripController::OnTabStripModelChanged. This class is only
// responsible for propagating glic focus changes for a fixed TabInterface,
// without handling moving and for changes to the sharing status of a tab.
class GlicTabIndicatorHelper {
 public:
  DECLARE_USER_DATA(GlicTabIndicatorHelper);

  static GlicTabIndicatorHelper* From(tabs::TabInterface* tab_interface);

  explicit GlicTabIndicatorHelper(tabs::TabInterface* tab);
  ~GlicTabIndicatorHelper();

  using GlicAlertStateChangeCallbackList =
      base::RepeatingCallbackList<void(bool)>;
  base::CallbackListSubscription RegisterGlicAccessingStateChange(
      GlicAlertStateChangeCallbackList::CallbackType accessing_change_callback);
  base::CallbackListSubscription RegisterGlicSharingStateChange(
      GlicAlertStateChangeCallbackList::CallbackType sharing_change_callback);

 private:
  // Updates the Tab UI. This only needs to be called when the tab gains/loses
  // focus, or when the indicator status changes.
  void UpdateTab();

  // Called when the focused tab changes with the focused tab data object.
  void OnFocusedTabChanged(const FocusedTabData& focused_tab_data);

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled);

  // Called when the tab's pinning status is updated.
  void OnTabPinningStatusChanged(tabs::TabInterface*, bool pinned);

  // Called when the tab is detached.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  // Called when the tab is inserted.
  void OnTabDidInsert(tabs::TabInterface* tab);

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  raw_ptr<GlicKeyedService> glic_keyed_service_ = nullptr;
  bool tab_is_focused_ = false;
  bool is_detached_ = false;
  bool is_glic_sharing_ = false;
  bool is_glic_accessing_ = false;
  std::vector<base::CallbackListSubscription> subscriptions_;

  GlicAlertStateChangeCallbackList glic_accessing_change_callbacks_;
  GlicAlertStateChangeCallbackList glic_sharing_change_callbacks_;
  ui::ScopedUnownedUserData<GlicTabIndicatorHelper> scoped_unowned_user_data_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_INDICATOR_HELPER_H_
