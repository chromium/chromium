// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/typed_macros.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_switches.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_urls.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
constexpr char kChallengeItemKey[] = "challenge";
constexpr char kSessionIdItemKey[] = "session_id";
const size_t kMaxAssertionRequestsAllowed = 5;

constexpr std::string_view kRotationResultHistogramName =
    "Signin.BoundSessionCredentials.CookieRotationResult";
constexpr std::string_view kRotationTotalDurationHistogramName =
    "Signin.BoundSessionCredentials.CookieRotationTotalDuration";

bool IsExpectedCookie(
    const GURL& url,
    const std::string& cookie_name,
    const network::mojom::CookieOrLineWithAccessResultPtr& cookie_ptr) {
  if (cookie_ptr->access_result.status.IsInclude()) {
    CHECK(cookie_ptr->cookie_or_line->is_cookie());
    const net::CanonicalCookie& cookie =
        cookie_ptr->cookie_or_line->get_cookie();
    return (cookie.Name() == cookie_name) &&
           cookie.IsDomainMatch(url.GetHost());
  }
  return false;
}

std::string UpdateDebugInfoAndSerializeToHeader(
    bound_session_credentials::RotationDebugInfo& debug_info) {
  *debug_info.mutable_request_time() =
      bound_session_credentials::TimeToTimestamp(base::Time::Now());
  std::string serialized = debug_info.SerializeAsString();
  return base::Base64Encode(serialized);
}

std::string_view GetRotationHttpResultHistogramName(
    bool had_previously_received_challenge,
    bool request_contained_challenge_response) {
  static constexpr std::string_view kHistogramNameWithoutChallenge =
      "Signin.BoundSessionCredentials.CookieRotationHttpResult."
      "WithoutChallenge";
  static constexpr std::string_view kHistogramNameWithCachedChallenge =
      "Signin.BoundSessionCredentials.CookieRotationHttpResult."
      "WithCachedChallenge";
  static constexpr std::string_view kHistogramNameWithFreshChallenge =
      "Signin.BoundSessionCredentials.CookieRotationHttpResult."
      "WithFreshChallenge";
  if (had_previously_received_challenge) {
    DUMP_WILL_BE_CHECK(request_contained_challenge_response);
    return kHistogramNameWithFreshChallenge;
  }

  return request_contained_challenge_response
             ? kHistogramNameWithCachedChallenge
             : kHistogramNameWithoutChallenge;
}

// LINT.IfChange(GetRotationHistogramTriggerSuffix)
std::string_view GetRotationHistogramTriggerSuffix(
    BoundSessionRefreshCookieFetcher::Trigger trigger) {
  using enum BoundSessionRefreshCookieFetcher::Trigger;
  switch (trigger) {
    case kOther:
      return ".Other";
    case kNewSession:
      return ".NewSession";
    case kStartup:
      return ".Startup";
    case kBlockedRequest:
      return ".BlockedRequest";
    case kCookieExpired:
      return ".CookieExpired";
    case kPreemptiveRefresh:
      return ".PreemptiveRefresh";
    case kRetryWithBackoff:
      return ".RetryWithBackoff";
    case kConnectionChanged:
      return ".ConnectionChanged";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/histograms.xml:BoundSessionRefreshCookieFetcherTrigger)

}  // namespace

BoundSessionRefreshCookieFetcherImpl::BoundSessionRefreshCookieFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    SessionBindingHelper& session_binding_helper,
    std::string_view session_id,
    const GURL& refresh_url,
    const GURL& cookie_url,
    bound_session_credentials::SessionOrigin session_origin,
    base::flat_set<std::string> cookie_names,
    bool is_off_the_record_profile,
    Trigger trigger,
    bound_session_credentials::RotationDebugInfo debug_info)
    : url_loader_factory_(std::move(url_loader_factory)),
      session_binding_helper_(session_binding_helper),
      session_id_(session_id),
      refresh_url_(refresh_url),
      session_origin_(session_origin),
      expected_cookie_domain_(cookie_url),
      expected_cookie_names_(std::move(cookie_names)),
      is_off_the_record_profile_(is_off_the_record_profile),
      trigger_(trigger),
      non_refreshed_cookie_names_(expected_cookie_names_),
      debug_info_(std::move(debug_info)) {
  CHECK(refresh_url.is_valid());
}

BoundSessionRefreshCookieFetcherImpl::~BoundSessionRefreshCookieFetcherImpl() =
    default;

