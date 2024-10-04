// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_DEBUG_REPORT_FETCHER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_DEBUG_REPORT_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// Stripped down version of a refresh cookie fetcher that sends a request with a
// debug header and then ignores the response.
class BoundSessionRefreshCookieDebugReportFetcher
    : public BoundSessionRefreshCookieFetcher {
 public:
  BoundSessionRefreshCookieDebugReportFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view session_id,
      const GURL& refresh_url,
      bool is_off_the_record_profile,
      bound_session_credentials::RotationDebugInfo debug_info);
  ~BoundSessionRefreshCookieDebugReportFetcher() override;

  // BoundSessionRefreshCookieFetcher:
  void Start(
      RefreshCookieCompleteCallback callback,
      std::optional<std::string> sec_session_challenge_response) override;
  bool IsChallengeReceived() const override;
  std::optional<std::string> TakeSecSessionChallengeResponseIfAny() override;

 private:
  void OnURLLoaderComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string session_id_;
  const GURL refresh_url_;
  // Required to attach X-Client-Data header to cookie rotation request for
  // GWS-visible Finch experiment.
  const bool is_off_the_record_profile_;
  RefreshCookieCompleteCallback callback_;

  bound_session_credentials::RotationDebugInfo debug_info_;

  // Non-null after a fetch has started.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_DEBUG_REPORT_FETCHER_H_
