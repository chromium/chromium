// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/device_bound_session_prewarmer.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"

namespace {
// The minimum prewarm timer interval. Used as:
// - The minimum delay when scheduling subsequent prewarm requests.
// - The fallback interval when a transient error is returned.
// - The fallback interval when the session manager is unavailable.
constexpr base::TimeDelta kMinPrewarmInterval = base::Seconds(60);

// Outcome of a single session in a pre-warm, ordered from most to least
// successful.
enum class PrewarmOutcome {
  kNotYetNeeded,
  kSuccess,
  kFatalError,
  kUnreachable,
  kOtherTransientError,
};

PrewarmOutcome GetPrewarmOutcome(
    net::device_bound_sessions::RefreshResult result) {
  switch (result) {
    case net::device_bound_sessions::RefreshResult::kInScopeRefreshNotYetNeeded:
      return PrewarmOutcome::kNotYetNeeded;
    case net::device_bound_sessions::RefreshResult::kRefreshed:
    case net::device_bound_sessions::RefreshResult::kRefreshedAsWaiter:
      return PrewarmOutcome::kSuccess;
    case net::device_bound_sessions::RefreshResult::kFatalError:
      return PrewarmOutcome::kFatalError;
    case net::device_bound_sessions::RefreshResult::kUnreachable:
      return PrewarmOutcome::kUnreachable;
    // Not expected: pre-warms wait for the service to initialize. Folded into
    // `kOtherTransientError` rather than `NOTREACHED()` so that an unexpected
    // value from the network service doesn't crash the browser.
    case net::device_bound_sessions::RefreshResult::kInitializedService:
    case net::device_bound_sessions::RefreshResult::kServerError:
    case net::device_bound_sessions::RefreshResult::kSigningQuotaExceeded:
    case net::device_bound_sessions::RefreshResult::kTransientSigningError:
      return PrewarmOutcome::kOtherTransientError;
  }
  NOTREACHED();
}

// LINT.IfChange(PrewarmOutcome)
std::string_view GetPrewarmOutcomeSuffix(PrewarmOutcome outcome) {
  switch (outcome) {
    case PrewarmOutcome::kNotYetNeeded:
      return ".NotYetNeeded";
    case PrewarmOutcome::kSuccess:
      return ".Success";
    case PrewarmOutcome::kFatalError:
      return ".FatalError";
    case PrewarmOutcome::kUnreachable:
      return ".Unreachable";
    case PrewarmOutcome::kOtherTransientError:
      return ".OtherTransientError";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/histograms.xml:PrewarmOutcome)

// Returns the `Net.DeviceBoundSessions.Prewarm.Duration` and `.Trigger` variant
// suffix for `results`. A single pre-warm can return a mix of outcomes; it is
// recorded under the least successful one, so that a failure is never hidden
// by successes in the same pre-warm.
std::string_view GetResultsOutcomeSuffix(
    const std::vector<net::device_bound_sessions::RefreshResult>& results) {
  PrewarmOutcome outcome = PrewarmOutcome::kNotYetNeeded;
  for (net::device_bound_sessions::RefreshResult result : results) {
    outcome = std::max(outcome, GetPrewarmOutcome(result));
  }
  return GetPrewarmOutcomeSuffix(outcome);
}

// `CONNECTION_UNKNOWN` means connected with an undetermined type, so only
// `CONNECTION_NONE` counts as offline, matching
// `NetworkConnectionTracker::IsOffline()`.
bool IsOfflineConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
  return type == net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE;
}

}  // namespace

DeviceBoundSessionPrewarmer::DeviceBoundSessionPrewarmer(
    GURL prewarm_url,
    SessionManagerProvider session_manager_provider,
    network::NetworkConnectionTracker* network_connection_tracker)
    : prewarm_url_(std::move(prewarm_url)),
      session_manager_provider_(std::move(session_manager_provider)) {
  CHECK(prewarm_url_.is_valid());
  CHECK(prewarm_url_.SchemeIs(url::kHttpsScheme));
  CHECK(session_manager_provider_);
  CHECK(network_connection_tracker);

  network_connection_observer_.Observe(network_connection_tracker);
  is_offline_ = network_connection_tracker->IsOffline();
}

DeviceBoundSessionPrewarmer::~DeviceBoundSessionPrewarmer() {
  Stop();
}

void DeviceBoundSessionPrewarmer::Start(bool is_startup_prewarm) {
  is_startup_prewarm_ = is_startup_prewarm;

  Stop();

  // Start the pre-warmer immediately on the first call.
  // Subsequent calls will be scheduled based on the Mojo response.
  DoPrewarm(PrewarmTrigger::kStart);
}

void DeviceBoundSessionPrewarmer::Stop() {
  timer_.Stop();
  retry_on_reconnect_ = false;
  retry_on_unreachable_ = false;
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
  SchedulePrewarm(kMinPrewarmInterval,
                  PrewarmTrigger::kNetworkServiceDisconnected);
}

