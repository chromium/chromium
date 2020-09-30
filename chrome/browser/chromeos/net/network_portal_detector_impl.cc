// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/notification_service.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using captive_portal::CaptivePortalDetector;

namespace chromeos {

namespace {

// Delay before portal detection caused by changes in proxy settings.
constexpr int kProxyChangeDelaySec = 1;

// Maximum number of reports from captive portal detector about
// offline state in a row before notification is sent to observers.
constexpr int kMaxOfflineResultsBeforeReport = 3;

// Delay before portal detection attempt after !ONLINE -> !ONLINE
// transition.
constexpr int kShortInitialDelayBetweenAttemptsMs = 600;

// Maximum timeout before portal detection attempts after !ONLINE ->
// !ONLINE transition.
constexpr int kShortMaximumDelayBetweenAttemptsMs = 2 * 60 * 1000;

// Delay before portal detection attempt after !ONLINE -> ONLINE
// transition.
constexpr int kLongInitialDelayBetweenAttemptsMs = 30 * 1000;

// Maximum timeout before portal detection attempts after !ONLINE ->
// ONLINE transition.
constexpr int kLongMaximumDelayBetweenAttemptsMs = 5 * 60 * 1000;

const NetworkState* DefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

void SetNetworkPortalDetected(const NetworkState* network,
                              bool portal_detected) {
  NetworkHandler::Get()
      ->network_state_handler()
      ->SetNetworkChromePortalDetected(network->path(), portal_detected);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl, public:

NetworkPortalDetectorImpl::NetworkPortalDetectorImpl(
    network::mojom::URLLoaderFactory* loader_factory_for_testing)
    : strategy_(PortalDetectorStrategy::CreateById(
          PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN,
          this)) {
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
  captive_portal_detector_.reset(new CaptivePortalDetector(loader_factory));

  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());

  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
  StartPortalDetection(false /* force */);
}

NetworkPortalDetectorImpl::~NetworkPortalDetectorImpl() {
  NET_LOG(EVENT) << "NetworkPortalDetectorImpl::~NetworkPortalDetectorImpl()";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  attempt_task_.Cancel();
  attempt_timeout_.Cancel();

  captive_portal_detector_->Cancel();
  captive_portal_detector_.reset();
  observers_.Clear();
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
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
  CaptivePortalState portal_state;
  const NetworkState* network = DefaultNetwork();
  if (network)
    portal_state = GetCaptivePortalState(network->guid());
  observer->OnPortalDetectionCompleted(network, portal_state);
}

void NetworkPortalDetectorImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer)
    observers_.RemoveObserver(observer);
}

bool NetworkPortalDetectorImpl::IsEnabled() {
  return enabled_;
}

void NetworkPortalDetectorImpl::Enable(bool start_detection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enabled_)
    return;

  DCHECK(is_idle());
  enabled_ = true;

  const NetworkState* network = DefaultNetwork();
  if (!start_detection || !network)
    return;
  NET_LOG(EVENT) << "Starting detection attempt:"
                 << " id=" << NetworkId(network);
  SetNetworkPortalDetected(network, false /* portal_detected */);
  portal_state_map_.erase(network->guid());
  StartDetection();
}

NetworkPortalDetectorImpl::CaptivePortalState
NetworkPortalDetectorImpl::GetCaptivePortalState(const std::string& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CaptivePortalStateMap::const_iterator it = portal_state_map_.find(guid);
  if (it == portal_state_map_.end())
    return CaptivePortalState();
  return it->second;
}

bool NetworkPortalDetectorImpl::StartPortalDetection(bool force) {
  if (!is_idle()) {
    if (!force)
      return false;
    StopDetection();
  }
  StartDetection();
  return true;
}

void NetworkPortalDetectorImpl::SetStrategy(
    PortalDetectorStrategy::StrategyId id) {
  if (id == strategy_->Id())
    return;
  strategy_ = PortalDetectorStrategy::CreateById(id, this);
  StartPortalDetection(true /* force */);
}

