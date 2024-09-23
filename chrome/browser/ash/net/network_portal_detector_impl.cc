// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_portal_detector_impl.h"

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

using ::captive_portal::CaptivePortalDetector;

// Default delay between portal detection attempts when Chrome portal detection
// is used.
constexpr base::TimeDelta kDefaultAttemptDelay = base::Seconds(1);

// Timeout for attempts.
constexpr base::TimeDelta kAttemptTimeout = base::Seconds(10);

const NetworkState* DefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

  // traffic annotation tag.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
  net::DefineNetworkTrafficAnnotation("network_portal_detector", R"(
    semantics {
      sender: "Network Portal Detector"
      description:
        "Checks if the system is behind a captive portal. To do so, makes "
        "an unlogged, dataless connection to a Google server and checks "
        "the response."
      trigger:
        "Portal detection by the OS is initiated when a new WiFi service "
        "is connected to in order to determine whether the network has "
        "internet access or is behind a captive portal."
      data: "None."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "cros-network-health-team@google.com"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-01-13"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature cannot be disabled by settings."
      policy_exception_justification:
        "This feature is required to deliver core user experiences and "
        "cannot be disabled by policy."
    })");

void SetNetworkPortalState(const NetworkState* network,
                           NetworkState::PortalState portal_state) {
  NetworkHandler::Get()->network_state_handler()->SetNetworkChromePortalState(
      network->path(), portal_state);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl, public:

NetworkPortalDetectorImpl::NetworkPortalDetectorImpl(
    network::mojom::URLLoaderFactory* loader_factory_for_testing)
    : attempt_timeout_(kAttemptTimeout) {
  NET_LOG(EVENT) << "NetworkPortalDetectorImpl::NetworkPortalDetectorImpl()";
  network::mojom::URLLoaderFactory* loader_factory;
  if (loader_factory_for_testing) {
    loader_factory = loader_factory_for_testing;
  } else {
    shared_url_loader_factory_ =
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory();
    loader_factory = shared_url_loader_factory_.get();
  }
  captive_portal_detector_ =
      std::make_unique<CaptivePortalDetector>(loader_factory);

  network_state_handler_observer_.Observe(
      NetworkHandler::Get()->network_state_handler());
}

NetworkPortalDetectorImpl::~NetworkPortalDetectorImpl() {
  NET_LOG(EVENT) << "NetworkPortalDetectorImpl::~NetworkPortalDetectorImpl()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  attempt_task_.Cancel();
  attempt_timeout_task_.Cancel();

  captive_portal_detector_->Cancel();
  captive_portal_detector_.reset();
}

bool NetworkPortalDetectorImpl::IsEnabled() {
  return enabled_;
}

void NetworkPortalDetectorImpl::Enable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enabled_)
    return;

  NET_LOG(EVENT) << "NetworkPortalDetector Enabled.";
  DCHECK(is_idle());
  enabled_ = true;

  const NetworkState* network = DefaultNetwork();
  if (!network)
    return;
  SetNetworkPortalState(network, NetworkState::PortalState::kUnknown);
}

void NetworkPortalDetectorImpl::PortalStateChanged(
    const NetworkState* default_network,
    NetworkState::PortalState portal_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!default_network || !default_network->IsConnectedState()) {
    NET_LOG(EVENT) << "No connected default network: "
                   << NetworkId(default_network)
                   << ", stopping portal detection.";
    if ((!default_network && !default_network_id_.empty()) ||
        (default_network && default_network->guid() != default_network_id_)) {
      default_network_id_ = std::string();
      StopDetection();
    }
    return;
  }

  default_network_id_ = default_network->guid();

  // If a proxy is configured and it is not "direct", then the
  // |default_network| has a proxy. By default, managed networks have a
  // "direct" proxy. A "direct" proxy is a direct connection to the
  // network, so the proxy preferences are ignored.
  bool has_proxy = false;
  if (default_network->proxy_config().has_value()) {
    ProxyConfigDictionary dict(default_network->proxy_config()->Clone());
    ProxyPrefs::ProxyMode mode;
    if (dict.GetMode(&mode)) {
      has_proxy = mode != ProxyPrefs::MODE_DIRECT;
    }
  }

  NET_LOG(EVENT) << "PortalStateChanged, id="
                 << NetworkGuidId(default_network_id_)
                 << " state=" << default_network->connection_state()
                 << " portal_state=" << portal_state
                 << " has_proxy=" << has_proxy;

  bool schedule_attempt = false;
  switch (portal_state) {
    case NetworkState::PortalState::kUnknown:
      // Not expected. Shill detection failed.
      NET_LOG(ERROR) << "Unknown PortalState";
      break;
    case NetworkState::PortalState::kOnline:
      // If a proxy is configured, use captive_portal_detector_ to detect a
      // portal.
      if (has_proxy) {
        schedule_attempt = true;
      }
      break;
    case NetworkState::PortalState::kPortalSuspected:
      break;
    case NetworkState::PortalState::kPortal:
      break;
    case NetworkState::PortalState::kNoInternet:
      break;
  }

  if (schedule_attempt) {
    ScheduleAttempt();
  } else if (!is_idle()) {
    StopDetection();
  }
}

