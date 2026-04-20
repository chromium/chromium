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
#include "chrome/browser/indigo/indigo_alpha_rpc.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
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

  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }

  // For now, onboarding is only triggered when forced, and the URL is specified
  // in the command line switch. In the future, this will typically be triggered
  // automatically based on the user's enrolment status, and the URL will be
  // determined by a feature param.
  std::string onboarding_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kForceIndigoOnboardingSwitch);
  if (!onboarding_url.empty()) {
    onboarding_dialog_ = IndigoOnboardingDialog::Show(
        tab(), GURL(onboarding_url),
        base::BindOnce(&IndigoPageActionController::OnOnboardingDialogClosed,
                       weak_ptr_factory_.GetWeakPtr()));
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
    toolbar_->Show(web_contents->GetNativeView());
    return;
  }

  // TODO: b/482792874 - Analyze the page and act on it, instead of just opening
  // a tab based on a fixed input.
  LOG(WARNING) << "IndigoAgentHost doesn't expect to be able to load. "
               << "Directly invoking generate RPC (for prototyping).";
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  ExecuteAlphaGenerateRpc(
      loader_factory.get(),
      base::BindOnce(
          [](base::WeakPtr<BrowserWindowInterface> window,
             base::expected<GURL, AlphaGenerateError> result) {
            if (window && result.has_value()) {
              window->OpenGURL(result.value(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB);
            } else if (!result.has_value()) {
              LOG(ERROR) << "Indigo alpha generate error "
                         << result.error().error_type << ": "
                         << result.error().error_message;
            }
          },
          tab().GetBrowserWindowInterface()->GetWeakPtr()));
}

void IndigoPageActionController::ShowToolbarInside(const gfx::Rect& rect) {
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }

  if (!toolbar_) {
    toolbar_ = std::make_unique<IndigoToolbar>(this);
    toolbar_->ShowInside(web_contents->GetNativeView(), rect);
  }
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
  NOTIMPLEMENTED();
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

}  // namespace indigo
