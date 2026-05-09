// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_page_action_controller.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "chrome/browser/indigo/indigo_agent_host.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/view.h"

namespace indigo {

namespace {
const char kForceIndigoSwitch[] = "force-indigo";
const char kForceIndigoOnboardingSwitch[] = "force-indigo-onboarding";
const char kForceIndigoToolbarSwitch[] = "force-indigo-toolbar";
}  // namespace

DEFINE_USER_DATA(IndigoPageActionController);

IndigoPageActionController::IndigoPageActionController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : tabs::ContentsObservingTabFeature(tab_interface),
      page_action_controller_(page_action_controller),
      optimization_guide_(OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(
              tab_interface.GetContents()->GetBrowserContext()))),
      indigo_service_(
          IndigoServiceFactory::GetForProfile(Profile::FromBrowserContext(
              tab_interface.GetContents()->GetBrowserContext()))),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  if (optimization_guide_) {
    optimization_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::OptimizationType::INDIGO});
  }

  if (indigo_service_) {
    indigo_service_subscription_ =
        indigo_service_->RegisterLocalEligibilityChangedCallback(
            base::BindRepeating(
                &IndigoPageActionController::OnLocalEligibilityChanged,
                base::Unretained(this)));
  }

  // TODO(b/511166876): Split view visual swaps (reversing panels) and some
  // other related changes do not fire tab visibility changes, even though it
  // may have changed which ContentsContainerView shows a tab contents.
  // We'll need to either observe these directly or create a higher level
  // event that describes when a tab may have changed how it is rendered in the
  // BrowserView.
  tab_became_hidden_subscription_ = tab_interface.RegisterWillBecomeHidden(
      base::BindRepeating(&IndigoPageActionController::TabWillBecomeHidden,
                          base::Unretained(this)));
  tab_became_visible_subscription_ = tab_interface.RegisterDidBecomeVisible(
      base::BindRepeating(&IndigoPageActionController::TabDidBecomeVisible,
                          base::Unretained(this)));

  UpdateEntryPointsState();
}

IndigoPageActionController::~IndigoPageActionController() {
  // If there is a toolbar, hide it before anything else. This makes sure that
  // the OnClose delegate function isn't called after some members have been
  // destroyed.
  if (toolbar_) {
    toolbar_->Hide();
    toolbar_.reset();
  }
}

