// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/util.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr net::BackoffEntry::Policy kBackoffPolicy{
    .num_errors_to_ignore = 0,
    .initial_delay_ms = 1000,
    .multiply_factor = 2.0,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = 5 * 60 * 1000,  // 5 min.
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false};

constexpr int kMaxRetryCount = 10;

}  // namespace

WinKeyNetworkDelegate::WinKeyNetworkDelegate()
    : backoff_entry_(&kBackoffPolicy) {}

WinKeyNetworkDelegate::~WinKeyNetworkDelegate() = default;

void WinKeyNetworkDelegate::SendPublicKeyToDmServer(
    const GURL& url,
    const std::string& dm_token,
    const std::string& body,
    UploadKeyCompletedCallback upload_key_completed_callback) {
  // Parallel requests are not supported.
  DCHECK_EQ(backoff_entry_.failure_count(), 0);
  base::flat_map<std::string, std::string> headers;
  headers.emplace("Authorization", "GoogleDMToken token=" + dm_token);
  win_network_fetcher_ = WinNetworkFetcher::Create(url, body, headers);
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
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
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
  backoff_entry_.InformOfRequest(/*succeeded=*/true);
  std::move(upload_key_completed_callback).Run(response_code);
}

}  // namespace enterprise_connectors