void DeviceBoundSessionPrewarmer::DoPrewarm(PrewarmTrigger trigger) {
  timer_.Stop();

  if (network::mojom::DeviceBoundSessionManager* session_manager =
          session_manager_provider_.Run()) {
    retry_on_reconnect_ = false;
    retry_on_unreachable_ = false;
    EnsureObserverBound(session_manager);
    session_manager->PrewarmSessionsForUrl(
        prewarm_url_,
        base::BindOnce(&DeviceBoundSessionPrewarmer::OnPrewarmComplete,
                       weak_ptr_factory_.GetWeakPtr(), trigger,
                       base::ElapsedTimer()));
  } else {
    SchedulePrewarm(kMinPrewarmInterval,
                    PrewarmTrigger::kNetworkServiceUnavailableRetry);
  }
}

void DeviceBoundSessionPrewarmer::SchedulePrewarm(base::TimeDelta delay,
                                                  PrewarmTrigger trigger) {
  // Unretained is safe because `timer_` is owned by `this` and cancels the
  // task when destroyed.
  timer_.Start(FROM_HERE, base::Time::Now() + delay,
               base::BindOnce(&DeviceBoundSessionPrewarmer::DoPrewarm,
                              base::Unretained(this), trigger));
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
    PrewarmTrigger trigger,
    base::ElapsedTimer prewarm_timer,
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

  if (!results.empty()) {
    const std::string_view outcome_suffix = GetResultsOutcomeSuffix(results);
    base::UmaHistogramEnumeration("Net.DeviceBoundSessions.Prewarm.Trigger",
                                  trigger);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"Net.DeviceBoundSessions.Prewarm.Trigger", outcome_suffix}),
        trigger);
    // Use `MediumTimes` because some refreshes take more than 10 seconds.
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"Net.DeviceBoundSessions.Prewarm.Duration", outcome_suffix}),
        prewarm_timer.Elapsed());
  }

  const bool unreachable = std::ranges::contains(
      results, net::device_bound_sessions::RefreshResult::kUnreachable);
  const bool reconnected = std::exchange(retry_on_unreachable_, false);
  if (unreachable && reconnected) {
    DoPrewarm(PrewarmTrigger::kReconnectDuringPrewarm);
    return;
  }
  retry_on_reconnect_ = unreachable;

  if (std::ranges::any_of(results,
                          &DeviceBoundSessionPrewarmer::IsTransientError)) {
    // If a session failed to refresh due to a transient error, retry after a
    // short delay regardless of what `earliest_next_refresh_time` is.
    // `earliest_next_refresh_time` only reflects the next refresh time of
    // sessions that rotated successfully or didn't need refresh, so it could
    // be set far in the future even if another session failed.
    // TODO(crbug.com/544602741): Revisit whether earliest_next_refresh_time
    // should account for failed sessions.
    SchedulePrewarm(kMinPrewarmInterval, PrewarmTrigger::kTransientErrorRetry);
    return;
  }

  if (!earliest_next_refresh_time) {
    // If there is no transient error and no next refresh time, we can stop
    // prewarming.
    return;
  }

  const base::Time now = base::Time::Now();
  const base::TimeDelta next_refresh_delay = *earliest_next_refresh_time - now;
  // Recorded before clamping to `kMinPrewarmInterval`, so that refreshes that
  // are already due remain visible.
  // TODO(crbug.com/566073494): Also record the time since the last successful
  // pre-warm, which differs from this because of sleep and errors.
  base::UmaHistogramCustomTimes(
      "Net.DeviceBoundSessions.Prewarm.NextRefreshDelay", next_refresh_delay,
      base::Milliseconds(1), base::Minutes(150), 100);

  // If the next refresh time is in the past or shorter than the minimum
  // interval, schedule the next prewarm after `kMinPrewarmInterval` to avoid
  // infinite loops or excessive requests.
  base::TimeDelta delay = std::max(next_refresh_delay, kMinPrewarmInterval);
  SchedulePrewarm(delay, PrewarmTrigger::kScheduledRefresh);
}

void DeviceBoundSessionPrewarmer::OnConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  const bool was_offline = is_offline_;
  is_offline_ = IsOfflineConnectionType(type);

  // Network switches arrive as `CONNECTION_NONE` then the new type, so they
  // count as regaining connectivity too.
  if (!was_offline || is_offline_) {
    return;
  }
  if (retry_on_reconnect_) {
    DoPrewarm(PrewarmTrigger::kReconnect);
  } else {
    // Harmless when idle: cleared when the next pre-warm is sent.
    retry_on_unreachable_ = true;
  }
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
  // TODO(crbug.com/566983318): Avoid overlapping a pre-warm in flight.
  DoPrewarm(PrewarmTrigger::kSessionCreated);
}

void DeviceBoundSessionPrewarmer::Clone(
    mojo::PendingReceiver<network::mojom::DeviceBoundSessionAccessObserver>
        observer) {
  // The `Clone` method is only called for observers that are part of network
  // requests, so it is not expected to be called here.
  NOTREACHED();
}
