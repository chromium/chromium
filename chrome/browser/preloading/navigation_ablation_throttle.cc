// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/navigation_ablation_throttle.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/pattern.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace {
BASE_FEATURE(kNavigationLatencyAblation,
             "NavigationLatencyAblation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The amount of time to stall before resuming loading.
const base::FeatureParam<base::TimeDelta> kNavigationLatencyAblationDuration{
    &kNavigationLatencyAblation, "duration", base::Milliseconds(250)};

// The `base::MatchPattern` pattern that identifies the URLs should be ablated.
const base::FeatureParam<std::string> kAblationTargetPattern{
    &kNavigationLatencyAblation, "pattern", ""};

// Whether default search Search queries should be ablated.
const base::FeatureParam<bool> kShouldAblateDefaultSearchQueries{
    &kNavigationLatencyAblation, "ablate_default_search_queries", true};

// Whether default search navigations to any of the related hosts should be
// ablated.
const base::FeatureParam<bool> kShouldAblateDefaultSearchHost{
    &kNavigationLatencyAblation, "ablate_default_search_host", true};

// Whether the corpus of URLs that are not part of the default search host
// corpus should be ablated.
const base::FeatureParam<bool> kShouldAblateNonDefaultSearchHost{
    &kNavigationLatencyAblation, "ablate_non_default_search_host", true};

// Different URLs re grouped into ablation types, which can be configured to be
// ablated or not.
enum class AblationType {
  kDefaultSearchQuery = 0,
  kDefaultSearchHost = 1,
  kNonDefaultSearchHost = 2,
};

// The name of the throttle for logging purposes.
constexpr char kNavigationAblationThrottleName[] = "NavigationAblationThrottle";

// Return the type of navigation based on whether `navigation_url` is a search
// query or related to the user's default search provider.
AblationType GetAblationType(const GURL& navigation_url,
                             content::BrowserContext* browser_context) {
  auto* template_url_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!template_url_service)
    return AblationType::kNonDefaultSearchHost;

  auto* default_search = template_url_service->GetDefaultSearchProvider();
  if (!default_search)
    return AblationType::kNonDefaultSearchHost;

  if (default_search->IsSearchURL(navigation_url,
                                  template_url_service->search_terms_data())) {
    return AblationType::kDefaultSearchQuery;
  }

  for (auto url_ref : default_search->url_refs()) {
    if (url::Origin::Create(navigation_url) ==
        url::Origin::Create(GURL(url_ref.GetURL()))) {
      return AblationType::kDefaultSearchHost;
    }
  }

  return AblationType::kNonDefaultSearchHost;
}

bool ShouldCreateThrottle(content::NavigationHandle* navigation) {
  if (!base::FeatureList::IsEnabled(kNavigationLatencyAblation)) {
    return false;
  }

  // Exclude navigations for prerender or other MPArch.
  if (!navigation->IsInPrimaryMainFrame())
    return false;

  // Don't slow down activations from prerender or BFCache.
  if (navigation->IsPageActivation())
    return false;

  // Avoid ablating pages that likely are served from HTTP Cache.
  if (navigation->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK) {
    return false;
  }

  // Avoid ablataing client redirects as they may be part of a chain for a
  // single navigation.
  if (navigation->GetPageTransition() & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    return false;
  }

  const GURL& url = navigation->GetURL();

  // Ignore navigations that are not HTTP(S).
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Ignore navigations to IP addresses as these may be local network.
  if (url.HostIsIPAddress()) {
    return false;
  }

  std::string pattern = kAblationTargetPattern.Get();
  if (!pattern.empty() && base::MatchPattern(url.spec(), pattern)) {
    return true;
  }

  auto ablation_type =
      GetAblationType(url, navigation->GetWebContents()->GetBrowserContext());
  switch (ablation_type) {
    case AblationType::kDefaultSearchQuery:
      return kShouldAblateDefaultSearchQueries.Get();
    case AblationType::kDefaultSearchHost:
      return kShouldAblateDefaultSearchHost.Get();
    case AblationType::kNonDefaultSearchHost:
      return kShouldAblateNonDefaultSearchHost.Get();
  }
}

// A navigation throttle that deferes during WillStartRequest and resumes after
// a fixed duration.
class NavigationAblationThrottle : public content::NavigationThrottle {
 public:
  explicit NavigationAblationThrottle(content::NavigationHandle* navigation)
      : content::NavigationThrottle(navigation) {}
  ~NavigationAblationThrottle() override = default;

 private:
  const char* GetNameForLogging() override {
    return kNavigationAblationThrottleName;
  }

  ThrottleCheckResult WillStartRequest() override {
    timer_.Start(
        FROM_HERE, kNavigationLatencyAblationDuration.Get(),
        base::BindOnce(&NavigationAblationThrottle::ResumeLoading,
                       base::Unretained(this), base::TimeTicks::Now()));
    return content::NavigationThrottle::DEFER;
  }

  void ResumeLoading(base::TimeTicks start_time) {
    // Measure the actual wait time to make sure we are near the expected wait
    // time for the experiment arm.
    auto wait_time = base::TimeTicks::Now() - start_time;
    auto excess_wait_time =
        wait_time - kNavigationLatencyAblationDuration.Get();

    // Record the time spent waiting beyond what was configured.
    base::UmaHistogramTimes("Navigation.LatencyAblation.ExcessWaitTime",
                            excess_wait_time);

    Resume();
  }

  // A timer that resumes the loading after a fixed duration. The timer ensures
  // that if |this| is deleted the callback does not run and cause a UAF.
  base::OneShotTimer timer_;
};

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
MaybeCreateNavigationAblationThrottle(content::NavigationHandle* navigation) {
  DCHECK(navigation);
  if (!ShouldCreateThrottle(navigation)) {
    return nullptr;
  }
  return std::make_unique<NavigationAblationThrottle>(navigation);
}
