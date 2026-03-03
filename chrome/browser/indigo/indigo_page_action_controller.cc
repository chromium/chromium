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
#include "chrome/browser/indigo/indigo_alpha_rpc.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"

namespace indigo {

namespace {
const char kForceIndigoSwitch[] = "force-indigo";
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
      identity_manager_(
          IdentityManagerFactory::GetForProfile(Profile::FromBrowserContext(
              tab_interface.GetContents()->GetBrowserContext()))),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  if (optimization_guide_) {
    optimization_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::OptimizationType::INDIGO});
  }

  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }

  UpdateEntryPointsState();
}

IndigoPageActionController::~IndigoPageActionController() = default;

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

  // TODO: b/482792874 - Analyze the page and act on it, instead of just opening
  // a tab based on a fixed input.
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }
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

void IndigoPageActionController::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  UpdateEntryPointsState();
}

void IndigoPageActionController::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateEntryPointsState();
}

void IndigoPageActionController::UpdateEntryPointsState() {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  const bool should_show =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kForceIndigoSwitch) ||
      (optimization_guide_decision_ ==
           optimization_guide::OptimizationGuideDecision::kTrue &&
       CanUseModelExecutionFeatures());
  if (should_show == is_shown_) {
    return;
  }

  if (should_show) {
    page_action_controller_->Show(kActionIndigo);
    page_action_controller_->ShowSuggestionChip(kActionIndigo);
    base::RecordAction(base::UserMetricsAction("Indigo.PageAction.Show"));
  } else {
    page_action_controller_->Hide(kActionIndigo);
  }
  is_shown_ = should_show;
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

bool IndigoPageActionController::CanUseModelExecutionFeatures() const {
  if (!identity_manager_) {
    return false;
  }

  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return false;
  }

  AccountInfo info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  return info.capabilities.can_use_model_execution_features() ==
         signin::Tribool::kTrue;
}

}  // namespace indigo
