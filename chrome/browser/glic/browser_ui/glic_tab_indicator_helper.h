// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_INDICATOR_HELPER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_INDICATOR_HELPER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

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
// without handling moving.
class GlicTabIndicatorHelper {
 public:
  explicit GlicTabIndicatorHelper(tabs::TabInterface* tab);
  ~GlicTabIndicatorHelper();

 private:
  // Updates the Tab UI. This only needs to be called when the tab gains/loses
  // focus, or when the indicator status changes.
  void UpdateTab();

  // Called when the focused tab changes with the focused tab data object.
  void OnFocusedTabChanged(FocusedTabData focused_tab_data);

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled);

  // Called when the tab is detached.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  // Called when the tab is inserted.
  void OnTabDidInsert(tabs::TabInterface* tab);

  raw_ptr<tabs::TabInterface> tab_;
  bool tab_is_focused_ = false;
  bool is_detached_ = false;
  base::CallbackListSubscription focus_change_subscription_;
  base::CallbackListSubscription indicator_change_subscription_;
  base::CallbackListSubscription will_detach_subscription_;
  base::CallbackListSubscription did_insert_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_INDICATOR_HELPER_H_
