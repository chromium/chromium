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
#include "base/notreached.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"

namespace {
// The minimum prewarm timer interval. Used as:
// - The minimum delay when scheduling subsequent prewarm requests.
// - The fallback interval when a transient error is returned.
// - The fallback interval when the session manager is unavailable.
constexpr base::TimeDelta kMinPrewarmInterval = base::Seconds(60);

}  // namespace

DeviceBoundSessionPrewarmer::DeviceBoundSessionPrewarmer(
    GURL prewarm_url,
    SessionManagerProvider session_manager_provider)
    : prewarm_url_(std::move(prewarm_url)),
      session_manager_provider_(std::move(session_manager_provider)) {
  CHECK(prewarm_url_.is_valid());
  CHECK(prewarm_url_.SchemeIs(url::kHttpsScheme));
  CHECK(session_manager_provider_);
}

DeviceBoundSessionPrewarmer::~DeviceBoundSessionPrewarmer() {
  Stop();
}

void DeviceBoundSessionPrewarmer::Start(bool is_startup_prewarm) {
  is_startup_prewarm_ = is_startup_prewarm;

  Stop();

  // Start the pre-warmer immediately on the first call.
  // Subsequent calls will be scheduled based on the Mojo response.
  DoPrewarm();
}

void DeviceBoundSessionPrewarmer::Stop() {
  timer_.Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
  receiver_.reset();
}

void DeviceBoundSessionPrewarmer::EnsureObserverBound(
    network::mojom::DeviceBoundSessionManager* session_manager) {
  if (receiver_.is_bound()) {
    return;
  }

  session_manager->AddObserver(prewarm_url_,
                               receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(
      base::BindOnce(&DeviceBoundSessionPrewarmer::OnObserverDisconnected,
                     base::Unretained(this)));
}

void DeviceBoundSessionPrewarmer::OnObserverDisconnected() {
  receiver_.reset();
  // The network service disconnected (e.g. crash). Schedule DoPrewarm()
  // after `kMinPrewarmInterval` to re-establish the observer and refresh
  // session state.
  timer_.Start(FROM_HERE, kMinPrewarmInterval, this,
               &DeviceBoundSessionPrewarmer::DoPrewarm);
}

void DeviceBoundSessionPrewarmer::DoPrewarm() {
  timer_.Stop();

  if (network::mojom::DeviceBoundSessionManager* session_manager =
          session_manager_provider_.Run()) {
    EnsureObserverBound(session_manager);
    session_manager->PrewarmSessionsForUrl(
        prewarm_url_,
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

  if (std::ranges::any_of(results,
                          &DeviceBoundSessionPrewarmer::IsTransientError)) {
    // If a session failed to refresh due to a transient error, retry after a
    // short delay regardless of what `earliest_next_refresh_time` is.
    // `earliest_next_refresh_time` only reflects the next refresh time of
    // sessions that rotated successfully or didn't need refresh, so it could
    // be set far in the future even if another session failed.
    // TODO(crbug.com/544602741): Revisit whether earliest_next_refresh_time
    // should account for failed sessions.
    timer_.Start(FROM_HERE, kMinPrewarmInterval, this,
                 &DeviceBoundSessionPrewarmer::DoPrewarm);
    return;
  }

  if (!earliest_next_refresh_time) {
    // If there is no transient error and no next refresh time, we can stop
    // prewarming.
    return;
  }

  // If the next refresh time is in the past or shorter than the minimum
  // interval, schedule the next prewarm after `kMinPrewarmInterval` to avoid
  // infinite loops or excessive requests.
  base::TimeDelta delay = std::max(
      *earliest_next_refresh_time - base::Time::Now(), kMinPrewarmInterval);
  timer_.Start(FROM_HERE, delay, this, &DeviceBoundSessionPrewarmer::DoPrewarm);
}

// network::mojom::DeviceBoundSessionAccessObserver:
void DeviceBoundSessionPrewarmer::OnDeviceBoundSessionAccessed(
    const net::device_bound_sessions::SessionAccess& access) {
  if (access.access_type !=
      net::device_bound_sessions::SessionAccess::AccessType::kCreation) {
    return;
  }
  // TODO(crbug.com/544602741): Consider passing next refresh time in
  // SessionAccess to avoid triggering an immediate prewarm IPC solely to
  // discover `earliest_next_refresh_time`.
  DoPrewarm();
}

void DeviceBoundSessionPrewarmer::Clone(
    mojo::PendingReceiver<network::mojom::DeviceBoundSessionAccessObserver>
        observer) {
  // The `Clone` method is only called for observers that are part of network
  // requests, so it is not expected to be called here.
  NOTREACHED();
}
