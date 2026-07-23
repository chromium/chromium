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
#include "base/timer/timer.h"
#include "chrome/browser/indigo/api_client.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "content/public/browser/tracked_element_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/document_metadata/document_metadata.mojom.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/rect.h"

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

#include "chrome/browser/ui/page_action/page_action_controller.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace indigo {

class IndigoOnboardingDialog;
struct OnboardingResult;
class IndigoService;

// LINT.IfChange(IndigoTransformationResult)

// Results of Indigo action invocation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IndigoTransformationResult {
  kUnknown = 0,
  kSuccess = 1,
  kNotSignedIn = 2,
  kMissingCapabilities = 3,
  kDisabledByPolicy = 4,
  kMissingScript = 5,
  kRemoteStatusMissing = 6,
  kServiceNotSupported = 7,
  kMissingUserImage = 8,
  kNotOnboarded = 9,
  kGenerateImageError = 10,
  kRefreshTokenInPersistentErrorState = 11,
  kManagedDomain = 12,
  kGlicDisabledForProfile = 13,
  kEnterpriseDisallowed = 14,
  kPrimaryImageDisconnected = 15,
  kEmptyPrimaryImageSize = 16,
  kPrimaryImageTooSmall = 17,
  kNoPrimaryImageFound = 18,
  kPrimaryImageReplacementCreationFailed = 19,
  kMaxValue = kPrimaryImageReplacementCreationFailed,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/indigo/enums.xml:IndigoTransformationResult)

// Trigger sources for the Indigo page action.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IndigoTriggerSource)
enum class IndigoTriggerSource {
  kUnknown = 0,
  kForced = 1,
  kOptimizationGuide = 2,
  kLocalProductKeywordHeuristic = 3,
  kMaxValue = kLocalProductKeywordHeuristic,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/indigo/enums.xml:IndigoTriggerSource)

enum class ResetType {
  kResetReplacementsAndContentScript,
  kResetReplacementsOnly,
};

enum class OnboardingDisposition {
  // Triggered in the normal course of using the feature.
  kDefault,
  // Triggered to replace the existing image.
  kReplacePhoto,
};

enum class EntryPoint {
  kSuggestionChip = 0,
  kAnchoredMessage = 1,
  kErrorToast = 2,
};

