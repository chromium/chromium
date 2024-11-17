// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_debug_report_fetcher.h"

#include <optional>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/typed_macros.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
std::string UpdateDebugInfoAndSerializeToHeader(
    bound_session_credentials::RotationDebugInfo& debug_info) {
  *debug_info.mutable_request_time() =
      bound_session_credentials::TimeToTimestamp(base::Time::Now());
  std::string serialized = debug_info.SerializeAsString();
  return base::Base64Encode(serialized);
}
}  // namespace

BoundSessionRefreshCookieDebugReportFetcher::
    BoundSessionRefreshCookieDebugReportFetcher(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::string_view session_id,
        const GURL& refresh_url,
        bool is_off_the_record_profile,
        bound_session_credentials::RotationDebugInfo debug_info)
    : url_loader_factory_(std::move(url_loader_factory)),
      session_id_(session_id),
      refresh_url_(refresh_url),
      is_off_the_record_profile_(is_off_the_record_profile),
      debug_info_(std::move(debug_info)) {}

BoundSessionRefreshCookieDebugReportFetcher::
    ~BoundSessionRefreshCookieDebugReportFetcher() = default;

void BoundSessionRefreshCookieDebugReportFetcher::Start(
    RefreshCookieCompleteCallback callback,
    std::optional<std::string> sec_session_challenge_response) {
  TRACE_EVENT("browser", "BoundSessionRefreshCookieDebugReportFetcher::Start",
              perfetto::Flow::FromPointer(this));
  CHECK(!callback_);
  CHECK(callback);
  callback_ = std::move(callback);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = refresh_url_;
  request->method = "GET";

  if (sec_session_challenge_response) {
    request->headers.SetHeader(kRotationChallengeResponseHeader,
                               *sec_session_challenge_response);
  }
  request->headers.SetHeader(kRotationDebugHeader,
                             UpdateDebugInfoAndSerializeToHeader(debug_info_));

  url::Origin origin = GaiaUrls::GetInstance()->gaia_origin();
  request->site_for_cookies = net::SiteForCookies::FromOrigin(origin);
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(origin);

  url_loader_ =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(request),
          is_off_the_record_profile_ ? variations::InIncognito::kYes
                                     : variations::InIncognito::kNo,
          kTrafficAnnotation);
  url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // `base::Unretained(this)` is safe as `this` owns `url_loader_`.
  url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(
          &BoundSessionRefreshCookieDebugReportFetcher::OnURLLoaderComplete,
          base::Unretained(this)));
}

bool BoundSessionRefreshCookieDebugReportFetcher::IsChallengeReceived() const {
  return false;
}

std::optional<std::string> BoundSessionRefreshCookieDebugReportFetcher::
    TakeSecSessionChallengeResponseIfAny() {
  return std::nullopt;
}

void BoundSessionRefreshCookieDebugReportFetcher::OnURLLoaderComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());
  TRACE_EVENT(
      "browser",
      "BoundSessionRefreshCookieDebugReportFetcher::OnURLLoaderComplete",
      perfetto::Flow::FromPointer(this), "net_error", net_error);
  std::move(callback_).Run(Result::kSuccess);
}