void BoundSessionRefreshCookieFetcherImpl::Start(
    RefreshCookieCompleteCallback callback,
    std::optional<std::string> sec_session_challenge_response) {
  TRACE_EVENT("browser", "BoundSessionRefreshCookieFetcherImpl::Start",
              perfetto::Flow::FromPointer(this), "url",
              expected_cookie_domain_);
  CHECK(!callback_);
  CHECK(callback);
  callback_ = std::move(callback);

  // Only used for manual testing.
  if (std::optional<base::TimeDelta> cookie_rotation_delay =
          bound_session_credentials::GetCookieRotationDelayIfSetByCommandLine();
      cookie_rotation_delay.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BoundSessionRefreshCookieFetcherImpl::StartRefreshRequest,
            weak_ptr_factory_.GetWeakPtr(),
            std::move(sec_session_challenge_response)),
        *cookie_rotation_delay);
  } else {
    StartRefreshRequest(std::move(sec_session_challenge_response));
  }
}

bool BoundSessionRefreshCookieFetcherImpl::IsChallengeReceived() const {
  return assertion_requests_count_ > 0;
}

std::optional<std::string>
BoundSessionRefreshCookieFetcherImpl::TakeSecSessionChallengeResponseIfAny() {
  std::optional<std::string> response;
  std::swap(response, sec_session_challenge_response_);
  return response;
}

base::flat_set<std::string>
BoundSessionRefreshCookieFetcherImpl::GetNonRefreshedCookieNames() {
  return non_refreshed_cookie_names_;
}

BoundSessionRefreshCookieFetcherImpl::Trigger
BoundSessionRefreshCookieFetcherImpl::GetTrigger() const {
  return trigger_;
}

void BoundSessionRefreshCookieFetcherImpl::StartRefreshRequest(
    std::optional<std::string> sec_session_challenge_response) {
  sec_session_challenge_response_ = std::move(sec_session_challenge_response);
  TRACE_EVENT("browser",
              "BoundSessionRefreshCookieFetcherImpl::StartRefreshRequest",
              perfetto::Flow::FromPointer(this), "has_challenge",
              sec_session_challenge_response_.has_value());
  if (!cookie_refresh_duration_.has_value()) {
    cookie_refresh_duration_ = base::TimeTicks::Now();
  }

  // Used only for manual testing.
  if (std::optional<BoundSessionRefreshCookieFetcher::Result> result =
          bound_session_credentials::
              GetCookieRotationResultIfSetByCommandLine();
      result.has_value()) {
    CompleteRequestAndReportRefreshResult(*result);
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = refresh_url_;
  request->method = "GET";

  if (sec_session_challenge_response_) {
    request->headers.SetHeader(kRotationChallengeResponseHeader,
                               *sec_session_challenge_response_);
  }
  request->headers.SetHeader(kRotationDebugHeader,
                             UpdateDebugInfoAndSerializeToHeader(debug_info_));

  url::Origin origin = GaiaUrls::GetInstance()->gaia_origin();
  request->site_for_cookies = net::SiteForCookies::FromOrigin(origin);
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(origin);

  mojo::PendingRemote<network::mojom::CookieAccessObserver> remote;
  cookie_observers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  request->trusted_params->cookie_observer = std::move(remote);

  url_loader_ =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(request),
          is_off_the_record_profile_ ? variations::InIncognito::kYes
                                     : variations::InIncognito::kNo,
          kTrafficAnnotation);
  url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // TODO(b/273920907): Download the response body to support in refresh DBSC
  // instructions update.
  // `base::Unretained(this)` is safe as `this` owns `url_loader_`.
  url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&BoundSessionRefreshCookieFetcherImpl::OnURLLoaderComplete,
                     base::Unretained(this)));
}