// Manages the Indigo page action and its various entry points, ensuring they
// are correctly displayed.
class IndigoPageActionController : public tabs::ContentsObservingTabFeature,
                                   public content::TrackedElementObserver,
                                   public IndigoToolbar::Delegate,
                                   public page_actions::PageActionObserver {
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

  void InvokeAction(EntryPoint entry_point);

  // Resets all image replacements and hides the toolbar.
  void Reset(ResetType reset_type);

  // Dismisses the page action anchored message.
  void DismissAnchoredMessage();

  // Shows the toolbar using the latest tracked bounds, if available.
  void ShowToolbar();

  void SetTrackedBoundsForTesting(const std::optional<gfx::Rect>& bounds) {
    tracked_bounds_ = bounds;
  }

  // Shows a toast notification informing the user that an error has occurred.
  // Note: `result` is only used for metrics purposes and doesn't affect the
  // error toast displayed.
  void ShowInvocationErrorToast(IndigoTransformationResult result);

  base::WeakPtr<IndigoPageActionController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Opens the settings page for Indigo.
  virtual void OpenSettings();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;

  // tabs::ContentsObservingTabFeature:
  void OnDiscardContents(tabs::TabInterface* tab,
                         content::WebContents* old_contents,
                         content::WebContents* new_contents) override;

  // content::TrackedElementObserver:
  void OnTrackedElementRectsChanged(const viz::TrackedElementRects& rects,
                                    float device_scale_factor) override;

  // IndigoToolbar::Delegate:
  void OnClose(IndigoToolbar* toolbar) override;
  void OnRegenerate(IndigoToolbar* toolbar) override;
  void OnReplaceOriginalPhoto(IndigoToolbar* toolbar) override;
  void OnDeleteOriginalPhoto(IndigoToolbar* toolbar) override;

  // page_actions::PageActionObserver:
  void OnPageActionAnchoredMessageShown(
      const page_actions::PageActionState& page_action) override;

  class TestApi {
   public:
    explicit TestApi(IndigoPageActionController* controller)
        : controller_(controller) {}

    void CheckEligibilityForOnboarding(bool skip_glic_invoke,
                                       const CombinedEligibility& eligibility) {
      controller_->CheckEligibilityForOnboarding(skip_glic_invoke, eligibility);
    }

    void CheckOnboardingResult(OnboardingDisposition disposition,
                               bool skip_glic_invoke,
                               const OnboardingResult& result) {
      controller_->OnOnboardingDialogClosed(disposition, skip_glic_invoke,
                                            result);
    }

    void SetOnboardingDialogFactory(OnboardingDialogFactory factory) {
      controller_->onboarding_dialog_factory_for_testing_ = std::move(factory);
    }

   private:
    raw_ptr<IndigoPageActionController> controller_;
  };

 private:
  // Resets the page triggering and classification state.
  void ResetTriggeringState();

  // Updates the visibility and states of all entry points.
  void UpdateEntryPointsState();

  // Determines the source that triggered the Indigo page action, if it should
  // be shown. Returns std::nullopt if the page action should not be shown.
  std::optional<IndigoTriggerSource> DetermineTriggerSource() const;

  // Shows the onboarding dialog with the appropriate URL based on disposition.
  void ShowOnboardingDialog(OnboardingDisposition disposition,
                            bool skip_glic_invoke);

  // Called when the eligibility has been fetched.
  void CheckEligibilityForOnboarding(bool skip_glic_invoke,
                                     const CombinedEligibility& eligibility);

  // Called when eligibility is known and onboarding is completed (if needed).
  void ContinueInvoke(bool skip_glic_invoke,
                      const CombinedEligibility& eligibility);

  // Helper to invoke IndigoAgent.
  void TriggerIndigoAgent();
  // Same as above, but introduces a delay before invoking.
  void TriggerIndigoAgentWithDelay();

  // Updates state and handles preference changes when the dialog closes.
  void OnOnboardingDialogClosed(OnboardingDisposition disposition,
                                bool skip_glic_invoke,
                                const OnboardingResult& result);

  // Called when the delete request completes.
  void OnDeleteOriginalPhotoComplete(base::expected<void, DeleteError> result);

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

  // Hides the toolbar if it is currently shown.
  void DestroyToolbar();

  // Shows the page action anchored message and notifies IndigoService.
  void ShowAnchoredMessage(page_actions::PageActionPriorityCategory priority);

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

  // The latest tracked bounds of the primary image, in DIPs.
  std::optional<gfx::Rect> tracked_bounds_;

  // Timer to delay the invocation of IndigoAgent after Glic is invoked and the
  // side panel is opened, to allow for the page to stabilize.
  base::OneShotTimer delay_agent_invoke_timer_;

  base::CallbackListSubscription tab_became_hidden_subscription_;
  base::CallbackListSubscription tab_became_visible_subscription_;

  base::CallbackListSubscription indigo_service_subscription_;

  ui::ScopedUnownedUserData<IndigoPageActionController>
      scoped_unowned_user_data_;

  void RegisterObserverWithHost(content::RenderWidgetHost* host);
  void UnregisterObserverFromHost(content::RenderWidgetHost* host);
  void ClearTrackedBoundsAndHideToolbar();
  void TriggerMetadataClassification();
  void OnProductClassified(blink::mojom::ProductClassificationResultPtr result);

  raw_ptr<content::RenderWidgetHost> current_host_ = nullptr;

  // True if the metadata heuristic determined this page has an allowed
  // category.
  bool page_has_allowed_category_by_heuristic_ = false;

  // Remote to the Blink-side metadata extraction service.
  mojo::Remote<blink::mojom::DocumentMetadata> metadata_remote_;

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
