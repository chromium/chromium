// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_page_context_eligibility_observer.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/glic_page_context_eligibility_metadata.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace glic {

namespace {

bool GetIsEligible(const optimization_guide::OptimizationMetadata& metadata) {
  auto eligibility_metadata = metadata.ParsedMetadata<
      optimization_guide::proto::GlicPageContextEligibilityMetadata>();
  if (!eligibility_metadata) {
    return features::kGlicPageContextEligibilityAllowNoMetadata.Get();
  }
  // Otherwise, get the value from the specific metadata.
  return eligibility_metadata->is_eligible();
}

}  // namespace

// static
std::unique_ptr<GlicPageContextEligibilityObserver>
GlicPageContextEligibilityObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(features::kGlicPageContextEligibility)) {
    return nullptr;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_keyed_service) {
    return nullptr;
  }
  return base::WrapUnique(new GlicPageContextEligibilityObserver(
      web_contents, optimization_guide_keyed_service));
}

GlicPageContextEligibilityObserver::GlicPageContextEligibilityObserver(
    content::WebContents* web_contents,
    OptimizationGuideKeyedService* optimization_guide_keyed_service)
    : content::WebContentsObserver(web_contents),
      optimization_guide_keyed_service_(optimization_guide_keyed_service) {
  CHECK(optimization_guide_keyed_service_);
  // Register to start fetching signals for page context eligibility.
  // No-op if user is not permitted or already registered via another observer.
  optimization_guide_keyed_service_->RegisterOptimizationTypes(
      {optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY});
}

// static
bool GlicPageContextEligibilityObserver::MaybeGetEligibilityForWebContents(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> eligibility_callback) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return false;
  }
  tabs::TabFeatures* features = tab->GetTabFeatures();
  if (!features) {
    return false;
  }
  GlicPageContextEligibilityObserver* context_eligibility_observer =
      features->glic_page_context_eligibility_observer();
  if (!context_eligibility_observer) {
    return false;
  }

  context_eligibility_observer->GetEligibility(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(eligibility_callback),
          ::features::kGlicPageContextEligibilityAllowNoMetadata.Get()));
  return true;
}

GlicPageContextEligibilityObserver::~GlicPageContextEligibilityObserver() =
    default;

void GlicPageContextEligibilityObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore sub-frame and uncommitted navigations.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  if (!navigation_handle->HasCommitted()) {
    return;
  }
  // Ignore fragment changes.
  if (navigation_handle->GetPreviousPrimaryMainFrameURL().GetWithoutRef() ==
      navigation_handle->GetURL().GetWithoutRef()) {
    return;
  }

  // Reset state.
  is_eligible_ = std::nullopt;
  is_currently_waiting_ = false;
  eligibility_callbacks_.clear();

  // The answer from the page call is not correct for unpermitted users, so do
  // not capture it.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile->IsOffTheRecord(), profile->GetPrefs())) {
    return;
  }

  is_currently_waiting_ = true;
  optimization_guide_keyed_service_->CanApplyOptimization(
      navigation_handle->GetURL(),
      optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY,
      base::BindOnce(&GlicPageContextEligibilityObserver::OnEligibilityResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     navigation_handle->GetURL()));
}

void GlicPageContextEligibilityObserver::OnEligibilityResult(
    const GURL& url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (url != web_contents()->GetLastCommittedURL()) {
    return;
  }
  is_currently_waiting_ = false;

  // Determine eligibility.
  is_eligible_ = GetIsEligible(metadata);

  NotifyCallbacks(*is_eligible_);
}

void GlicPageContextEligibilityObserver::GetEligibility(
    base::OnceCallback<void(bool)> eligibility_callback) {
  if (is_eligible_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(eligibility_callback), *is_eligible_));
    return;
  }

  eligibility_callbacks_.push_back(std::move(eligibility_callback));

  if (is_currently_waiting_) {
    return;
  }

  // Otherwise call opt guide on-demand.
  is_currently_waiting_ = true;
  optimization_guide_keyed_service_->CanApplyOptimizationOnDemand(
      {web_contents()->GetLastCommittedURL()},
      {optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY},
      optimization_guide::proto::CONTEXT_GLIC_PAGE_CONTEXT,
      base::BindRepeating(
          [&](base::WeakPtr<GlicPageContextEligibilityObserver> observer,
              const GURL& url,
              const base::flat_map<
                  optimization_guide::proto::OptimizationType,
                  optimization_guide::OptimizationGuideDecisionWithMetadata>&
                  decisions) {
            auto it = decisions.find(
                optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY);
            if (it != decisions.end()) {
              observer->OnEligibilityResult(url, it->second.decision,
                                            it->second.metadata);
            } else {
              // This shouldn't happen if this gets called, so just invoke
              // result with false.
              observer->OnEligibilityResult(
                  url, optimization_guide::OptimizationGuideDecision::kFalse,
                  {});
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void GlicPageContextEligibilityObserver::NotifyCallbacks(bool is_eligible) {
  for (auto& eligibility_callback : eligibility_callbacks_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(eligibility_callback), is_eligible));
  }
  eligibility_callbacks_.clear();
}

}  // namespace glic
