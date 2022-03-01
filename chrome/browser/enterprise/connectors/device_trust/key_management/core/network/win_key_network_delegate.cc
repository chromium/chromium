// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "components/winhttp/network_fetcher.h"
#include "components/winhttp/scoped_hinternet.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr int kMaxRetryCount = 10;

void UploadKey(base::OnceCallback<void(int)> callback,
               const base::flat_map<std::string, std::string>& headers,
               const GURL& url,
               const std::string& body) {
  // TODO(b/202321214): need to pass in winhttp::ProxyInfo somehow.
  // If specified use it to create an winhttp::ProxyConfiguration instance.
  // Otherwise create an winhttp::AutoProxyConfiguration instance.
  auto proxy_config = base::MakeRefCounted<winhttp::ProxyConfiguration>();
  auto session = winhttp::CreateSessionHandle(L"DeviceTrustKeyManagement",
                                              proxy_config->access_type());
  auto fetcher = base::MakeRefCounted<winhttp::NetworkFetcher>(session.get(),
                                                               proxy_config);

  fetcher->PostRequest(url, body, std::string(), headers,
                       /*fetch_started_callback=*/base::DoNothing(),
                       /*fetch_progress_callback=*/base::DoNothing(),
                       /*fetch_completed_callback=*/std::move(callback));
}

}  // namespace

WinKeyNetworkDelegate::WinKeyNetworkDelegate()
    : upload_callback_(base::BindRepeating(&UploadKey)) {}

WinKeyNetworkDelegate::WinKeyNetworkDelegate(UploadKeyCallback upload_callback,
                                             bool sleep_during_backoff)
    : upload_callback_(upload_callback),
      sleep_during_backoff_(sleep_during_backoff) {
  DCHECK(upload_callback_);
}

WinKeyNetworkDelegate::~WinKeyNetworkDelegate() = default;

KeyNetworkDelegate::HttpResponseCode
WinKeyNetworkDelegate::SendPublicKeyToDmServerSync(const GURL& url,
                                                   const std::string& dm_token,
                                                   const std::string& body) {
  base::flat_map<std::string, std::string> headers;
  headers.emplace("Authorization", "GoogleDMToken token=" + dm_token);

  constexpr net::BackoffEntry::Policy kBackoffPolicy{
      .num_errors_to_ignore = 0,
      .initial_delay_ms = 1000,
      .multiply_factor = 2.0,
      .jitter_factor = 0.1,
      .maximum_backoff_ms = 5 * 60 * 1000,  // 5 min.
      .entry_lifetime_ms = -1,
      .always_use_initial_delay = false};

  net::BackoffEntry boe(&kBackoffPolicy);
  int try_count = 0;
  for (; boe.failure_count() < kMaxRetryCount; ++try_count) {
    // Wait before trying to send again, if needed. This will not block on
    // the first request.
    if (sleep_during_backoff_ && boe.ShouldRejectRequest())
      base::PlatformThread::Sleep(boe.GetTimeUntilRelease());

    // Cleaning up the state for the upload key call.
    response_code_.reset();

    // Starting the upload request.
    base::RunLoop run_loop;
    auto completion_callback =
        base::BindOnce(&WinKeyNetworkDelegate::FetchCompleted,
                       weak_factory_.GetWeakPtr())
            .Then(run_loop.QuitClosure());
    upload_callback_.Run(std::move(completion_callback), headers, url, body);
    run_loop.Run();

    auto response_code = response_code_.value_or(0);
    int status_leading_digit = response_code / 100;

    // 2xx is a success and 4xx are non retriable errors.
    if (status_leading_digit == 2 || status_leading_digit == 4)
      break;

    // Received retryable error.
    boe.InformOfRequest(/*succeeded=*/false);
  }

  // Recording the retry count.
  base::UmaHistogramCustomCounts(
      "Enterprise.DeviceTrust.RotateSigningKey.Tries", try_count, 1,
      kMaxRetryCount, kMaxRetryCount + 1);

  return response_code_.value_or(0);
}

void WinKeyNetworkDelegate::FetchCompleted(int response_code) {
  response_code_ = response_code;
}

}  // namespace enterprise_connectors