void NetworkPortalDetectorImpl::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl, private:

void NetworkPortalDetectorImpl::StopDetection() {
  if (is_idle()) {
    NET_LOG(EVENT) << "StopDetection(): Attempt not running";
    return;
  }
  NET_LOG(EVENT) << "StopDetection";
  attempt_task_.Cancel();
  attempt_timeout_task_.Cancel();
  captive_portal_detector_->Cancel();
  state_ = STATE_IDLE;
  ResetCountersAndSendMetrics();
}

void NetworkPortalDetectorImpl::ScheduleAttempt(const base::TimeDelta& delay) {
  if (!IsEnabled())
    return;

  if (!is_idle()) {
    NET_LOG(EVENT) << "ScheduleAttempt(): Attempt already running, restarting.";
    if (state_ == STATE_CHECKING_FOR_PORTAL) {
      // When a new attempt is scheduled, cancel any pending attempt to avoid
      // a DCHECK in CaptivePortalDetector when an attempt is started before
      // the current attempt completes. See b/327072851 for details.
      captive_portal_detector_->Cancel();
    }
  }

  attempt_task_.Cancel();
  attempt_timeout_task_.Cancel();
  state_ = STATE_PORTAL_CHECK_PENDING;

  if (attempt_delay_for_testing_) {
    next_attempt_delay_ = *attempt_delay_for_testing_;
  } else if (!delay.is_zero()) {
    next_attempt_delay_ = delay;
  } else if (captive_portal_detector_run_count_ == 0) {
    // No delay for first attempt.
    next_attempt_delay_ = base::TimeDelta();
  } else {
    next_attempt_delay_ = kDefaultAttemptDelay;
  }
  attempt_task_.Reset(base::BindOnce(&NetworkPortalDetectorImpl::StartAttempt,
                                     weak_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, attempt_task_.callback(), next_attempt_delay_);
}

void NetworkPortalDetectorImpl::StartAttempt() {
  DCHECK(is_portal_check_pending());

  state_ = STATE_CHECKING_FOR_PORTAL;

  const NetworkState* default_network = DefaultNetwork();
  if (!default_network) {
    NET_LOG(EVENT) << "Start attempt called with no default network, aborting.";
    return;
  }

  GURL url = default_network->probe_url();
  if (url.is_empty())
    url = GURL(captive_portal::CaptivePortalDetector::kDefaultURL);
  NET_LOG(EVENT) << "Starting captive portal detection for: "
                 << NetworkId(default_network) << " Probe url: " << url;
  captive_portal_detector_->DetectCaptivePortal(
      url,
      base::BindOnce(&NetworkPortalDetectorImpl::OnAttemptCompleted,
                     weak_factory_.GetWeakPtr()),
      kTrafficAnnotation);
  attempt_timeout_task_.Reset(
      base::BindOnce(&NetworkPortalDetectorImpl::OnAttemptTimeout,
                     weak_factory_.GetWeakPtr()));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, attempt_timeout_task_.callback(), attempt_timeout_);
}

void NetworkPortalDetectorImpl::OnAttemptTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_checking_for_portal());

  NET_LOG(EVENT) << "Portal detection timeout: "
                 << " id=" << NetworkGuidId(default_network_id_);

  captive_portal_detector_->Cancel();
  CaptivePortalDetector::Results results;
  results.result = captive_portal::RESULT_NO_RESPONSE;
  OnAttemptCompleted(results);
}

