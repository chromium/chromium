// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace indigo {

// Manages the Indigo page action and its various entry points, ensuring they
// are correctly displayed.
class IndigoPageActionController : public tabs::ContentsObservingTabFeature,
                                   public signin::IdentityManager::Observer {
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

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

 private:
  // Updates the visibility and states of all entry points.
  void UpdateEntryPointsState();

  // Called when optimization guide has decided whether this feature should be
  // enabled for the page.
  void OnOptimizationGuideDecision(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata&);

  // Returns whether the profile is known to belong to a primary account which
  // has the capabilities required to use this feature.
  bool CanUseModelExecutionFeatures() const;

  // `page_action_controller_` is owned by the same `TabFeatures` that owns
  // `this`. Since `page_action_controller_` is initialized before `this` and
  // destroyed after, it is safe to hold as a `raw_ref`.
  const raw_ref<page_actions::PageActionController> page_action_controller_;

  // Owned by the profile, which outlives this object.
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_;

  // Owned by the profile, which outlives this object.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // The optimization guide's opinion about whether this page is eligible for
  // Indigo (or kUnknown if not determined).
  optimization_guide::OptimizationGuideDecision optimization_guide_decision_ =
      optimization_guide::OptimizationGuideDecision::kUnknown;

  // If true, the Indigo page action is currently shown.
  bool is_shown_ = false;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  ui::ScopedUnownedUserData<IndigoPageActionController>
      scoped_unowned_user_data_;
  base::WeakPtrFactory<IndigoPageActionController> weak_ptr_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_
