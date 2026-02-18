// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace indigo {

// Manages the Indigo page action and its various entry points, ensuring they
// are correctly displayed.
class IndigoPageActionController : public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(IndigoPageActionController);

  explicit IndigoPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);
  ~IndigoPageActionController() override;

  // Retrieves an IndigoPageActionController from the given tab, or nullptr if
  // it does not exist.
  static IndigoPageActionController* From(tabs::TabInterface* tab);

  void InvokeAction();

 private:
  // Updates the visibility and states of all entry points.
  void UpdateEntryPointsState();

  // `page_action_controller_` is owned by the same `TabFeatures` that owns
  // `this`. Since `page_action_controller_` is initialized before `this` and
  // destroyed after, it is safe to hold as a `raw_ref`.
  const raw_ref<page_actions::PageActionController> page_action_controller_;

  ui::ScopedUnownedUserData<IndigoPageActionController>
      scoped_unowned_user_data_;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_
