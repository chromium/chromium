// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/rect.h"

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

class IndigoOnboardingDialog;
struct OnboardingResult;
class IndigoService;

// Manages the Indigo page action and its various entry points, ensuring they
// are correctly displayed.
class IndigoPageActionController : public tabs::ContentsObservingTabFeature,
                                   public IndigoToolbar::Delegate {
 public:
  DECLARE_USER_DATA(IndigoPageActionController);

  using OnboardingDialogFactory =
      base::RepeatingCallback<std::unique_ptr<IndigoOnboardingDialog>(
          tabs::TabInterface&,
          const GURL&,
          base::OnceCallback<void(const OnboardingResult&)>)>;

  explicit IndigoPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);
  ~IndigoPageActionController() override;

  // Retrieves an IndigoPageActionController from the given tab, or nullptr if
  // it does not exist.
  static IndigoPageActionController* From(tabs::TabInterface* tab);

  void InvokeAction();

  // Shows the toolbar at the specified rectangle in the web contents view.
  void ShowToolbarInside(const gfx::Rect& rect);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // IndigoToolbar::Delegate:
  void OnClose(IndigoToolbar* toolbar) override;
  void OnRegenerate(IndigoToolbar* toolbar) override;
  void OnReplaceOriginalPhoto(IndigoToolbar* toolbar) override;
  void OnDeleteOriginalPhoto(IndigoToolbar* toolbar) override;

  class TestApi {
   public:
    explicit TestApi(IndigoPageActionController* controller)
        : controller_(controller) {}

    void CheckEligibilityForOnboarding(const CombinedEligibility& eligibility) {
      controller_->CheckEligibilityForOnboarding(eligibility);
    }

    void CheckOnboardingResult(const OnboardingResult& result) {
      controller_->OnOnboardingDialogClosed(result);
    }

    void SetOnboardingDialogFactory(OnboardingDialogFactory factory) {
      controller_->onboarding_dialog_factory_for_testing_ = std::move(factory);
    }

   private:
    raw_ptr<IndigoPageActionController> controller_;
  };

 private:
  // Updates the visibility and states of all entry points.
  void UpdateEntryPointsState();

  // Called when the eligibility has been fetched.
  void CheckEligibilityForOnboarding(const CombinedEligibility& eligibility);

  // Called when eligibility is known and onboarding is completed (if needed).
  void ContinueInvoke(const CombinedEligibility& eligibility);

  // Updates state and handles preference changes when the dialog closes.
  void OnOnboardingDialogClosed(const OnboardingResult& result);

  // Called when the profile state has changed in a way that might affect
  // whether this feature should be offered.
  void OnLocalEligibilityChanged(LocalEligibility state);

  // Called when optimization guide has decided whether this feature should be
  // enabled for the page.
  void OnOptimizationGuideDecision(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata&);

  void TabWillBecomeHidden(tabs::TabInterface* tab);
  void TabDidBecomeVisible(tabs::TabInterface* tab);

  // Retrieves the indigo overlay view for this tab. May return nullptr if
  // the tab is currently invisible (backgrounded) or has no active browser
  // window.
  views::View* GetIndigoOverlayView() const;

  // `page_action_controller_` is owned by the same `TabFeatures` that owns
  // `this`. Since `page_action_controller_` is initialized before `this` and
  // destroyed after, it is safe to hold as a `raw_ref`.
  const raw_ref<page_actions::PageActionController> page_action_controller_;

  // Owned by the profile, which outlives this object.
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_;

  // Owned by the profile, which outlives this object.
  const raw_ptr<IndigoService> indigo_service_;

  // The optimization guide's opinion about whether this page is eligible for
  // Indigo (or kUnknown if not determined).
  optimization_guide::OptimizationGuideDecision optimization_guide_decision_ =
      optimization_guide::OptimizationGuideDecision::kUnknown;

  // If true, the Indigo page action is currently shown.
  bool is_shown_ = false;

  // The onboarding dialog, if shown.
  std::unique_ptr<IndigoOnboardingDialog> onboarding_dialog_;

  // Factory for creating the onboarding dialog in tests.
  OnboardingDialogFactory onboarding_dialog_factory_for_testing_;

  // The floating toolbar, if shown.
  std::unique_ptr<IndigoToolbar> toolbar_;

  base::CallbackListSubscription tab_became_hidden_subscription_;
  base::CallbackListSubscription tab_became_visible_subscription_;

  base::CallbackListSubscription indigo_service_subscription_;

  ui::ScopedUnownedUserData<IndigoPageActionController>
      scoped_unowned_user_data_;

  // Weak pointer factory used for the invocation flow. This is invalidated on
  // navigation to ensure that if a user starts an action (like onboarding) and
  // then navigates away, the action does not continue on the new page.
  base::WeakPtrFactory<IndigoPageActionController> invoke_weak_ptr_factory_{
      this};

  // Weak pointer factory for general callbacks that should persist across
  // navigations (as long as this controller exists).
  base::WeakPtrFactory<IndigoPageActionController> weak_ptr_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_PAGE_ACTION_CONTROLLER_H_
