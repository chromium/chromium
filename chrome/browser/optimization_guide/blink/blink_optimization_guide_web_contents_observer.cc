// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_web_contents_observer.h"

#include <utility>
#include <vector>

#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_feature_flag_helper.h"
#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_inquirer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/previews_resource_loading_hints.mojom.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace optimization_guide {

BlinkOptimizationGuideWebContentsObserver::
    ~BlinkOptimizationGuideWebContentsObserver() = default;

void BlinkOptimizationGuideWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Currently the optimization guide supports only the main frame navigation.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Abort the inquiry for the previous main frame navigation.
  current_inquirer_.reset();

  // Don't support non-HTTP(S) navigation.
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  OptimizationGuideDecider* decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!decider)
    return;

  // Creates and starts a new inquiry for this main frame navigation.
  current_inquirer_ = BlinkOptimizationGuideInquirer::CreateAndStart(
      *navigation_handle, *decider);
  // The hints may not be ready for some optimization types because the inquirer
  // asynchronously asks the optimization guide server if the hints are not
  // locally cached.
  // TODO(https://crbug.com/1113980): Support the case where the hints get
  // available after navigation commit.
  // TODO(https://crbug.com/1113980): Add UMAs to record if the hints are
  // available on navigation commit ready.
  blink::mojom::BlinkOptimizationGuideHintsPtr hints =
      current_inquirer_->GetHints();

  // Tentatively use the Previews interface to talk with the renderer.
  // TODO(https://crbug.com/1113980): Implement our own interface.
  mojo::AssociatedRemote<previews::mojom::PreviewsResourceLoadingHintsReceiver>
      hints_receiver_associated;
  if (navigation_handle->GetRenderFrameHost()
          ->GetRemoteAssociatedInterfaces()) {
    navigation_handle->GetRenderFrameHost()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&hints_receiver_associated);
  }

  // Sends the hints currently available to the renderer.
  hints_receiver_associated->SetBlinkOptimizationGuideHints(
      current_inquirer_->GetHints());
}

BlinkOptimizationGuideWebContentsObserver::
    BlinkOptimizationGuideWebContentsObserver(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OptimizationGuideDecider* decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!decider)
    return;

  // Register the optimization types which we want to subscribe to.
  std::vector<proto::OptimizationType> opts;
  if (features::ShouldUseOptimizationGuideForDelayAsyncScript())
    opts.push_back(proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION);
  if (features::
          ShouldUseOptimizationGuideForDelayCompetingLowPriorityRequests()) {
    opts.push_back(
        proto::OptimizationType::DELAY_COMPETING_LOW_PRIORITY_REQUESTS);
  }
  if (!opts.empty())
    decider->RegisterOptimizationTypes(opts);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BlinkOptimizationGuideWebContentsObserver)

}  // namespace optimization_guide