void BoundSessionRefreshCookieFetcherImpl::OnURLLoaderComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());
  TRACE_EVENT("browser",
              "BoundSessionRefreshCookieFetcherImpl::OnURLLoaderComplete",
              perfetto::Flow::FromPointer(this), "net_error", net_error);

  base::UmaHistogramSparse(
      GetRotationHttpResultHistogramName(
          IsChallengeReceived(), sec_session_challenge_response_.has_value()),
      headers ? headers->response_code() : net_error);

  std::optional<std::string> challenge_header_value =
      GetChallengeIfBindingKeyAssertionRequired(headers);
  if (challenge_header_value) {
    sec_session_challenge_response_.reset();
    HandleBindingKeyAssertionRequired(*challenge_header_value);
    return;
  }

  cookie_refresh_completed_ = true;
  result_ = GetResultFromNetErrorAndHttpStatusCode(
      net_error,
      headers ? std::optional<int>(headers->response_code()) : std::nullopt);

  if (result_ == Result::kConnectionError) {
    base::UmaHistogramSparse(
        "Signin.BoundSessionCredentials.CookieRotationNetError", -net_error);
  }

  if (result_ == Result::kSuccess && !reported_cookies_notified_) {
    // Normally, a cookie update notification should be sent before the request
    // is complete. Add some leeway in the case mojo messages are delivered out
    // of order.
    const base::TimeDelta kResponseCookiesNotifiedMaxDelay =
        base::Milliseconds(100);
    // `base::Unretained` is safe as `this` owns
    // `reported_cookies_notified_timer_`.
    reported_cookies_notified_timer_.Start(
        FROM_HERE, kResponseCookiesNotifiedMaxDelay,
        base::BindOnce(
            &BoundSessionRefreshCookieFetcherImpl::ReportRefreshResult,
            base::Unretained(this)));
  } else {
    ReportRefreshResult();
  }
}

BoundSessionRefreshCookieFetcher::Result
BoundSessionRefreshCookieFetcherImpl::GetResultFromNetErrorAndHttpStatusCode(
    net::Error net_error,
    std::optional<int> response_code) {
  if ((net_error != net::OK &&
       net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE) ||
      !response_code) {
    return Result::kConnectionError;
  }

  if (response_code == net::HTTP_OK) {
    return Result::kSuccess;
  }

  if (response_code >= net::HTTP_INTERNAL_SERVER_ERROR) {
    // Server error 5xx.
    return Result::kServerTransientError;
  }

  if (response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
    // Treat proxy errors as connection errors. It makes sense to retry again
    // once the user responds to the authentication challenge.
    return Result::kConnectionError;
  }

  if (response_code >= net::HTTP_BAD_REQUEST) {
    // Server error 4xx.
    return Result::kServerPersistentError;
  }

  // Unexpected response code.
  return Result::kServerPersistentError;
}

void BoundSessionRefreshCookieFetcherImpl::ReportRefreshResult() {
  reported_cookies_notified_timer_.Stop();
  CHECK(cookie_refresh_completed_);
  if (result_ == Result::kSuccess && !non_refreshed_cookie_names_.empty()) {
    result_ = Result::kServerUnexepectedResponse;
  }
  TRACE_EVENT("browser",
              "BoundSessionRefreshCookieFetcherImpl::ReportRefreshResult",
              perfetto::TerminatingFlow::FromPointer(this), "result", result_);
  base::UmaHistogramEnumeration(kRotationResultHistogramName, result_);

  const std::string_view histogram_trigger_suffix =
      GetRotationHistogramTriggerSuffix(trigger_);
  base::UmaHistogramEnumeration(
      base::StrCat({kRotationResultHistogramName, histogram_trigger_suffix}),
      result_);
  if (const std::optional<std::string_view> histogram_session_origin_suffix =
          bound_session_credentials::GetSessionOriginHistogramSuffix(
              session_origin_);
      histogram_session_origin_suffix.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {kRotationResultHistogramName, *histogram_session_origin_suffix}),
        result_);
    base::UmaHistogramEnumeration(
        base::StrCat({kRotationResultHistogramName,
                      *histogram_session_origin_suffix,
                      histogram_trigger_suffix}),
        result_);
  }

  CHECK(cookie_refresh_duration_.has_value());
  base::TimeDelta duration = base::TimeTicks::Now() - *cookie_refresh_duration_;
  cookie_refresh_duration_.reset();
  base::UmaHistogramMediumTimes(kRotationTotalDurationHistogramName, duration);
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kRotationTotalDurationHistogramName, histogram_trigger_suffix}),
      duration);

  std::move(callback_).Run(result_);
}

std::optional<std::string>
BoundSessionRefreshCookieFetcherImpl::GetChallengeIfBindingKeyAssertionRequired(
    const scoped_refptr<net::HttpResponseHeaders>& headers) const {
  if (!headers || headers->response_code() != net::HTTP_UNAUTHORIZED ||
      !headers->HasHeader(kRotationChallengeHeader)) {
    return std::nullopt;
  }

  return headers->GetNormalizedHeader(kRotationChallengeHeader)
      .value_or(std::string());
}

