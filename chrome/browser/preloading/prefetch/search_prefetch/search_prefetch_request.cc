// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_request.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/prefetch/prefetch_headers.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/streaming_search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/check_is_test.h"
#include "chrome/browser/android/omnibox/geolocation_header.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

// A custom URLLoaderThrottle delegate that is very sensitive. Anything that
// would delay or cancel the request is treated the same, which would prevent
// the prefetch request.
class CheckForCancelledOrPausedDelegate
    : public blink::URLLoaderThrottle::Delegate {
 public:
  CheckForCancelledOrPausedDelegate() = default;
  ~CheckForCancelledOrPausedDelegate() override = default;

  CheckForCancelledOrPausedDelegate(const CheckForCancelledOrPausedDelegate&) =
      delete;
  CheckForCancelledOrPausedDelegate& operator=(
      const CheckForCancelledOrPausedDelegate&) = delete;

  // URLLoaderThrottle::Delegate:
  void CancelWithError(int error_code,
                       std::string_view custom_reason) override {
    cancelled_ = true;
  }

  void Resume() override {}

  bool cancelled() const { return cancelled_; }

 private:
  bool cancelled_ = false;
};

// Computes the user agent value that should set for the User-Agent header.
std::string GetUserAgentValue(const net::HttpRequestHeaders& headers) {
  return embedder_support::GetUserAgent();
}

// Used for StateTransitions matching.
const char* SearchPrefetchStatusToString(SearchPrefetchStatus status) {
  switch (status) {
    case SearchPrefetchStatus::kNotStarted:
      return "NotStarted";
    case SearchPrefetchStatus::kInFlight:
      return "InFlight";
    case SearchPrefetchStatus::kCanBeServed:
      return "CanBeServed";
    case SearchPrefetchStatus::kComplete:
      return "Complete";
    case SearchPrefetchStatus::kRequestFailed:
      return "RequestFailed";
    case SearchPrefetchStatus::kPrefetchServedForRealNavigation:
      return "kPrefetchServedForRealNavigation";
  }
}

void MaybeRecordTraceFromSearchPrefetchRequestStartToNavigationIntercepted(
    SearchPrefetchRequest* search_prefetch_request,
    base::TimeTicks time_start_prefetch_request) {
  if (time_start_prefetch_request.is_null()) {
    return;
  }

  const char kSearchPrefetchRequestStartToNavigationIntercepted[] =
      "SearchPrefetchRequestStartToNavigationIntercepted";
  const auto trace_id =
      TRACE_ID_WITH_SCOPE(kSearchPrefetchRequestStartToNavigationIntercepted,
                          TRACE_ID_LOCAL(search_prefetch_request));
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "navigation", kSearchPrefetchRequestStartToNavigationIntercepted,
      trace_id, time_start_prefetch_request);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "navigation", kSearchPrefetchRequestStartToNavigationIntercepted,
      trace_id, base::TimeTicks::Now());
}

}  // namespace

SearchPrefetchRequest::SearchPrefetchRequest(
    const GURL& canonical_search_url,
    const GURL& prefetch_url,
    bool navigation_prefetch,
    content::PreloadingAttempt* prefetch_preloading_attempt,
    base::OnceCallback<void(bool)> report_error_callback)
    : canonical_search_url_(canonical_search_url),
      prefetch_url_(prefetch_url),
      navigation_prefetch_(navigation_prefetch),
      prefetch_preloading_attempt_(
          prefetch_preloading_attempt
              ? prefetch_preloading_attempt->GetWeakPtr()
              : nullptr),
      report_error_callback_(std::move(report_error_callback)) {
  base::trace_event::EmitNamedTrigger("search-prefetch-start");
}

SearchPrefetchRequest::~SearchPrefetchRequest() {
  StopPrerender();
  // If the loader has been taken by a real navigation.
  if (!streaming_url_loader_) {
    return;
  }
  streaming_url_loader_->ClearOwnerPointer();
  // If it is the last instance owning StreamingSearchPrefetchURLLoader, it
  // should be SearchPrefetchService that calls this method.
  // In this case, there is no StreamingSearchPrefetchURLLoader instance that
  // would be needed.
  streaming_url_loader_.reset();
}