// static
IndigoPageActionController* IndigoPageActionController::From(
    tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

void IndigoPageActionController::InvokeAction() {
  base::RecordAction(base::UserMetricsAction("Indigo.PageAction.Click"));

  if (!indigo_service_) {
    return;
  }

  indigo_service_->GetCombinedEligibility(
      base::BindOnce(&IndigoPageActionController::CheckEligibilityForOnboarding,
                     invoke_weak_ptr_factory_.GetWeakPtr()));
}

void IndigoPageActionController::CheckEligibilityForOnboarding(
    const CombinedEligibility& eligibility) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool force_onboarding =
      command_line->HasSwitch(kForceIndigoOnboardingSwitch);

  // Show onboarding if the user is ready to onboard, or if it's forced.
  if (eligibility.ReadyToOnboard() || force_onboarding) {
    std::string onboarding_url =
        command_line->GetSwitchValueASCII(kForceIndigoOnboardingSwitch);
    if (onboarding_url.empty()) {
      onboarding_url = features::kIndigoOnboardingUrl.Get();
    }
    if (onboarding_dialog_factory_for_testing_) {
      onboarding_dialog_ = onboarding_dialog_factory_for_testing_.Run(
          tab(), GURL(onboarding_url),
          base::BindOnce(&IndigoPageActionController::OnOnboardingDialogClosed,
                         invoke_weak_ptr_factory_.GetWeakPtr()));
    } else {
      onboarding_dialog_ = IndigoOnboardingDialog::Show(
          tab(), GURL(onboarding_url),
          base::BindOnce(&IndigoPageActionController::OnOnboardingDialogClosed,
                         invoke_weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  ContinueInvoke(eligibility);
}

void IndigoPageActionController::ContinueInvoke(
    const CombinedEligibility& eligibility) {
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }

  if (!eligibility.CanGenerateImage()) {
    // TODO(b/505743640): Show a toast or something if we can't generate an
    // image and aren't ready to onboard.
    LOG(WARNING)
        << "Indigo not eligible for generation and not ready to onboard";
    return;
  }

  if (IndigoAgentHost::GetOrCreateForPage(web_contents->GetPrimaryPage())
          ->Invoke()) {
    return;
  }

  // The toolbar isn't quite ready yet (nor is it integrated with anything else)
  // but it's useful to force it to show for manual testing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kForceIndigoToolbarSwitch)) {
    if (!toolbar_) {
      toolbar_ = std::make_unique<IndigoToolbar>(this);
    }
    views::View* parent_view = GetIndigoOverlayView();
    toolbar_->Show(parent_view);
    return;
  }
}

void IndigoPageActionController::ShowToolbarInside(const gfx::Rect& rect) {
  if (!toolbar_) {
    toolbar_ = std::make_unique<IndigoToolbar>(this);
  }

  views::View* parent_view = GetIndigoOverlayView();

  // TODO(b/511166876): We assume that contents_webview and
  // indigo_overlay_view share the same origin and coordinate space for now.
  // In the future, if their layouts differ (e.g., in RTL or if devtools
  // placement changes the sibling origins), we should perform an appropriate
  // coordinate conversion using views::View::ConvertRectToTarget.
  toolbar_->ShowInside(parent_view, rect);
}

void IndigoPageActionController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  ContentsObservingTabFeature::DidFinishNavigation(navigation_handle);

  // Only care about navigations where the URL seems to have changed, excluding
  // the URL fragment. Notably we _do_ care about navigation within a
  // single-page application.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->GetPreviousPrimaryMainFrameURL().EqualsIgnoringRef(
          navigation_handle->GetURL())) {
    return;
  }

  if (toolbar_) {
    toolbar_->Hide();
    toolbar_.reset();
  }

  // TODO: b/508219600 Consider closing the onboarding dialog if navigates away.

  invoke_weak_ptr_factory_.InvalidateWeakPtrs();

  optimization_guide_decision_ =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  UpdateEntryPointsState();

  if (optimization_guide_) {
    const GURL& url = navigation_handle->GetURL();
    optimization_guide_->CanApplyOptimization(
        url, optimization_guide::proto::OptimizationType::INDIGO,
        base::BindOnce(&IndigoPageActionController::OnOptimizationGuideDecision,
                       weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void IndigoPageActionController::OnClose(IndigoToolbar* toolbar) {
  if (toolbar_) {
    toolbar_->Hide();
    toolbar_.reset();
  }
  content::WebContents* web_contents = tab().GetContents();
  if (web_contents) {
    auto* manager = IndigoImageReplacementManager::GetForPage(
        web_contents->GetPrimaryPage());
    if (manager) {
      manager->ResetAllReplacements();
    }
  }
}

void IndigoPageActionController::TabWillBecomeHidden(tabs::TabInterface* tab) {
  DCHECK_EQ(tab, &this->tab());
  if (toolbar_) {
    toolbar_->TabWillBecomeHidden();
  }
}

void IndigoPageActionController::TabDidBecomeVisible(tabs::TabInterface* tab) {
  DCHECK_EQ(tab, &this->tab());
  if (!toolbar_) {
    return;
  }

  auto* parent_view = GetIndigoOverlayView();
  if (!parent_view) {
    return;
  }

  toolbar_->TabDidBecomeVisible(parent_view);
}

void IndigoPageActionController::OnRegenerate(IndigoToolbar* toolbar) {
  NOTIMPLEMENTED();
}

void IndigoPageActionController::OnReplaceOriginalPhoto(
    IndigoToolbar* toolbar) {
  NOTIMPLEMENTED();
}

void IndigoPageActionController::OnDeleteOriginalPhoto(IndigoToolbar* toolbar) {
  NOTIMPLEMENTED();
}

void IndigoPageActionController::UpdateEntryPointsState() {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  if (!indigo_service_) {
    return;
  }

  const bool forced =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kForceIndigoSwitch);
  const bool eligible =
      optimization_guide_decision_ ==
          optimization_guide::OptimizationGuideDecision::kTrue &&
      indigo_service_->IsLocallyEligible();

  const bool should_show = forced || eligible;
  if (should_show == is_shown_) {
    return;
  }

  if (should_show) {
    page_action_controller_->Show(kActionIndigo);
    if (indigo_service_->CanShowAnchoredMessage()) {
      page_action_controller_->SetAnchoredMessageText(
          kActionIndigo, l10n_util::GetStringUTF16(
                             IDS_INDIGO_ENTRYPOINT_ANCHORED_MESSAGE_TEXT));
      page_action_controller_->ShowAnchoredMessage(
          kActionIndigo,
          {.priority =
               page_actions::PageActionPriorityCategory::kContextualCue});
      // TODO(b/483103108): ShowAnchoredMessage is not guaranteed to show the
      // anchored message. Migrate the following logic to use
      // PageActionObserver.
      indigo_service_->AnchoredMessageShown();
      base::RecordAction(
          base::UserMetricsAction("Indigo.PageAction.ShowAnchoredMessage"));
    } else {
      page_action_controller_->ShowSuggestionChip(kActionIndigo);
    }
    base::RecordAction(base::UserMetricsAction("Indigo.PageAction.Show"));
  } else {
    page_action_controller_->Hide(kActionIndigo);
  }
  is_shown_ = should_show;
}

void IndigoPageActionController::OnOnboardingDialogClosed(
    const OnboardingResult& result) {
  if (result.acknowledge_chrome_disclaimer) {
    content::WebContents* web_contents = tab().GetContents();
    if (!web_contents) {
      return;
    }

    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    profile->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, true);

    if (indigo_service_) {
      indigo_service_->InvalidateRemoteEligibility();
      indigo_service_->GetCombinedEligibility(
          base::BindOnce(&IndigoPageActionController::ContinueInvoke,
                         invoke_weak_ptr_factory_.GetWeakPtr()));
    }
  }
  // Onboarding dialog must be reset after reading its result.
  onboarding_dialog_.reset();
}

void IndigoPageActionController::OnLocalEligibilityChanged(
    LocalEligibility state) {
  UpdateEntryPointsState();
}

void IndigoPageActionController::OnOptimizationGuideDecision(
    const GURL& url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // If the answer comes after another navigation, ignore it.
  if (!url.EqualsIgnoringRef(tab().GetContents()->GetLastCommittedURL())) {
    return;
  }
  optimization_guide_decision_ = decision;
  UpdateEntryPointsState();
}

views::View* IndigoPageActionController::GetIndigoOverlayView() const {
  if (!tab().IsVisible()) {
    return nullptr;
  }

  auto* browser_view =
      BrowserView::GetBrowserViewForBrowser(tab().GetBrowserWindowInterface());
  CHECK(browser_view);

  auto* contents_container =
      browser_view->GetContentsContainerViewFor(tab().GetContents());
  CHECK(contents_container);

  return contents_container->indigo_overlay_view();
}

}  // namespace indigo
