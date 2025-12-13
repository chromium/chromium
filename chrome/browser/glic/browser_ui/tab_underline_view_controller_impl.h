// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_CONTROLLER_IMPL_H_

#include <list>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view_controller.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/view_observer.h"

class Browser;

namespace content {
class WebContents;
}

namespace glic {

class TabUnderlineView;

class TabUnderlineViewControllerImpl
    : public TabUnderlineViewController,
      public GlicWindowController::StateObserver,
      public contextual_tasks::ActiveTaskContextProvider::Observer {
 public:
  TabUnderlineViewControllerImpl();
  TabUnderlineViewControllerImpl(const TabUnderlineViewControllerImpl&) =
      delete;
  TabUnderlineViewControllerImpl& operator=(
      const TabUnderlineViewControllerImpl&) = delete;
  ~TabUnderlineViewControllerImpl() override;

  // TabUnderlineViewController overrides:
  void Initialize(TabUnderlineView* underline_view, Browser* browser) override;

  // contextual_tasks::ActiveTaskContextProvider::Observer overrides:
  // Handles updates from the contextual Tasks backend.
  // Note: This flow is distinct from the GLIC flow.
  void OnContextTabsChanged(
      const std::set<tabs::TabHandle>& context_tabs) override;

 private:
  // Called when the focused tab changes with the focused tab data object.
  void OnFocusedTabChanged(const FocusedTabData& focused_tab_data);

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled);

  // Called when the glic set of pinned tabs changes.
  void OnPinnedTabsChanged(
      const std::vector<content::WebContents*>& pinned_contents);

  // GlicWindowController::StateObserver:
  void PanelStateChanged(
      const glic::mojom::PanelState& panel_state,
      const GlicWindowController::PanelStateContext& context) override;

  void OnUserInputSubmitted();

  // Types of updates to the tab underline UI effect given changes in relevant
  // triggering signals, including tab focus, glic sharing controls, pinned tabs
  // and the floaty panel.
  enum class UpdateUnderlineReason {
    kContextAccessIndicatorOn = 0,
    kContextAccessIndicatorOff,

    // Tab focus change not involving this underline.
    kFocusedTabChanged_NoFocusChange,
    // This underline's tab gained focus.
    kFocusedTabChanged_TabGainedFocus,
    // This underline's tab lost focus.
    kFocusedTabChanged_TabLostFocus,

    kFocusedTabChanged_ChromeGainedFocus,
    kFocusedTabChanged_ChromeLostFocus,

    // Chanes were made to the set of pinned of tabs.
    kPinnedTabsChanged_TabInPinnedSet,
    kPinnedTabsChanged_TabNotInPinnedSet,

    // Events related to the glic panel's state.
    kPanelStateChanged_PanelShowing,
    kPanelStateChanged_PanelHidden,

    // Changes were made to the set of tabs for contextual task. Note that this
    // is independent of the GLIC flow.
    kContextualTask_TabInContext,
    kContextualTask_TabNotInContext,

    kUserInputSubmitted,
  };

  GlicKeyedService* GetGlicKeyedService();

  // Returns the TabInterface corresponding to `underline_view_`, if it is
  // valid.
  base::WeakPtr<tabs::TabInterface> GetTabInterface();

  bool IsUnderlineTabPinned();

  bool IsUnderlineTabSharedThroughActiveFollow();

  // Trigger the necessary UI effect, primarily based on the given
  // `UpdateUnderlineReason` and whether or not `underline_view_`'s tab is
  // being shared via pinning or active following.
  void UpdateUnderlineView(UpdateUnderlineReason reason);

  // Off to On. Throw away everything we have and start the animation from
  // the beginning.
  void ShowAndAnimateUnderline();

  void HideUnderline();

  // Replay the animation without hiding and re-showing the view.
  void AnimateUnderline();

  void ShowOrAnimatePinnedUnderline();

  bool IsGlicWindowShowing() const;

  bool IsTabInCurrentWindow(const content::WebContents* tab) const;

  std::string UpdateReasonToString(UpdateUnderlineReason reason);

  void AddReasonForDebugging(UpdateUnderlineReason reason);

  // Helper methods to check feature flags for Glic and contextual tasks.
  bool ShouldUseSignalsForGlicUnderlines();
  bool ShouldUseSignalsForContextualTasks();

  std::string UpdateReasonsToString() const;

  // Back pointer to the owner. Guaranteed to outlive `this`.
  raw_ptr<TabUnderlineView> underline_view_;

  // The pointer to the browser in which the underline view lives. Outlives the
  // underline view.
  raw_ptr<Browser> browser_;

  // The Glic keyed service. This is only assigned if
  // ShouldUseSignalsForGlicUnderlines() returns true. Otherwise, it will stay
  // null.
  raw_ptr<GlicKeyedService> glic_service_;

  // Tracked states and their subscriptions.
  base::WeakPtr<content::WebContents> glic_current_focused_contents_;
  base::CallbackListSubscription focus_change_subscription_;
  bool context_access_indicator_enabled_ = false;
  base::CallbackListSubscription indicator_change_subscription_;
  base::CallbackListSubscription pinned_tabs_change_subscription_;
  base::CallbackListSubscription user_input_submitted_subscription_;

  // Subscription for contextual tasks backend.
  base::ScopedObservation<contextual_tasks::ActiveTaskContextProvider,
                          contextual_tasks::ActiveTaskContextProvider::Observer>
      contextual_task_observation_{this};

  static constexpr size_t kNumReasonsToKeep = 10u;
  std::list<std::string> underline_update_reasons_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_CONTROLLER_IMPL_H_
