// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_portal_detector_impl.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/login/login_state/login_state.h"
#include "content/public/browser/notification_service.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

using ::captive_portal::CaptivePortalDetector;

// Default delay between portal detection attempts when Chrome portal detection
// is used (for detecting proxy auth or when Shill portal state is unknown).
constexpr base::TimeDelta kDefaultAttemptDelay = base::Seconds(1);

// Delay before portal detection caused by changes in proxy settings.
constexpr int kProxyChangeDelaySec = 1;

// Timeout for attempts.
constexpr base::TimeDelta kAttemptTimeout = base::Seconds(15);

// Maximum number of reports from captive portal detector about
// offline state in a row before notification is sent to observers.
constexpr int kMaxOfflineResultsBeforeReport = 3;

const NetworkState* DefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

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

  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());

  network_state_handler_observer_.Observe(
      NetworkHandler::Get()->network_state_handler());
  StartPortalDetection();
}

NetworkPortalDetectorImpl::~NetworkPortalDetectorImpl() {
  NET_LOG(EVENT) << "NetworkPortalDetectorImpl::~NetworkPortalDetectorImpl()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  attempt_task_.Cancel();
  attempt_timeout_task_.Cancel();

  captive_portal_detector_->Cancel();
  captive_portal_detector_.reset();
  observers_.Clear();
  for (auto& observer : observers_)
    observer.OnShutdown();
}

void NetworkPortalDetectorImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer && !observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkPortalDetectorImpl::AddAndFireObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observer)
    return;
  AddObserver(observer);
  observer->OnPortalDetectionCompleted(DefaultNetwork(),
                                       GetCaptivePortalStatus());
}

void NetworkPortalDetectorImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer)
    observers_.RemoveObserver(observer);
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
  StartDetection();
}

NetworkPortalDetector::CaptivePortalStatus
NetworkPortalDetectorImpl::GetCaptivePortalStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return default_portal_status_;
}

void NetworkPortalDetectorImpl::StartPortalDetection() {
  if (!is_idle())
    return;
  const NetworkState* network = DefaultNetwork();
  if (!network) {
    NET_LOG(ERROR) << "StartPortalDetection called with no default network.";
    return;
  }
  StartDetection();
}

void NetworkPortalDetectorImpl::PortalStateChanged(
    const NetworkState* default_network,
    NetworkState::PortalState portal_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!default_network || !default_network->IsConnectedState()) {
    NET_LOG(EVENT)
        << "No connected default network, stopping portal detection.";
    default_network_id_ = std::string();
    StopDetection();
    DetectionCompleted(nullptr, CAPTIVE_PORTAL_STATUS_OFFLINE);
    return;
  }

  default_network_id_ = default_network->guid();
  bool has_proxy = !default_network->proxy_config().is_none();
  NET_LOG(EVENT) << "PortalStateChanged, id="
                 << NetworkGuidId(default_network_id_)
                 << " state=" << default_network->connection_state()
                 << " portal_state=" << portal_state
                 << " has_proxy=" << has_proxy;

  switch (portal_state) {
    case NetworkState::PortalState::kUnknown:
      // Not expected. Shill detection failed or unexpected results, use Chrome
      // portal detection.
      NET_LOG(ERROR) << "Unknown PortalState, scheduling Chrome detection.";
      ScheduleAttempt();
      return;
    case NetworkState::PortalState::kOnline:
      // If a proxy is configured, use captive_portal_detector_ to detect a
      // proxy auth required (407) response.
      if (has_proxy)
        ScheduleAttempt();
      else
        DetectionCompleted(default_network, CAPTIVE_PORTAL_STATUS_ONLINE);
      return;
    case NetworkState::PortalState::kPortalSuspected:
      // Shill result was inconclusive.
      ScheduleAttempt();
      return;
    case NetworkState::PortalState::kPortal:
      DetectionCompleted(default_network, CAPTIVE_PORTAL_STATUS_PORTAL);
      return;
    case NetworkState::PortalState::kNoInternet:
      // If a proxy is configured it may be interfering with Shill portal
      // detection
      if (has_proxy)
        ScheduleAttempt();
      else
        DetectionCompleted(default_network, CAPTIVE_PORTAL_STATUS_ONLINE);
      return;
    case NetworkState::PortalState::kProxyAuthRequired:
      // This may happen if a global proxy is applied. Run Chrome detection
      // to verify.
      ScheduleAttempt();
      return;
  }
}