void NetworkPortalDetectorImpl::DefaultNetworkChanged(
    const NetworkState* default_network) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!default_network) {
    NET_LOG(EVENT) << "Default network changed: None";

    default_proxy_config_.reset();

    StopDetection();

    CaptivePortalState state;
    state.status = CAPTIVE_PORTAL_STATUS_OFFLINE;
    DetectionCompleted(nullptr, state);
    return;
  }

  bool network_changed = (default_network_id_ != default_network->guid());
  if (network_changed)
    default_network_id_ = default_network->guid();

  bool connection_state_changed =
      (default_connection_state_ != default_network->connection_state());
  default_connection_state_ = default_network->connection_state();

  bool proxy_config_changed = false;
  if (!default_network->proxy_config()) {
    if (default_proxy_config_) {
      proxy_config_changed = true;
      default_proxy_config_.reset();
    }
  } else {
    if (!default_proxy_config_ || network_changed ||
        (*default_proxy_config_ != *default_network->proxy_config())) {
      proxy_config_changed = true;
      default_proxy_config_ = std::make_unique<base::Value>(
          default_network->proxy_config()->Clone());
    }
  }

  NET_LOG(EVENT) << "Default network changed:"
                 << " id=" << NetworkGuidId(default_network_id_)
                 << " state=" << default_connection_state_
                 << " changed=" << network_changed
                 << " state_changed=" << connection_state_changed;

  if (network_changed || connection_state_changed || proxy_config_changed)
    StopDetection();

  if (!NetworkState::StateIsConnected(default_connection_state_))
    return;

  if (proxy_config_changed) {
    ScheduleAttempt(base::TimeDelta::FromSeconds(kProxyChangeDelaySec));
    return;
  }

  if (is_idle()) {
    // Initiate Captive Portal detection if network's captive
    // portal state is unknown (e.g. for freshly created networks),
    // offline or if network connection state was changed.
    CaptivePortalState state = GetCaptivePortalState(default_network->guid());
    if (state.status == CAPTIVE_PORTAL_STATUS_UNKNOWN ||
        state.status == CAPTIVE_PORTAL_STATUS_OFFLINE ||
        (!network_changed && connection_state_changed)) {
      ScheduleAttempt(base::TimeDelta());
    }
  }
}

int NetworkPortalDetectorImpl::NoResponseResultCount() {
  return no_response_result_count_;
}

base::TimeTicks NetworkPortalDetectorImpl::AttemptStartTime() {
  return attempt_start_time_;
}

