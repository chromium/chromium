// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/device_bound_session_prewarmer.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"

namespace {
// The minimum prewarm timer interval. Used as:
// - The minimum delay when scheduling subsequent prewarm requests.
// - The fallback interval when a transient error is returned.
// - The fallback interval when the session manager is unavailable.
constexpr base::TimeDelta kMinPrewarmInterval = base::Seconds(60);

// The prewarm timer interval when the URL provider callback returns an empty or
// invalid URL.
constexpr base::TimeDelta kLongPrewarmInterval = base::Minutes(10);

// The maximum number of times to retry prewarming with an invalid URL.
constexpr int kMaxInvalidUrlRetries = 5;

}  // namespace

DeviceBoundSessionPrewarmer::DeviceBoundSessionPrewarmer(
    SessionManagerProvider session_manager_provider)
    : session_manager_provider_(std::move(session_manager_provider)) {
  CHECK(session_manager_provider_);
}

DeviceBoundSessionPrewarmer::~DeviceBoundSessionPrewarmer() {
  Stop();
}

void DeviceBoundSessionPrewarmer::Start(
    PrewarmUrlProvider url_provider_callback,
    bool is_startup_prewarm) {
  CHECK(url_provider_callback);
  url_provider_callback_ = std::move(url_provider_callback);
  invalid_url_consecutive_retries_ = 0;
  is_startup_prewarm_ = is_startup_prewarm;

  Stop();

  // Start the pre-warmer immediately on the first call.
  // Subsequent calls will be scheduled based on the Mojo response.
  DoPrewarm();
}

void DeviceBoundSessionPrewarmer::Stop() {
  timer_.Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void DeviceBoundSessionPrewarmer::DoPrewarm() {
  // If the URL provider callback returns an empty or invalid URL, we should
  // skip the prewarming entirely and schedule the next prewarm at a long
  // interval up to a maximum number of consecutive failures.
  GURL target_url = url_provider_callback_.Run();
  const bool invalid_url =
      !target_url.is_valid() || !target_url.SchemeIs(url::kHttpsScheme);
  if (invalid_url) {
    invalid_url_consecutive_retries_++;
    base::UmaHistogramCounts100(
        "Net.DeviceBoundSessions.PrewarmInvalidUrlConsecutiveFailures",
        invalid_url_consecutive_retries_);
    if (invalid_url_consecutive_retries_ <= kMaxInvalidUrlRetries) {
      timer_.Start(FROM_HERE, kLongPrewarmInterval, this,
                   &DeviceBoundSessionPrewarmer::DoPrewarm);
    }
    return;
  }

  // Reset the retry count if the URL is valid.
  invalid_url_consecutive_retries_ = 0;

  if (network::mojom::DeviceBoundSessionManager* session_manager =
          session_manager_provider_.Run()) {
    session_manager->PrewarmSessionsForUrl(
        target_url,
        base::BindOnce(&DeviceBoundSessionPrewarmer::OnPrewarmComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    timer_.Start(FROM_HERE, kMinPrewarmInterval, this,
                 &DeviceBoundSessionPrewarmer::DoPrewarm);
  }
}

bool DeviceBoundSessionPrewarmer::IsTransientError(
    net::device_bound_sessions::RefreshResult result) {
  switch (result) {
    case net::device_bound_sessions::RefreshResult::kRefreshed:
    case net::device_bound_sessions::RefreshResult::kRefreshedAsWaiter:
    case net::device_bound_sessions::RefreshResult::kInScopeRefreshNotYetNeeded:
    case net::device_bound_sessions::RefreshResult::kInitializedService:
    case net::device_bound_sessions::RefreshResult::kFatalError:
      return false;
    case net::device_bound_sessions::RefreshResult::kUnreachable:
    case net::device_bound_sessions::RefreshResult::kServerError:
    case net::device_bound_sessions::RefreshResult::kTransientSigningError:
    case net::device_bound_sessions::RefreshResult::kSigningQuotaExceeded:
      return true;
  }
  NOTREACHED();
}

void DeviceBoundSessionPrewarmer::OnPrewarmComplete(
    const std::vector<net::device_bound_sessions::RefreshResult>& results,
    std::optional<base::Time> earliest_next_refresh_time) {
  bool is_startup = std::exchange(is_startup_prewarm_, false);
  for (const auto& result : results) {
    if (is_startup) {
      base::UmaHistogramEnumeration(
          "Net.DeviceBoundSessions.PrewarmResult.Startup", result);
    } else {
      base::UmaHistogramEnumeration(
          "Net.DeviceBoundSessions.PrewarmResult.Scheduled", result);
    }
  }

  if (!earliest_next_refresh_time) {
    if (std::ranges::none_of(results,
                             &DeviceBoundSessionPrewarmer::IsTransientError)) {
      // If there is no transient error and no next refresh time, we can stop
      // prewarming.
      return;
    }

    timer_.Start(FROM_HERE, kMinPrewarmInterval, this,
                 &DeviceBoundSessionPrewarmer::DoPrewarm);
    return;
  }

  // If the next refresh time is in the past or shorter than the minimum
  // interval, schedule the next prewarm after `kMinPrewarmInterval` to avoid
  // infinite loops or excessive requests.
  base::TimeDelta delay = std::max(
      *earliest_next_refresh_time - base::Time::Now(), kMinPrewarmInterval);
  timer_.Start(FROM_HERE, delay, this, &DeviceBoundSessionPrewarmer::DoPrewarm);
}