void NetworkPortalDetectorImpl::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl, private:

void NetworkPortalDetectorImpl::StartDetection() {
  NET_LOG(EVENT) << "StartDetection";

  ResetCountersAndSendMetrics();
  default_portal_status_ = CAPTIVE_PORTAL_STATUS_UNKNOWN;
  ScheduleAttempt();
}

void NetworkPortalDetectorImpl::StopDetection() {
  if (is_idle())
    return;
  NET_LOG(EVENT) << "StopDetection";
  attempt_task_.Cancel();
  attempt_timeout_task_.Cancel();
  captive_portal_detector_->Cancel();
  default_portal_status_ = CAPTIVE_PORTAL_STATUS_UNKNOWN;
  state_ = STATE_IDLE;
  ResetCountersAndSendMetrics();
}

void NetworkPortalDetectorImpl::ScheduleAttempt(const base::TimeDelta& delay) {
  DCHECK(is_idle());

  if (!IsEnabled())
    return;

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
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, attempt_task_.callback(), next_attempt_delay_);
}

void NetworkPortalDetectorImpl::StartAttempt() {
  DCHECK(is_portal_check_pending());

  state_ = STATE_CHECKING_FOR_PORTAL;

  NET_LOG(EVENT) << "Starting captive portal detection.";
  captive_portal_detector_->DetectCaptivePortal(
      GURL(CaptivePortalDetector::kDefaultURL),
      base::BindOnce(&NetworkPortalDetectorImpl::OnAttemptCompleted,
                     weak_factory_.GetWeakPtr()),
      NO_TRAFFIC_ANNOTATION_YET);
  attempt_timeout_task_.Reset(
      base::BindOnce(&NetworkPortalDetectorImpl::OnAttemptTimeout,
                     weak_factory_.GetWeakPtr()));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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

  bool shill_is_captive_portal = false;
  if (network) {
    switch (network->shill_portal_state()) {
      case NetworkState::PortalState::kUnknown:
      case NetworkState::PortalState::kOnline:
        break;
      // TODO(b/207069182): Handle each state correctly.
      case NetworkState::PortalState::kPortalSuspected:
      case NetworkState::PortalState::kPortal:
      case NetworkState::PortalState::kProxyAuthRequired:
      case NetworkState::PortalState::kNoInternet:
        shill_is_captive_portal = true;
        break;
    }
  }

  state_ = STATE_IDLE;
  attempt_timeout_task_.Cancel();

  CaptivePortalStatus status = CAPTIVE_PORTAL_STATUS_UNKNOWN;
  switch (result) {
    case captive_portal::RESULT_NO_RESPONSE:
      if (response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
        status = CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
      } else if (shill_is_captive_portal) {
        // Take into account shill's detection results.
        status = CAPTIVE_PORTAL_STATUS_PORTAL;
      } else {
        // We should only get here if Shill does not detect a portal but the
        // Chrome detector does not receive a response. Use 'offline' to
        // trigger continued detection.
        status = CAPTIVE_PORTAL_STATUS_OFFLINE;
      }
      break;
    case captive_portal::RESULT_INTERNET_CONNECTED:
      status = CAPTIVE_PORTAL_STATUS_ONLINE;
      break;
    case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
      status = CAPTIVE_PORTAL_STATUS_PORTAL;
      break;
    case captive_portal::RESULT_COUNT:
      NOTREACHED();
      break;
  }

  NET_LOG(EVENT) << "NetworkPortalDetector: AttemptCompleted: id="
                 << NetworkGuidId(default_network_id_) << ", result="
                 << captive_portal::CaptivePortalResultToString(result)
                 << ", status=" << status
                 << ", response_code=" << response_code;

  base::UmaHistogramEnumeration("Network.NetworkPortalDetectorResult", status);
  NetworkState::NetworkTechnologyType type =
      NetworkState::NetworkTechnologyType::kUnknown;
  if (status == CAPTIVE_PORTAL_STATUS_PORTAL) {
    if (network)
      type = network->GetNetworkTechnologyType();
    base::UmaHistogramEnumeration("Network.NetworkPortalDetectorType", type);
  }

  if (last_detection_status_ != status) {
    last_detection_status_ = status;
    same_detection_result_count_ = 1;
  } else {
    ++same_detection_result_count_;
  }

  captive_portal_detector_run_count_++;

  bool detection_completed = false;
  if (status == CAPTIVE_PORTAL_STATUS_ONLINE ||
      status == CAPTIVE_PORTAL_STATUS_PORTAL ||
      status == CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED) {
    // Chrome positively identified an online, portal or proxy auth state.
    // No need to continue detection.
    detection_completed = true;
  } else if (same_detection_result_count_ >= kMaxOfflineResultsBeforeReport) {
    NET_LOG(EVENT) << "Max identical portal detection results reached: "
                   << same_detection_result_count_ << " Status: " << status;
    detection_completed = true;
  }

  if (detection_completed) {
    response_code_for_testing_ = response_code;
    DetectionCompleted(network, status);
  } else if (is_idle()) {
    ScheduleAttempt(results.retry_after_delta);
  }
}

void NetworkPortalDetectorImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
      type == chrome::NOTIFICATION_AUTH_CANCELLED) {
    NET_LOG(EVENT) << "Restarting portal detection due to auth change"
                   << " id=" << NetworkGuidId(default_network_id_);
    StopDetection();
    ScheduleAttempt(base::Seconds(kProxyChangeDelaySec));
  }
}

void NetworkPortalDetectorImpl::DetectionCompleted(
    const NetworkState* network,
    const CaptivePortalStatus& status) {
  NET_LOG(EVENT) << "NetworkPortalDetector: DetectionCompleted: id="
                 << (network ? NetworkGuidId(network->guid()) : "<none>")
                 << ", status=" << status;

  default_portal_status_ = status;
  if (network) {
    NetworkState::PortalState portal_state;
    switch (status) {
      case CAPTIVE_PORTAL_STATUS_UNKNOWN:
      case CAPTIVE_PORTAL_STATUS_COUNT:
      case CAPTIVE_PORTAL_STATUS_OFFLINE:
        portal_state = NetworkState::PortalState::kUnknown;
        break;
      case CAPTIVE_PORTAL_STATUS_ONLINE:
        // TODO(b/207069182): This should state PortalState::kOnline.
        portal_state = NetworkState::PortalState::kUnknown;
        break;
      case CAPTIVE_PORTAL_STATUS_PORTAL:
        portal_state = NetworkState::PortalState::kPortal;
        break;
      case CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
        // TODO(b/207069182): This should state PortalState::kProxyAuthRequired.
        portal_state = NetworkState::PortalState::kUnknown;
        break;
    }
    // Note: setting an unknown portal state will ignore the Chrome result and
    // fall back to the Shill result.
    SetNetworkPortalState(network, portal_state);
  }
  for (auto& observer : observers_)
    observer.OnPortalDetectionCompleted(network, status);

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
  last_detection_status_ = CAPTIVE_PORTAL_STATUS_UNKNOWN;
  same_detection_result_count_ = 0;
}

bool NetworkPortalDetectorImpl::AttemptTimeoutIsCancelledForTesting() const {
  return attempt_timeout_task_.IsCancelled();
}

}  // namespace ash