// static
net::NetworkTrafficAnnotationTag
SearchPrefetchRequest::NetworkAnnotationForPrefetch() {
  return net::DefineNetworkTrafficAnnotation("search_prefetch_service", R"(
        semantics {
          sender: "Search Prefetch Service"
          description:
            "Prefetches search results page (HTML) based on omnibox hints "
            "provided by the user's default search engine. This allows the "
            "prefetched content to be served when the user navigates to the "
            "omnibox hint."
          trigger:
            "User typing in the omnibox and the default search provider "
            "indicates the provided omnibox hint entry is likely to be "
            "navigated which would result in loading a search results page for "
            "that hint."
          data: "Credentials if user is signed in."
          destination: OTHER
          destination_other: "The user's default search engine."
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature by opting out of 'Preload pages "
            "for faster browsing and searching'"
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            NetworkPredictionOptions {
              NetworkPredictionOptions: 2
            }
          }
        })");
}

bool SearchPrefetchRequest::StartPrefetchRequest(Profile* profile) {
  TRACE_EVENT0("loading", "SearchPrefetchRequest::StartPrefetchRequest");
  time_start_prefetch_request_ = base::TimeTicks::Now();

  url::Origin prefetch_origin = url::Origin::Create(prefetch_url_);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  // This prefetch is not as high priority as navigation, but due to its
  // navigation speeding and relatively high likelihood of being served to a
  // navigation, the request is relatively high priority.
  resource_request->priority =
      navigation_prefetch_ ? net::HIGHEST : net::MEDIUM;
  resource_request->url = prefetch_url_;
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->method = "GET";
  resource_request->mode = network::mojom::RequestMode::kNavigate;
  resource_request->site_for_cookies =
      net::SiteForCookies::FromUrl(prefetch_url_);
  resource_request->destination = network::mojom::RequestDestination::kDocument;
  resource_request->resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  // We don't handle redirects, so |kOther| makes sense here.
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, prefetch_origin, prefetch_origin,
      resource_request->site_for_cookies);
  resource_request->referrer_policy = net::ReferrerPolicy::NO_REFERRER;
  resource_request->update_first_party_url_on_redirect = true;

  bool js_enabled = profile->GetPrefs() && profile->GetPrefs()->GetBoolean(
                                               prefs::kWebKitJavascriptEnabled);

  AddClientHintsHeadersToPrefetchNavigation(
      prefetch_origin, &(resource_request->headers), profile,
      profile->GetClientHintsControllerDelegate(),
      /*is_ua_override_on=*/false, js_enabled);

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
  resource_request->headers.SetHeader("Upgrade-Insecure-Requests", "1");

  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kUserAgent,
      GetUserAgentValue(resource_request->headers));
  resource_request->headers.SetHeader(content::kCorsExemptPurposeHeaderName,
                                      "prefetch");
  resource_request->headers.SetHeader(
      prefetch::headers::kSecPurposeHeaderName,
      prefetch::headers::kSecPurposePrefetchHeaderValue);
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAccept,
      content::FrameAcceptHeaderValue(/*allow_sxg_responses=*/true, profile));

