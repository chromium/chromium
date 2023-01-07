// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/util.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

// The retry values set here account for 7 retried requests over a period of
// approximately 25.5 seconds. Requests are sent at:
// - 0s
// - 0.5s
// - 1s
// - 2s
// - 4s
// - 8s
// - 10s
constexpr int kMaxRetryCount = 7;

constexpr net::BackoffEntry::Policy kBackoffPolicy{
    .num_errors_to_ignore = 0,
    .initial_delay_ms = 500,
    .multiply_factor = 2.0,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = 10 * 1000,  // 10 seconds.
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false};

}  // namespace

WinKeyNetworkDelegate::WinKeyNetworkDelegate(
    std::unique_ptr<WinNetworkFetcherFactory> factory)
    : win_network_fetcher_factory_(std::move(factory)),
      backoff_entry_(&kBackoffPolicy) {
  DCHECK(win_network_fetcher_factory_);
}

WinKeyNetworkDelegate::WinKeyNetworkDelegate()
    : win_network_fetcher_factory_(WinNetworkFetcherFactory::Create()),
      backoff_entry_(&kBackoffPolicy) {
  DCHECK(win_network_fetcher_factory_);
}

WinKeyNetworkDelegate::~WinKeyNetworkDelegate() = default;

void WinKeyNetworkDelegate::SendPublicKeyToDmServer(
    const GURL& url,
    const std::string& dm_token,
    const std::string& body,
    UploadKeyCompletedCallback upload_key_completed_callback) {
  // Parallel requests are not supported.
  DCHECK(!win_network_fetcher_);

  base::flat_map<std::string, std::string> headers;
  headers.emplace("Authorization", "GoogleDMToken token=" + dm_token);
  win_network_fetcher_ =
      win_network_fetcher_factory_->CreateNetworkFetcher(url, body, headers);
  UploadKey(std::move(upload_key_completed_callback));
}

void WinKeyNetworkDelegate::UploadKey(
    UploadKeyCompletedCallback upload_key_completed_callback) {
  DCHECK(win_network_fetcher_);
  win_network_fetcher_->Fetch(base::BindOnce(
      &WinKeyNetworkDelegate::FetchCompleted, weak_factory_.GetWeakPtr(),
      std::move(upload_key_completed_callback)));
}

void WinKeyNetworkDelegate::FetchCompleted(
    UploadKeyCompletedCallback upload_key_completed_callback,
    HttpResponseCode response_code) {
  if (ParseUploadKeyStatus(response_code) ==
          UploadKeyStatus::kFailedRetryable &&
      backoff_entry_.failure_count() != kMaxRetryCount) {
    backoff_entry_.InformOfRequest(/*succeeded=*/false);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WinKeyNetworkDelegate::UploadKey,
                       weak_factory_.GetWeakPtr(),
                       std::move(upload_key_completed_callback)),
        backoff_entry_.GetTimeUntilRelease());
    return;
  }
  // Recording the retry count.
  base::UmaHistogramCustomCounts(
      "Enterprise.DeviceTrust.RotateSigningKey.Tries",
      backoff_entry_.failure_count(), 1, kMaxRetryCount, kMaxRetryCount + 1);

  backoff_entry_.Reset();
  win_network_fetcher_.reset();

  std::move(upload_key_completed_callback).Run(response_code);
}

}  // namespace enterprise_connectors