void BoundSessionRefreshCookieFetcherImpl::HandleBindingKeyAssertionRequired(
    const std::string& challenge_header_value) {
  if (assertion_requests_count_ >= kMaxAssertionRequestsAllowed) {
    CompleteRequestAndReportRefreshResult(
        Result::kChallengeRequiredLimitExceeded);
    return;
  }

  ChallengeHeaderItems items = ParseChallengeHeader(challenge_header_value);

  if (items.challenge.empty() ||
      !base::IsStringUTF8AllowingNoncharacters(items.challenge)) {
    CompleteRequestAndReportRefreshResult(
        Result::kChallengeRequiredUnexpectedFormat);
    return;
  }

  if (items.session_id != session_id_) {
    CompleteRequestAndReportRefreshResult(
        Result::kChallengeRequiredSessionIdMismatch);
    return;
  }

  // Binding key assertion required.
  assertion_requests_count_++;
  RefreshWithChallenge(items.challenge);
}

// static
BoundSessionRefreshCookieFetcherImpl::ChallengeHeaderItems
BoundSessionRefreshCookieFetcherImpl::ParseChallengeHeader(
    const std::string& header) {
  base::StringPairs items;
  base::SplitStringIntoKeyValuePairs(header, '=', ';', &items);
  ChallengeHeaderItems result;
  for (const auto& [key, value] : items) {
    if (base::EqualsCaseInsensitiveASCII(key, kSessionIdItemKey)) {
      result.session_id = value;
    }

    if (base::EqualsCaseInsensitiveASCII(key, kChallengeItemKey)) {
      result.challenge = value;
    }
  }

  return result;
}

void BoundSessionRefreshCookieFetcherImpl::
    CompleteRequestAndReportRefreshResult(Result result) {
  cookie_refresh_completed_ = true;
  result_ = result;
  ReportRefreshResult();
}

void BoundSessionRefreshCookieFetcherImpl::RefreshWithChallenge(
    const std::string& challenge) {
  TRACE_EVENT("browser",
              "BoundSessionRefreshCookieFetcherImpl::RefreshWithChallenge",
              perfetto::Flow::FromPointer(this));
  session_binding_helper_->GenerateBindingKeyAssertion(
      challenge, refresh_url_,
      base::BindOnce(
          &BoundSessionRefreshCookieFetcherImpl::OnGenerateBindingKeyAssertion,
          weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer(), challenge));
}

void BoundSessionRefreshCookieFetcherImpl::OnGenerateBindingKeyAssertion(
    base::ElapsedTimer generate_assertion_timer,
    const std::string& challenge,
    base::expected<std::string, SessionBindingHelper::Error>
        assertion_or_error) {
  base::UmaHistogramMediumTimes(
      "Signin.BoundSessionCredentials.CookieRotationGenerateAssertionDuration",
      generate_assertion_timer.Elapsed());
  base::UmaHistogramEnumeration(
      "Signin.BoundSessionCredentials.CookieRotationGenerateAssertionResult",
      assertion_or_error.error_or(SessionBindingHelper::kNoErrorForMetrics));
  TRACE_EVENT(
      "browser",
      "BoundSessionRefreshCookieFetcherImpl::OnGenerateBindingKeyAssertion",
      perfetto::Flow::FromPointer(this), "error",
      assertion_or_error.error_or(SessionBindingHelper::kNoErrorForMetrics));

  if (!assertion_or_error.has_value()) {
    CompleteRequestAndReportRefreshResult(Result::kSignChallengeFailed);
    return;
  }

  StartRefreshRequest(std::move(assertion_or_error).value());
}

void BoundSessionRefreshCookieFetcherImpl::OnCookiesAccessed(
    std::vector<network::mojom::CookieAccessDetailsPtr> details_vector) {
  // TODO(b/296999000): record a trace event.
  for (const auto& cookie_details : details_vector) {
    if (cookie_details->type !=
        network::mojom::CookieAccessDetails::Type::kChange) {
      continue;
    }

    reported_cookies_notified_ = true;
    for (const std::string& expected_cookie_name : expected_cookie_names_) {
      auto it = std::ranges::find_if(
          cookie_details->cookie_list,
          [this, &expected_cookie_name](
              const network::mojom::CookieOrLineWithAccessResultPtr& cookie) {
            return IsExpectedCookie(expected_cookie_domain_,
                                    expected_cookie_name, cookie);
          });
      if (it != cookie_details->cookie_list.end()) {
        non_refreshed_cookie_names_.erase(expected_cookie_name);
      }
    }
  }

  if (cookie_refresh_completed_ && reported_cookies_notified_) {
    ReportRefreshResult();
  }
}

void BoundSessionRefreshCookieFetcherImpl::Clone(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> observer) {
  cookie_observers_.Add(this, std::move(observer));
}
