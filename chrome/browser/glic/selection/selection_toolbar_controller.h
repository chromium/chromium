// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_TOOLBAR_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace actions {
class ActionItem;
}  // namespace actions

namespace glic {

class GlicKeyedService;

// Keeps the "Ask about this" toolbar button visible while Glic is allowed,
// enabled while a click can open (or close) the overlay, and highlighted while
// the active tab's overlay is showing.
class SelectionToolbarController {
 public:
  DECLARE_USER_DATA(SelectionToolbarController);

  static SelectionToolbarController* From(BrowserWindowInterface* browser);

  // Whether `browser` registered the toolbar action this class drives.
  static bool IsToolbarActionRegistered(BrowserWindowInterface& browser);

  SelectionToolbarController(BrowserWindowInterface& browser,
                             GlicKeyedService& glic_service);
  SelectionToolbarController(const SelectionToolbarController&) = delete;
  SelectionToolbarController& operator=(const SelectionToolbarController&) =
      delete;
  ~SelectionToolbarController();

  // Recomputes the button's visibility, enabled state, and highlight.
  void UpdateState();

  // Updates the button when the active tab's overlay shows or hides, since
  // `OverlayBaseController` notifies this controller before updating `state_`.
  void OnOverlayShowingChanged(bool is_showing);

 private:
  void OnActiveTabChanged(BrowserWindowInterface* browser);
  void UpdateStateImpl(bool is_overlay_showing);

  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ref<actions::ActionItem> action_item_;
  std::vector<base::CallbackListSubscription> subscriptions_;
  base::CallbackListSubscription eligibility_subscription_;
  ui::ScopedUnownedUserData<SelectionToolbarController>
      scoped_unowned_user_data_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_TOOLBAR_CONTROLLER_H_