base::TimeTicks NetworkPortalDetectorImpl::NowTicks() const {
  if (time_ticks_for_testing_.is_null())
    return base::TimeTicks::Now();
  return time_ticks_for_testing_;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkPortalDetectorImpl, private:

void NetworkPortalDetectorImpl::StartDetection() {
  DCHECK(is_idle());

  ResetStrategyAndCounters();
  detection_start_time_ = NowTicks();
  ScheduleAttempt(base::TimeDelta());
}

void NetworkPortalDetectorImpl::StopDetection() {
  attempt_task_.Cancel();
  attempt_timeout_.Cancel();
  captive_portal_detector_->Cancel();
  state_ = STATE_IDLE;
  ResetStrategyAndCounters();
}

void NetworkPortalDetectorImpl::ScheduleAttempt(const base::TimeDelta& delay) {
  DCHECK(is_idle());

  if (!IsEnabled())
    return;

  attempt_task_.Cancel();
  attempt_timeout_.Cancel();
  state_ = STATE_PORTAL_CHECK_PENDING;

  next_attempt_delay_ = std::max(delay, strategy_->GetDelayTillNextAttempt());
  attempt_task_.Reset(base::Bind(&NetworkPortalDetectorImpl::StartAttempt,
                                 weak_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, attempt_task_.callback(), next_attempt_delay_);
}

void NetworkPortalDetectorImpl::StartAttempt() {
  DCHECK(is_portal_check_pending());

  state_ = STATE_CHECKING_FOR_PORTAL;
  attempt_start_time_ = NowTicks();

  NET_LOG(EVENT) << "Starting captive portal detection.";
  captive_portal_detector_->DetectCaptivePortal(
      GURL(CaptivePortalDetector::kDefaultURL),
      base::BindOnce(&NetworkPortalDetectorImpl::OnAttemptCompleted,
                     weak_factory_.GetWeakPtr()),
      NO_TRAFFIC_ANNOTATION_YET);
  attempt_timeout_.Reset(
      base::Bind(&NetworkPortalDetectorImpl::OnAttemptTimeout,
                 weak_factory_.GetWeakPtr()));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, attempt_timeout_.callback(),
      strategy_->GetNextAttemptTimeout());
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

  // If using a fake profile client, also fake being behind a captive portal
  // if the default network is in portal state.
  if (result != captive_portal::RESULT_NO_RESPONSE &&
      DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface() &&
      network && network->is_captive_portal()) {
    result = captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL;
    response_code = 200;
  }

  state_ = STATE_IDLE;
  attempt_timeout_.Cancel();

  CaptivePortalState state;
  state.response_code = response_code;
  state.time = NowTicks();
  bool no_response_since_portal = false;
  switch (result) {
    case captive_portal::RESULT_NO_RESPONSE:
      if (state.response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
        state.status = CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED;
      } else if (network && network->is_captive_portal()) {
        // Take into account shill's detection results.
        state.status = CAPTIVE_PORTAL_STATUS_PORTAL;
        no_response_since_portal = true;
      } else {
        state.status = CAPTIVE_PORTAL_STATUS_OFFLINE;
      }
      break;
    case captive_portal::RESULT_INTERNET_CONNECTED:
      state.status = CAPTIVE_PORTAL_STATUS_ONLINE;
      break;
    case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
      state.status = CAPTIVE_PORTAL_STATUS_PORTAL;
      break;
    default:
      break;
  }

  NET_LOG(EVENT) << "NetworkPortalDetector completed: id="
                 << NetworkGuidId(default_network_id_) << ", result="
                 << captive_portal::CaptivePortalResultToString(result)
                 << ", status=" << state.status
                 << ", response_code=" << response_code;

  UMA_HISTOGRAM_ENUMERATION("CaptivePortal.NetworkPortalDetectorResult",
                            state.status);
  NetworkState::NetworkTechnologyType type =
      NetworkState::NetworkTechnologyType::kUnknown;
  if (state.status == CAPTIVE_PORTAL_STATUS_PORTAL) {
    if (network)
      type = network->GetNetworkTechnologyType();
    UMA_HISTOGRAM_ENUMERATION("CaptivePortal.NetworkPortalDetectorType", type);
  }

  if (last_detection_result_ != state.status) {
    last_detection_result_ = state.status;
    same_detection_result_count_ = 1;
    net::BackoffEntry::Policy policy = strategy_->policy();
    if (state.status == CAPTIVE_PORTAL_STATUS_ONLINE) {
      policy.initial_delay_ms = kLongInitialDelayBetweenAttemptsMs;
      policy.maximum_backoff_ms = kLongMaximumDelayBetweenAttemptsMs;
    } else {
      policy.initial_delay_ms = kShortInitialDelayBetweenAttemptsMs;
      policy.maximum_backoff_ms = kShortMaximumDelayBetweenAttemptsMs;
    }
    strategy_->SetPolicyAndReset(policy);
  } else {
    ++same_detection_result_count_;
  }
  strategy_->OnDetectionCompleted();

  if (result == captive_portal::RESULT_NO_RESPONSE)
    ++no_response_result_count_;
  else
    no_response_result_count_ = 0;

  if (state.status != CAPTIVE_PORTAL_STATUS_OFFLINE ||
      same_detection_result_count_ >= kMaxOfflineResultsBeforeReport) {
    DetectionCompleted(network, state);
  }

  // Observers (via DetectionCompleted) may already schedule new attempt.
  if (is_idle())
    ScheduleAttempt(results.retry_after_delta);
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
    ScheduleAttempt(base::TimeDelta::FromSeconds(kProxyChangeDelaySec));
  }
}

void NetworkPortalDetectorImpl::DetectionCompleted(
    const NetworkState* network,
    const CaptivePortalState& state) {
  if (!network) {
    NotifyDetectionCompleted(network, state);
    return;
  }

  CaptivePortalStateMap::const_iterator it =
      portal_state_map_.find(network->guid());
  if (it == portal_state_map_.end() || it->second.status != state.status ||
      it->second.response_code != state.response_code) {
    // Record detection duration iff detection result differs from the
    // previous one for this network. The reason is to record all stats
    // only when network changes it's state.
    SetNetworkPortalDetected(
        network,
        state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
    portal_state_map_[network->guid()] = state;
  }
  NotifyDetectionCompleted(network, state);
}

void NetworkPortalDetectorImpl::NotifyDetectionCompleted(
    const NetworkState* network,
    const CaptivePortalState& state) {
  for (auto& observer : observers_)
    observer.OnPortalDetectionCompleted(network, state);
}

bool NetworkPortalDetectorImpl::AttemptTimeoutIsCancelledForTesting() const {
  return attempt_timeout_.IsCancelled();
}

void NetworkPortalDetectorImpl::ResetStrategyAndCounters() {
  last_detection_result_ = CAPTIVE_PORTAL_STATUS_UNKNOWN;
  same_detection_result_count_ = 0;
  no_response_result_count_ = 0;
  strategy_->Reset();
}

}  // namespace chromeos