#if BUILDFLAG(IS_ANDROID)
  base::TimeTicks geo_header_start_timestamp = base::TimeTicks::Now();
  std::optional<std::string> geo_header =
      GetGeolocationHeaderIfAllowed(resource_request->url, profile);
  if (geo_header) {
    resource_request->headers.AddHeaderFromString(geo_header.value());

    std::string histogram_name =
        "Omnibox.SearchPrefetch.GeoLocationHeaderTime.";
    histogram_name.append(navigation_prefetch_ ? "NavigationPrefetch"
                                               : "SuggestionPrefetch");
    base::UmaHistogramTimes(
        histogram_name, (base::TimeTicks::Now() - geo_header_start_timestamp));
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Before sending out the request, allow throttles to modify the request (not
  // the URL). The rest of the URL Loader throttle calls are captured in the
  // navigation stack. Headers can be added by throttles at this point, which we
  // want to capture.
  auto wc_getter =
      base::BindRepeating([]() -> content::WebContents* { return nullptr; });
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      content::CreateContentBrowserURLLoaderThrottles(
          *resource_request, profile, std::move(wc_getter),
          /*navigation_ui_data=*/nullptr, content::FrameTreeNodeId(),
          /*navigation_id=*/std::nullopt);

  bool should_defer = false;
  {
    TRACE_EVENT0(
        "loading",
        "SearchPrefetchRequest::StartPrefetchRequest.ExecuteThrottles");

    for (auto& throttle : throttles) {
      CheckForCancelledOrPausedDelegate cancel_or_pause_delegate;
      throttle->set_delegate(&cancel_or_pause_delegate);

      {
        TRACE_EVENT0(
            "loading",
            "SearchPrefetchRequest::StartPrefetchRequest.WillStartRequest");
        throttle->WillStartRequest(resource_request.get(), &should_defer);
      }
      // Make sure throttles are deleted before |cancel_or_pause_delegate| in
      // case they call into the delegate in the destructor.
      throttle.reset();

      GURL new_canonical_search_url;

      // Check that the search preloading URL has not been altered by a
      // navigation throttle such that its canonical representation has changed.
      HasCanonicalPreloadingOmniboxSearchURL(resource_request->url, profile,
                                             &new_canonical_search_url);

      if (should_defer || new_canonical_search_url != canonical_search_url_ ||
          cancel_or_pause_delegate.cancelled()) {
        return false;
      }
    }
  }

  prefetch_url_ = resource_request->url;

  SetSearchPrefetchStatus(SearchPrefetchStatus::kInFlight);

  StartPrefetchRequestInternal(profile, std::move(resource_request),
                               std::move(report_error_callback_));
  return true;
}

void SearchPrefetchRequest::MaybeStartPrerenderSearchResult(
    PrerenderManager& prerender_manager,
    const GURL& prerender_url,
    content::PreloadingAttempt& attempt) {
  // Prerendering is supposed to be requested after prefetch received a servable
  // response and take over the prefetched main resource response. When
  // prerendering is requested while prefetching is still running, it has to
  // wait until the completion of that. This procedure depends on the progress
  // of prefetching as follows:
  //    *1  |         *2     |    *3         | *4
  //  prefetch started     received      prerender started

  switch (current_status_) {
    case SearchPrefetchStatus::kNotStarted:
      // Case1: This request has been canceled before it starts sending network
      // requests (see `StartPrefetchRequest`), so prerender should not be
      // triggered.
      attempt.SetEligibility(ToPreloadingEligibility(
          ChromePreloadingEligibility::kPrefetchNotStarted));
      return;
    case SearchPrefetchStatus::kInFlight:
    case SearchPrefetchStatus::kCanBeServed:
    case SearchPrefetchStatus::kComplete:
      break;
    case SearchPrefetchStatus::kRequestFailed:
      // Case N: The prefetch request failed, or has failed. Prerender cannot
      // reuse the response and will fail for sure, so this does not start
      // prerendering.
      attempt.SetEligibility(ToPreloadingEligibility(
          ChromePreloadingEligibility::kPrefetchFailed));
      return;
    case SearchPrefetchStatus::kPrefetchServedForRealNavigation:
      NOTREACHED_IN_MIGRATION();
  }

  // maintain a weak ptr so that this can cancel prerendering when
  // needed.
  prerender_url_ = prerender_url;
  prerender_manager_ = prerender_manager.GetWeakPtr();
  prerender_preloading_attempt_ = attempt.GetWeakPtr();

  if (servable_response_code_received_) {
    // Case 3, 4: This can start prerendering because it has received a
    // response.
    // TODO(crbug.com/40214220): Do not start prerendering if this
    // request is about to expire.
    prerender_manager_->StartPrerenderSearchResult(
        canonical_search_url_, prerender_url, prerender_preloading_attempt_);
  }
}

void SearchPrefetchRequest::ErrorEncountered() {
  SetSearchPrefetchStatus(SearchPrefetchStatus::kRequestFailed);
  StopPrefetch();
  StopPrerender();
}

void SearchPrefetchRequest::OnServableResponseCodeReceived() {
  servable_response_code_received_ = true;

  if (!prerender_manager_) {
    return;
  }

  // TODO(crbug.com/40214220): Do not start prerendering if this request
  // is about to expire.
  prerender_manager_->StartPrerenderSearchResult(
      canonical_search_url_, prerender_url_, prerender_preloading_attempt_);
}

void SearchPrefetchRequest::MarkPrefetchAsServable() {
  SetSearchPrefetchStatus(SearchPrefetchStatus::kCanBeServed);
}

void SearchPrefetchRequest::ResetPrerenderUpgrader() {
  prerender_manager_ = nullptr;
  prerender_preloading_attempt_ = nullptr;
  prerender_url_ = GURL();
}

void SearchPrefetchRequest::MarkPrefetchAsComplete() {
  SetSearchPrefetchStatus(SearchPrefetchStatus::kComplete);
}

void SearchPrefetchRequest::MarkPrefetchAsServed() {
  SetSearchPrefetchStatus(
      SearchPrefetchStatus::kPrefetchServedForRealNavigation);
  UMA_HISTOGRAM_TIMES("Omnibox.SearchPrefetch.ClickToNavigationIntercepted",
                      base::TimeTicks::Now() - time_clicked_);
}

void SearchPrefetchRequest::RecordClickTime() {
  time_clicked_ = base::TimeTicks::Now();
}

scoped_refptr<StreamingSearchPrefetchURLLoader>
SearchPrefetchRequest::TakeSearchPrefetchURLLoader() {
  TRACE_EVENT0("loading", "SearchPrefetchRequest::TakeSearchPrefetchURLLoader");
  MaybeRecordTraceFromSearchPrefetchRequestStartToNavigationIntercepted(
      this, time_start_prefetch_request_);
  DCHECK(streaming_url_loader_);
  // This method should be called upon serving, so the service does not want to
  // keep the request.
  streaming_url_loader_->ClearOwnerPointer();

  return std::move(streaming_url_loader_);
}

SearchPrefetchURLLoader::RequestHandler
SearchPrefetchRequest::CreateResponseReader() {
  DCHECK(streaming_url_loader_);
  if (!servable_response_code_received_) {
    // It is not expected to reach here, as DSE prerender should only be
    // triggered after `this` received servable response. But other triggers may
    // unexpectedly trigger prerendering due to https://crbug.com/1484914.
    return {};
  }
  TRACE_EVENT0("loading", "SearchPrefetchRequest::CreateResponseReader");
  MaybeRecordTraceFromSearchPrefetchRequestStartToNavigationIntercepted(
      this, time_start_prefetch_request_);
  return StreamingSearchPrefetchURLLoader::
      GetCallbackForReadingViaResponseReader(streaming_url_loader_);
}

void SearchPrefetchRequest::StartPrefetchRequestInternal(
    Profile* profile,
    std::unique_ptr<network::ResourceRequest> resource_request,
    base::OnceCallback<void(bool)> report_error_callback) {
  TRACE_EVENT0("loading",
               "SearchPrefetchRequest::StartPrefetchRequestInternal");
  prefetch_url_ = resource_request->url;
  streaming_url_loader_ =
      base::MakeRefCounted<StreamingSearchPrefetchURLLoader>(
          this, profile, navigation_prefetch_, std::move(resource_request),
          NetworkAnnotationForPrefetch(), std::move(report_error_callback));
}

void SearchPrefetchRequest::StopPrefetch() {
  if (!streaming_url_loader_) {
    return;
  }
  // If it is the last reference to the `streaming_url_loader_`, we can release
  // it directly and its callers are aware of it can be deleted.
  streaming_url_loader_->ClearOwnerPointer();
  streaming_url_loader_.reset();
}

void SearchPrefetchRequest::StopPrerender() {
  if (prerender_manager_) {
    prerender_manager_->StopPrerenderSearchResult(canonical_search_url_);
    prerender_manager_ = nullptr;
    prerender_preloading_attempt_ = nullptr;
    prerender_url_ = GURL();
  }
}

void SearchPrefetchRequest::SetPrefetchAttemptFailureReason(
    content::PreloadingFailureReason reason) {
  if (!prefetch_preloading_attempt_)
    return;

  prefetch_preloading_attempt_->SetFailureReason(reason);

  // For prefetch it is possible that the prefetch could be used for a different
  // navigation after failure which is out of scope with Preloading APIs. Reset
  // the PreloadingAttempt to avoid setting the values for different navigation
  // than the one we are observing.
  prefetch_preloading_attempt_.reset();
}

void SearchPrefetchRequest::SetLoaderDestructionCallbackForTesting(
    base::OnceClosure streaming_url_loader_destruction_callback) {
  streaming_url_loader_->set_on_destruction_callback_for_testing(  // IN-TEST
      std::move(streaming_url_loader_destruction_callback));
}

void SearchPrefetchRequest::SetPrefetchAttemptTriggeringOutcome(
    content::PreloadingTriggeringOutcome outcome) {
  if (!prefetch_preloading_attempt_)
    return;

  prefetch_preloading_attempt_->SetTriggeringOutcome(outcome);
}

void SearchPrefetchRequest::SetSearchPrefetchStatus(
    SearchPrefetchStatus new_status) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<SearchPrefetchStatus>>
      allowed_transitions(base::StateTransitions<SearchPrefetchStatus>({
          {SearchPrefetchStatus::kNotStarted,
           {SearchPrefetchStatus::kInFlight}},

          {SearchPrefetchStatus::kInFlight,
           {SearchPrefetchStatus::kCanBeServed,
            SearchPrefetchStatus::kRequestFailed}},

          {SearchPrefetchStatus::kCanBeServed,
           {SearchPrefetchStatus::kComplete,
            SearchPrefetchStatus::kRequestFailed,
            SearchPrefetchStatus::kPrefetchServedForRealNavigation}},

          {SearchPrefetchStatus::kComplete,
           {SearchPrefetchStatus::kPrefetchServedForRealNavigation}},

          {SearchPrefetchStatus::kPrefetchServedForRealNavigation, {}},

          {SearchPrefetchStatus::kRequestFailed, {}},
      }));
  DCHECK_STATE_TRANSITION(allowed_transitions,
                          /*old_state=*/current_status_,
                          /*new_state=*/new_status);
#endif  // DCHECK_IS_ON()
  current_status_ = new_status;

  // Update the PreloadingTriggeringOutcome once we update status.
  switch (current_status_) {
    case SearchPrefetchStatus::kNotStarted:
      // When prefetch is not started, we consider the
      // PreloadingTriggeringOutcome as kUnspecified. The exact reason why
      // prefetch is not started is recorded in PreloadingEligibility.
      return;
    case SearchPrefetchStatus::kInFlight:
      // Once prefetch started set TriggeringOutcome to kRunning.
      SetPrefetchAttemptTriggeringOutcome(
          content::PreloadingTriggeringOutcome::kRunning);
      return;
    case SearchPrefetchStatus::kCanBeServed:
      // Mark prefetch to ready, once we can serve prefetch. With
      // PreloadingAttempt, ready means the attempt can be used when needed.
      SetPrefetchAttemptTriggeringOutcome(
          content::PreloadingTriggeringOutcome::kReady);
      return;
    case SearchPrefetchStatus::kComplete:
      // Don't update the TriggeringOutcome here as we have already set the
      // TriggeringOutcome when the status was updated to kCanServed.
      return;
    case SearchPrefetchStatus::kRequestFailed:
      // Since we are cancelling prefetch when the request failed, we consider
      // it as a failure with PreloadingTriggeringOutcome.
      SetPrefetchAttemptTriggeringOutcome(
          content::PreloadingTriggeringOutcome::kFailure);
      return;
    case SearchPrefetchStatus::kPrefetchServedForRealNavigation:
      // Once prefetch is served mark it as success.
      SetPrefetchAttemptTriggeringOutcome(
          content::PreloadingTriggeringOutcome::kSuccess);
      return;
  }
}

std::ostream& operator<<(std::ostream& o, const SearchPrefetchStatus& s) {
  return o << SearchPrefetchStatusToString(s);
}
