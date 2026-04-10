// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_TAB_HELPER_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_TAB_HELPER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}

class OptimizationGuideKeyedService;
class TemplateURLService;
class PrefService;

namespace finds {

class FindsService;
class FindsTabHelperTest;

// Tab helper to track user opt-in eligibility.
class FindsTabHelper : public content::WebContentsObserver,
                       public content::WebContentsUserData<FindsTabHelper> {
 public:
  FindsTabHelper(const FindsTabHelper&) = delete;
  FindsTabHelper& operator=(const FindsTabHelper&) = delete;
  ~FindsTabHelper() override;

  // Determine whether the current platform is supported for the finds feature.
  static bool IsSupportedPlatform();

 private:
  explicit FindsTabHelper(content::WebContents* web_contents,
                          FindsService* finds_service,
                          OptimizationGuideKeyedService* opt_guide_service,
                          TemplateURLService* template_url_service,
                          PrefService* pref_service);
  friend class content::WebContentsUserData<FindsTabHelper>;
  friend class FindsTabHelperTest;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Called when observing navigation metadata hints through an optimization
  // guide decision.
  void OnOptimizationGuideDecision(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Checks if the user has returned to the SRP enough from a backpress to
  // trigger the opt in promo if the threshold is met.
  void CheckSRPReturnCountAndMaybeTriggerOptIn(
      content::NavigationHandle* navigation_handle);

  raw_ptr<FindsService> finds_service_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<OptimizationGuideKeyedService> opt_guide_service_ = nullptr;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  int srp_return_count_ = 0;
  base::WeakPtrFactory<FindsTabHelper> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_TAB_HELPER_H_
