// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/base_search_prefetch_request.h"

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "net/base/load_flags.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/omnibox/geolocation_header.h"
#endif  // defined(OS_ANDROID)

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
                       base::StringPiece custom_reason) override {
    cancelled_or_paused_ = true;
  }

  void Resume() override {}

  void PauseReadingBodyFromNet() override { cancelled_or_paused_ = true; }

  void RestartWithFlags(int additional_load_flags) override {
    cancelled_or_paused_ = true;
  }

  void RestartWithURLResetAndFlags(int additional_load_flags) override {
    cancelled_or_paused_ = true;
  }

  void RestartWithURLResetAndFlagsNow(int additional_load_flags) override {
    cancelled_or_paused_ = true;
  }

  void RestartWithModifiedHeadersNow(
      const net::HttpRequestHeaders& modified_headers) override {
    cancelled_or_paused_ = true;
  }

  bool cancelled_or_paused() const { return cancelled_or_paused_; }

 private:
  bool cancelled_or_paused_ = false;
};

}  // namespace

BaseSearchPrefetchRequest::BaseSearchPrefetchRequest(
    const GURL& prefetch_url,
    base::OnceClosure report_error_callback)
    : prefetch_url_(prefetch_url),
      report_error_callback_(std::move(report_error_callback)) {}

BaseSearchPrefetchRequest::~BaseSearchPrefetchRequest() = default;

// static
net::NetworkTrafficAnnotationTag
BaseSearchPrefetchRequest::NetworkAnnotationForPrefetch() {
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

bool BaseSearchPrefetchRequest::StartPrefetchRequest(Profile* profile) {
  net::NetworkTrafficAnnotationTag network_traffic_annotation =
      NetworkAnnotationForPrefetch();

  url::Origin prefetch_origin = url::Origin::Create(prefetch_url_);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  // This prefetch is not as high priority as navigation, but due to its
  // navigation speeding and relatively high likelihood of being served to a
  // navigation, the request is relatively high priority.
  resource_request->priority = net::MEDIUM;
  resource_request->url = prefetch_url_;
  // Search prefetch URL Loaders should check |report_raw_headers| on the
  // intercepted request to clear out the raw headers when |report_raw_headers|
  // is false.
  resource_request->report_raw_headers = true;
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

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
  resource_request->headers.SetHeader("Upgrade-Insecure-Requests", "1");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                      embedder_support::GetUserAgent());
  resource_request->headers.SetHeader(content::kCorsExemptPurposeHeaderName,
                                      "prefetch");
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAccept,
      content::FrameAcceptHeaderValue(/*allow_sxg_responses=*/true, profile));

  bool js_enabled = profile->GetPrefs() && profile->GetPrefs()->GetBoolean(
                                               prefs::kWebKitJavascriptEnabled);

  AddClientHintsHeadersToPrefetchNavigation(
      resource_request->url, &(resource_request->headers), profile,
      profile->GetClientHintsControllerDelegate(),
      /*is_ua_override_on=*/false, js_enabled);

#if defined(OS_ANDROID)
  base::Optional<std::string> geo_header =
      GetGeolocationHeaderIfAllowed(resource_request->url, profile);
  if (geo_header) {
    resource_request->headers.AddHeaderFromString(geo_header.value());
  }
#endif  // defined(OS_ANDROID)

  // Before sending out the request, allow throttles to modify the request (not
  // the URL). The rest of the URL Loader throttle calls are captured in the
  // navigation stack. Headers can be added by throttles at this point, which we
  // want to capture.
  auto wc_getter =
      base::BindRepeating([]() -> content::WebContents* { return nullptr; });
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      content::CreateContentBrowserURLLoaderThrottles(
          *resource_request, profile, std::move(wc_getter),
          /*navigation_ui_data=*/nullptr,
          content::RenderFrameHost::kNoFrameTreeNodeId);

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service);
  auto* default_search = template_url_service->GetDefaultSearchProvider();
  DCHECK(default_search);

  std::u16string prefetch_url_search_terms;

  default_search->ExtractSearchTermsFromURL(
      prefetch_url_, template_url_service->search_terms_data(),
      &prefetch_url_search_terms);

  bool should_defer = false;
  for (auto& throttle : throttles) {
    CheckForCancelledOrPausedDelegate cancel_or_pause_delegate;
    throttle->set_delegate(&cancel_or_pause_delegate);
    throttle->WillStartRequest(resource_request.get(), &should_defer);
    // Make sure throttles are deleted before |cancel_or_pause_delegate| in case
    // they call into the delegate in the destructor.
    throttle.reset();

    std::u16string new_url_search_terms;

    // Check that search terms still match. Google URLs can be changed by
    // AndroidDarkSearch (and in other cases like safe search). Make sure the
    // URL still has the same search terms for the DSE.
    default_search->ExtractSearchTermsFromURL(
        resource_request->url, template_url_service->search_terms_data(),
        &new_url_search_terms);

    if (should_defer || new_url_search_terms != prefetch_url_search_terms ||
        cancel_or_pause_delegate.cancelled_or_paused()) {
      return false;
    }
  }

  prefetch_url_ = resource_request->url;

  current_status_ = SearchPrefetchStatus::kInFlight;

  StartPrefetchRequestInternal(profile, std::move(resource_request),
                               network_traffic_annotation);
  return true;
}

void BaseSearchPrefetchRequest::CancelPrefetch() {
  DCHECK(current_status_ == SearchPrefetchStatus::kInFlight ||
         current_status_ == SearchPrefetchStatus::kCanBeServed);
  current_status_ = SearchPrefetchStatus::kRequestCancelled;
  StopPrefetch();
}

void BaseSearchPrefetchRequest::ErrorEncountered() {
  DCHECK(!report_error_callback_.is_null());
  DCHECK(current_status_ == SearchPrefetchStatus::kInFlight ||
         current_status_ == SearchPrefetchStatus::kCanBeServed ||
         current_status_ == SearchPrefetchStatus::kCanBeServedAndUserClicked);
  current_status_ = SearchPrefetchStatus::kRequestFailed;
  std::move(report_error_callback_).Run();
  StopPrefetch();
}

void BaseSearchPrefetchRequest::MarkPrefetchAsServable() {
  DCHECK(current_status_ == SearchPrefetchStatus::kInFlight);
  current_status_ = SearchPrefetchStatus::kCanBeServed;
}

void BaseSearchPrefetchRequest::MarkPrefetchAsComplete() {
  DCHECK(current_status_ == SearchPrefetchStatus::kInFlight ||
         current_status_ == SearchPrefetchStatus::kCanBeServed ||
         current_status_ == SearchPrefetchStatus::kCanBeServedAndUserClicked);
  current_status_ = SearchPrefetchStatus::kComplete;
}

void BaseSearchPrefetchRequest::MarkPrefetchAsClicked() {
  DCHECK(current_status_ == SearchPrefetchStatus::kCanBeServed);
  current_status_ = SearchPrefetchStatus::kCanBeServedAndUserClicked;
}

bool BaseSearchPrefetchRequest::CanServePrefetchRequest(
    const scoped_refptr<net::HttpResponseHeaders> headers) {
  if (!headers)
    return false;

  // Any 200 response can be served.
  if (headers->response_code() >= net::HTTP_OK &&
      headers->response_code() < net::HTTP_MULTIPLE_CHOICES) {
    return true;
  }

  return false;
}