void NetworkPortalDetectorImpl::OnAttemptCompleted(
    const CaptivePortalDetector::Results& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_checking_for_portal());

  captive_portal::CaptivePortalResult result = results.result;
  int response_code = results.response_code;

  const NetworkState* network = DefaultNetwork();

  state_ = STATE_IDLE;
  attempt_timeout_task_.Cancel();

  bool detection_completed = false;

  // |portal_state| defaults to kUnknown which will cause the Chrome portal
  // state to be ignored in favor of the Shill portal state.
  // See NetworkState::GetPortalState for details.
  NetworkState::PortalState portal_state = NetworkState::PortalState::kUnknown;

  switch (result) {
    case captive_portal::RESULT_NO_RESPONSE:
      // Do not override shill results.
      if (response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
        detection_completed = true;
      }
      break;
    case captive_portal::RESULT_INTERNET_CONNECTED:
      // Set the portal state to kOnline for metrics reporting. This will not
      // override the shill result.
      portal_state = NetworkState::PortalState::kOnline;
      detection_completed = true;
      break;
    case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
      // Override shill results with kPortal.
      // TODO(b/292141089): This should only happen when a proxy is configured
      // and Shill is unable to perform accurate portal detection.
      portal_state = NetworkState::PortalState::kPortal;
      break;
    case captive_portal::RESULT_COUNT:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  NET_LOG(EVENT) << "NetworkPortalDetector: AttemptCompleted: id="
                 << NetworkGuidId(default_network_id_) << ", result="
                 << captive_portal::CaptivePortalResultToString(result)
                 << ", response_code=" << response_code
                 << ", content_length=" << results.content_length.value_or(-1);

  captive_portal_detector_run_count_++;

  if (detection_completed) {
    // Chrome positively identified an online or proxy-auth (407) response.
    // No need to continue detection.
    response_code_for_testing_ = response_code;
    DetectionCompleted(network, portal_state);
    return;
  }

  MaybeReportMetrics(network, portal_state, /*detection_completed=*/false);

  if (!is_idle()) {
    return;
  }

  // Set network portal state and continue scheduling attempts until online.
  if (portal_state == NetworkState::PortalState::kPortal) {
    response_code_for_testing_ = response_code;
    SetNetworkPortalState(network, portal_state);
  }
  ScheduleAttempt(results.retry_after_delta);
}

void NetworkPortalDetectorImpl::MaybeReportMetrics(
    const NetworkState* network,
    NetworkState::PortalState portal_state,
    bool detection_completed) {
  if (metrics_reported_) {
    return;
  }
  if (!detection_completed &&
      portal_state == NetworkState::PortalState::kUnknown &&
      captive_portal_detector_run_count_ < 10) {
    return;
  }
  base::UmaHistogramEnumeration("Network.NetworkPortalDetectorState",
                                portal_state);
  if (network && portal_state == NetworkState::PortalState::kPortal) {
    base::UmaHistogramEnumeration("Network.NetworkPortalDetectorType",
                                  network->GetNetworkTechnologyType());
  }
  metrics_reported_ = true;
}

void NetworkPortalDetectorImpl::DetectionCompleted(
    const NetworkState* network,
    NetworkState::PortalState portal_state) {
  NET_LOG(EVENT) << "NetworkPortalDetector: DetectionCompleted: id="
                 << (network ? NetworkGuidId(network->guid()) : "<none>")
                 << ", PortalState=" << portal_state;

  if (network) {
    // Only set kPortal to override the shill result. Setting kUnknown will
    // ignore the Chrome result.
    if (portal_state == NetworkState::PortalState::kPortal) {
      SetNetworkPortalState(network, NetworkState::PortalState::kPortal);
    } else {
      SetNetworkPortalState(network, NetworkState::PortalState::kUnknown);
    }
  }

  MaybeReportMetrics(network, portal_state, /*detection_completed=*/true);
  ResetCountersAndSendMetrics();
}

void NetworkPortalDetectorImpl::ResetCountersAndSendMetrics() {
  if (captive_portal_detector_run_count_ > 0) {
    base::UmaHistogramCustomCounts("Network.NetworkPortalDetectorRunCount",
                                   captive_portal_detector_run_count_,
                                   /*min=*/1, /*exclusive_max=*/10,
                                   /*buckets=*/10);
    captive_portal_detector_run_count_ = 0;
  }
  metrics_reported_ = false;
}

bool NetworkPortalDetectorImpl::AttemptTimeoutIsCancelledForTesting() const {
  return attempt_timeout_task_.IsCancelled();
}

void NetworkPortalDetectorImpl::StartDetectionForTesting() {
  ScheduleAttempt();
}

}  // namespace ash
