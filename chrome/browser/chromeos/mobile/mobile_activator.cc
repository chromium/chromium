// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mobile/mobile_activator.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_activation_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Number of times we'll try an OTASP before failing the activation process.
const int kMaxOTASPTries = 3;
// Number of times we will retry to reconnect and reload payment portal page.
const int kMaxPortalReconnectCount = 2;
// Time between connection attempts when forcing a reconnect.
const int kReconnectDelayMS = 3000;
// Retry delay after failed OTASP attempt.
const int kOTASPRetryDelay = 40000;
// Maximum amount of time we'll wait for a service to reconnect.
const int kMaxReconnectTime = 30000;

// Returns true if the device follows the simple activation flow.
bool IsSimpleActivationFlow(const chromeos::NetworkState* network) {
  return (network->activation_type() == shill::kActivationTypeNonCellular ||
          network->activation_type() == shill::kActivationTypeOTA);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// MobileActivator
//
////////////////////////////////////////////////////////////////////////////////
MobileActivator::MobileActivator()
    : state_(PLAN_ACTIVATION_PAGE_LOADING),
      terminated_(true),
      pending_activation_request_(false),
      connection_retry_count_(0),
      initial_OTASP_attempts_(0),
      trying_OTASP_attempts_(0),
      final_OTASP_attempts_(0),
      payment_reconnect_count_(0) {}

MobileActivator::~MobileActivator() {
  TerminateActivation();
}

MobileActivator* MobileActivator::GetInstance() {
  return base::Singleton<MobileActivator>::get();
}

void MobileActivator::TerminateActivation() {
  state_duration_timer_.Stop();
  continue_reconnect_timer_.Stop();
  reconnect_timeout_timer_.Stop();

  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->
        RemoveObserver(this, FROM_HERE);
  }
  meid_.clear();
  iccid_.clear();
  service_path_.clear();
  device_path_.clear();
  state_ = PLAN_ACTIVATION_PAGE_LOADING;
  terminated_ = true;
}

void MobileActivator::DefaultNetworkChanged(const NetworkState* network) {
  RefreshCellularNetworks();
}

void MobileActivator::NetworkPropertiesUpdated(const NetworkState* network) {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING)
    return;

  if (!network || network->type() != shill::kTypeCellular)
    return;

  const DeviceState* device = NetworkHandler::Get()->network_state_handler()->
      GetDeviceState(network->device_path());
  if (!device) {
    LOG(ERROR) << "Cellular device can't be found: " << network->device_path();
    return;
  }
  if (network->device_path() != device_path_) {
    LOG(WARNING) << "Ignoring property update for cellular service "
                 << network->path()
                 << " on unknown device " << network->device_path()
                 << " (Stored device path = " << device_path_ << ")";
    return;
  }

  // A modem reset leads to a new service path. Since we have verified that we
  // are a cellular service on a still valid stored device path, update it.
  service_path_ = network->path();

  EvaluateCellularNetwork(network);
}

void MobileActivator::AddObserver(MobileActivator::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void MobileActivator::RemoveObserver(MobileActivator::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void MobileActivator::InitiateActivation(const std::string& service_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const NetworkState* network =  GetNetworkState(service_path);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path;
    return;
  }
  const DeviceState* device = NetworkHandler::Get()->network_state_handler()->
      GetDeviceState(network->device_path());
  if (!device) {
    LOG(ERROR) << "Cellular device can't be found: " << network->device_path();
    return;
  }

  DCHECK(network->Matches(NetworkTypePattern::Cellular()));

  terminated_ = false;
  meid_ = device->meid();
  iccid_ = device->iccid();
  service_path_ = service_path;
  device_path_ = network->device_path();

  ChangeState(network, PLAN_ACTIVATION_PAGE_LOADING, ActivationError::kNone);

  // We want shill to connect us after activations, so enable autoconnect.
  base::DictionaryValue auto_connect_property;
  auto_connect_property.SetBoolean(shill::kAutoConnectProperty, true);
  NetworkHandler::Get()->network_configuration_handler()->SetShillProperties(
      service_path_, auto_connect_property, base::DoNothing(),
      network_handler::ErrorCallback());

  StartActivation();
}

void MobileActivator::GetPropertiesFailure(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("MobileActivator GetProperties Failed: " + error_name,
                service_path_);
}

void MobileActivator::OnSetTransactionStatus(bool success) {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&MobileActivator::HandleSetTransactionStatus,
                                weak_ptr_factory_.GetWeakPtr(), success));
}

void MobileActivator::HandleSetTransactionStatus(bool success) {
  // The payment is received, try to reconnect and check the status all over
  // again.
  if (success && state_ == PLAN_ACTIVATION_SHOWING_PAYMENT) {
    SignalCellularPlanPayment();
    UMA_HISTOGRAM_COUNTS_1M("Cellular.PaymentReceived", 1);
    const NetworkState* network = GetNetworkState(service_path_);
    if (network && IsSimpleActivationFlow(network)) {
      state_ = PLAN_ACTIVATION_DONE;
      NetworkHandler::Get()->network_activation_handler()->CompleteActivation(
          network->path(), base::DoNothing(), network_handler::ErrorCallback());
    } else {
      StartOTASP();
    }
  } else {
    UMA_HISTOGRAM_COUNTS_1M("Cellular.PaymentFailed", 1);
  }
}

void MobileActivator::OnPortalLoaded(bool success) {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&MobileActivator::HandlePortalLoaded,
                                weak_ptr_factory_.GetWeakPtr(), success));
}

void MobileActivator::HandlePortalLoaded(bool success) {
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    ChangeState(NULL, PLAN_ACTIVATION_ERROR,
                ActivationError::kNoCellularService);
    return;
  }
  if (state_ == PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING ||
      state_ == PLAN_ACTIVATION_SHOWING_PAYMENT) {
    if (success) {
      payment_reconnect_count_ = 0;
      ChangeState(network, PLAN_ACTIVATION_SHOWING_PAYMENT,
                  ActivationError::kNone);
    } else {
      // There is no point in forcing reconnecting the cellular network if the
      // activation should not be done over it.
      if (network->activation_type() == shill::kActivationTypeNonCellular)
        return;

      payment_reconnect_count_++;
      if (payment_reconnect_count_ > kMaxPortalReconnectCount) {
        ChangeState(NULL, PLAN_ACTIVATION_ERROR,
                    ActivationError::kNoCellularService);
        return;
      }

      // Reconnect and try and load the frame again.
      ChangeState(network, PLAN_ACTIVATION_RECONNECTING,
                  ActivationError::kNone);
    }
  } else {
    NOTREACHED() << "Called paymentPortalLoad while in unexpected state: "
                 << GetStateDescription(state_);
  }
}

void MobileActivator::StartOTASPTimer() {
  pending_activation_request_ = false;
  state_duration_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kOTASPRetryDelay),
      this, &MobileActivator::HandleOTASPTimeout);
}

void MobileActivator::StartActivation() {
  UMA_HISTOGRAM_COUNTS_1M("Cellular.MobileSetupStart", 1);
  const NetworkState* network = GetNetworkState(service_path_);
  // Check if we can start activation process.
  if (!network) {
    NetworkStateHandler::TechnologyState technology_state =
        NetworkHandler::Get()->network_state_handler()->GetTechnologyState(
            NetworkTypePattern::Cellular());
    ActivationError error = ActivationError::kActivationFailed;
    if (technology_state == NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
      error = ActivationError::kNoCellularDevice;
    } else if (technology_state != NetworkStateHandler::TECHNOLOGY_ENABLED) {
      error = ActivationError::kCellularDisabled;
    } else {
      error = ActivationError::kNoCellularService;
    }
    ChangeState(NULL, PLAN_ACTIVATION_ERROR, error);
    return;
  }

  // Start monitoring network property changes.
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);

  if (network->activation_type() == shill::kActivationTypeNonCellular) {
    StartActivationOverNonCellularNetwork();
  } else if (network->activation_type() == shill::kActivationTypeOTA) {
    StartActivationOTA();
  } else if (network->activation_type() == shill::kActivationTypeOTASP) {
    StartActivationOTASP();
  }
}

void MobileActivator::StartActivationOverNonCellularNetwork() {
  // Fast forward to payment portal loading.
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  ChangeState(network,
              (network->activation_state() == shill::kActivationStateActivated)
                  ? PLAN_ACTIVATION_DONE
                  : PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING,
              ActivationError::kNone);

  RefreshCellularNetworks();
}

void MobileActivator::StartActivationOTA() {
  // Connect to the network if we don't currently have access.
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  const NetworkState* default_network = GetDefaultNetwork();
  bool is_online_or_portal =
      default_network &&
      (default_network->connection_state() == shill::kStateOnline ||
       NetworkState::StateIsPortalled(default_network->connection_state()));
  if (!is_online_or_portal)
    ConnectNetwork(network);

  ChangeState(network, PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING,
              ActivationError::kNone);
  RefreshCellularNetworks();
}

void MobileActivator::StartActivationOTASP() {
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  if (HasRecentCellularPlanPayment() &&
      (network->activation_state() ==
       shill::kActivationStatePartiallyActivated)) {
    // Try to start with OTASP immediately if we have received payment recently.
    state_ = PLAN_ACTIVATION_START_OTASP;
  } else {
    state_ =  PLAN_ACTIVATION_START;
  }

  EvaluateCellularNetwork(network);
}

void MobileActivator::RetryOTASP() {
  DCHECK(state_ == PLAN_ACTIVATION_DELAY_OTASP);
  StartOTASP();
}

void MobileActivator::StartOTASP() {
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  ChangeState(network, PLAN_ACTIVATION_START_OTASP, ActivationError::kNone);
  EvaluateCellularNetwork(network);
}

void MobileActivator::HandleOTASPTimeout() {
  LOG(WARNING) << "OTASP seems to be taking too long.";
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  // We're here because one of OTASP steps is taking too long to complete.
  // Usually, this means something bad has happened below us.
  if (state_ == PLAN_ACTIVATION_INITIATING_ACTIVATION) {
    ++initial_OTASP_attempts_;
    if (initial_OTASP_attempts_ <= kMaxOTASPTries) {
      ChangeState(network, PLAN_ACTIVATION_RECONNECTING,
                  ActivationError::kActivationFailed);
      return;
    }
  } else if (state_ == PLAN_ACTIVATION_TRYING_OTASP) {
    ++trying_OTASP_attempts_;
    if (trying_OTASP_attempts_ <= kMaxOTASPTries) {
      ChangeState(network, PLAN_ACTIVATION_RECONNECTING,
                  ActivationError::kActivationFailed);
      return;
    }
  } else if (state_ == PLAN_ACTIVATION_OTASP) {
    ++final_OTASP_attempts_;
    if (final_OTASP_attempts_ <= kMaxOTASPTries) {
      // Give the portal time to propagate all those magic bits.
      ChangeState(network, PLAN_ACTIVATION_DELAY_OTASP,
                  ActivationError::kActivationFailed);
      return;
    }
  } else {
    LOG(ERROR) << "OTASP timed out from a non-OTASP wait state?";
  }
  LOG(ERROR) << "OTASP failed too many times; aborting.";
  ChangeState(network, PLAN_ACTIVATION_ERROR,
              ActivationError::kActivationFailed);
}

void MobileActivator::ConnectNetwork(const NetworkState* network) {
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      network->path(), base::DoNothing(), network_handler::ErrorCallback(),
      false /* check_error_state */, ConnectCallbackMode::ON_STARTED);
}

void MobileActivator::ForceReconnect(const NetworkState* network,
                                     PlanActivationState next_state) {
  DCHECK(network);
  // Store away our next destination for when we complete.
  post_reconnect_state_ = next_state;
  UMA_HISTOGRAM_COUNTS_1M("Cellular.ActivationRetry", 1);
  // First, disconnect...
  VLOG(1) << "Disconnecting from " << network->path();
  // Explicit service Disconnect()s disable autoconnect on the service until
  // Connect() is called on the service again.  Hence this dance to explicitly
  // call Connect().
  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      network->path(), base::DoNothing(), network_handler::ErrorCallback());
  // Keep trying to connect until told otherwise.
  continue_reconnect_timer_.Stop();
  continue_reconnect_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kReconnectDelayMS),
      this, &MobileActivator::ContinueConnecting);
  // If we don't ever connect again, we're going to call this a failure.
  reconnect_timeout_timer_.Stop();
  reconnect_timeout_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kMaxReconnectTime),
      this, &MobileActivator::ReconnectTimedOut);
}

void MobileActivator::ReconnectTimedOut() {
  LOG(ERROR) << "Ending activation attempt after failing to reconnect.";
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  ChangeState(network, PLAN_ACTIVATION_ERROR,
              ActivationError::kActivationFailed);
}

void MobileActivator::ContinueConnecting() {
  const NetworkState* network = GetNetworkState(service_path_);
  if (network && network->IsConnectedState()) {
    if (NetworkState::StateIsPortalled(network->connection_state()) &&
        network->GetError() == shill::kErrorDNSLookupFailed) {
      // It isn't an error to be in a restricted pool, but if DNS doesn't work,
      // then we're not getting traffic through at all.  Just disconnect and
      // try again.
      NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
          network->path(), base::DoNothing(), network_handler::ErrorCallback());
      return;
    }
    // Stop this callback
    continue_reconnect_timer_.Stop();
    EvaluateCellularNetwork(network);
  } else {
    LOG(WARNING) << "Connect failed, will try again in a little bit.";
    if (network) {
      VLOG(1) << "Connecting to: " << network->path();
      NetworkConnect::Get()->ConnectToNetworkId(network->guid());
    }
  }
}

void MobileActivator::RefreshCellularNetworks() {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING ||
      state_ == PLAN_ACTIVATION_DONE ||
      state_ == PLAN_ACTIVATION_ERROR) {
    return;
  }

  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  if (IsSimpleActivationFlow(network)) {
    bool waiting = (state_ == PLAN_ACTIVATION_WAITING_FOR_CONNECTION);
    // We're only interested in whether or not we have access to the payment
    // portal (regardless of which network we use to access it), so check
    // the default network connection state. The default network is the network
    // used to route default traffic. Also, note that we can access the
    // payment portal over a cellular network in the portalled state.
    const NetworkState* default_network = GetDefaultNetwork();
    bool is_online_or_portal =
        default_network &&
        (default_network->connection_state() == shill::kStateOnline ||
         (default_network->type() == shill::kTypeCellular &&
          NetworkState::StateIsPortalled(default_network->connection_state())));
    if (waiting && is_online_or_portal) {
      ChangeState(network, post_reconnect_state_, ActivationError::kNone);
    } else if (!waiting && !is_online_or_portal) {
      ChangeState(network, PLAN_ACTIVATION_WAITING_FOR_CONNECTION,
                  ActivationError::kNone);
    }
  }

  EvaluateCellularNetwork(network);
}

const NetworkState* MobileActivator::GetNetworkState(
    const std::string& service_path) {
  return NetworkHandler::Get()->network_state_handler()->GetNetworkState(
      service_path);
}

const NetworkState* MobileActivator::GetDefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

void MobileActivator::EvaluateCellularNetwork(const NetworkState* network) {
  if (terminated_) {
    LOG(ERROR) << "Tried to run MobileActivator state machine while "
               << "terminated.";
    return;
  }

  if (!network) {
    LOG(WARNING) << "Cellular service lost";
    return;
  }

  LOG(WARNING) << "Cellular:\n  service state=" << network->connection_state()
               << "\n  ui=" << GetStateDescription(state_)
               << "\n  activation=" << network->activation_state()
               << "\n  error=" << network->GetError()
               << "\n  setvice_path=" << network->path()
               << "\n  connected=" << network->IsConnectedState();

  // If the network is activated over non cellular network or OTA, the
  // activator state does not depend on the network's own state.
  if (IsSimpleActivationFlow(network))
    return;

  PlanActivationState new_state = PickNextState(network);
  ActivationError error = new_state == PLAN_ACTIVATION_ERROR
                              ? ActivationError::kActivationFailed
                              : ActivationError::kNone;
  ChangeState(network, new_state, error);
}

MobileActivator::PlanActivationState MobileActivator::PickNextState(
    const NetworkState* network) const {
  PlanActivationState new_state = state_;
  if (!network->IsConnectedState())
    new_state = PickNextOfflineState(network);
  else
    new_state = PickNextOnlineState(network);

  if (new_state != PLAN_ACTIVATION_ERROR &&
      network->connection_state() == shill::kStateActivationFailure) {
    // Check for this special case when we try to do activate partially
    // activated device. If that attempt failed, try to disconnect to clear the
    // state and reconnect again.
    const std::string& activation = network->activation_state();
    if ((activation == shill::kActivationStatePartiallyActivated ||
         activation == shill::kActivationStateActivating) &&
        (network->GetError().empty() ||
         network->GetError() == shill::kErrorOtaspFailed)) {
      NET_LOG_EVENT("Activation failure detected ", network->path());
      switch (state_) {
        case PLAN_ACTIVATION_OTASP:
          new_state = PLAN_ACTIVATION_DELAY_OTASP;
          break;
        case PLAN_ACTIVATION_INITIATING_ACTIVATION:
        case PLAN_ACTIVATION_TRYING_OTASP:
          new_state = PLAN_ACTIVATION_START;
          break;
        case PLAN_ACTIVATION_START:
          // We are just starting, so this must be previous activation attempt
          // failure.
          new_state = PLAN_ACTIVATION_TRYING_OTASP;
          break;
        case PLAN_ACTIVATION_DELAY_OTASP:
          new_state = state_;
          break;
        default:
          new_state = PLAN_ACTIVATION_ERROR;
          break;
      }
    } else {
      LOG(WARNING) << "Unexpected activation failure for " << network->path();
      new_state = PLAN_ACTIVATION_ERROR;
    }
  }

  return new_state;
}

MobileActivator::PlanActivationState MobileActivator::PickNextOfflineState(
    const NetworkState* network) const {
  PlanActivationState new_state = state_;
  const std::string& activation = network->activation_state();
  switch (state_) {
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      if (!IsSimpleActivationFlow(network))
        new_state = PLAN_ACTIVATION_RECONNECTING;
      break;
    case PLAN_ACTIVATION_START:
      if (activation == shill::kActivationStateActivated) {
        if (NetworkState::StateIsPortalled(network->connection_state()))
          new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
        else
          new_state = PLAN_ACTIVATION_DONE;
      } else if (activation == shill::kActivationStatePartiallyActivated) {
        new_state = PLAN_ACTIVATION_TRYING_OTASP;
      } else {
        new_state = PLAN_ACTIVATION_INITIATING_ACTIVATION;
      }
      break;
    default:
      VLOG(1) << "Waiting for cellular service to connect.";
      break;
  }
  return new_state;
}

MobileActivator::PlanActivationState MobileActivator::PickNextOnlineState(
    const NetworkState* network) const {
  PlanActivationState new_state = state_;
  const std::string& activation = network->activation_state();
  switch (state_) {
    case PLAN_ACTIVATION_START:
      if (activation == shill::kActivationStateActivated) {
        if (network->connection_state() == shill::kStateOnline)
          new_state = PLAN_ACTIVATION_DONE;
        else
          new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
      } else if (activation == shill::kActivationStatePartiallyActivated) {
        new_state = PLAN_ACTIVATION_TRYING_OTASP;
      } else {
        new_state = PLAN_ACTIVATION_INITIATING_ACTIVATION;
      }
      break;
    case PLAN_ACTIVATION_START_OTASP: {
      if (activation == shill::kActivationStatePartiallyActivated) {
          new_state = PLAN_ACTIVATION_OTASP;
      } else if (activation == shill::kActivationStateActivated) {
        new_state = PLAN_ACTIVATION_RECONNECTING;
      } else {
        LOG(WARNING) << "Unexpected activation state for device "
                     << network->path();
      }
      break;
    }
    case PLAN_ACTIVATION_DELAY_OTASP:
      // Just ignore any changes until the OTASP retry timer kicks in.
      break;
    case PLAN_ACTIVATION_INITIATING_ACTIVATION: {
      if (pending_activation_request_) {
        VLOG(1) << "Waiting for pending activation attempt to finish";
      } else if (activation == shill::kActivationStateActivated ||
                 activation == shill::kActivationStatePartiallyActivated) {
        new_state = PLAN_ACTIVATION_START;
      } else if (activation == shill::kActivationStateNotActivated ||
                 activation == shill::kActivationStateActivating) {
        // Wait in this state until activation state changes.
      } else {
        LOG(WARNING) << "Unknown transition";
      }
      break;
    }
    case PLAN_ACTIVATION_OTASP:
    case PLAN_ACTIVATION_TRYING_OTASP:
      if (pending_activation_request_) {
        VLOG(1) << "Waiting for pending activation attempt to finish";
      } else if (activation == shill::kActivationStateNotActivated ||
                 activation == shill::kActivationStateActivating) {
        VLOG(1) << "Waiting for the OTASP to finish and the service to "
                << "come back online";
      } else if (activation == shill::kActivationStateActivated) {
        new_state = PLAN_ACTIVATION_DONE;
      } else {
        new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
      }
      break;
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
      if (!NetworkState::StateIsPortalled(network->connection_state()) &&
          activation == shill::kActivationStateActivated)
        // We're not portalled, and we're already activated, so we're online!
        new_state = PLAN_ACTIVATION_DONE;
      else
        new_state = PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING;
      break;
    // Initial state
    case PLAN_ACTIVATION_PAGE_LOADING:
      break;
    // Just ignore all signals until the site confirms payment.
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
    case PLAN_ACTIVATION_WAITING_FOR_CONNECTION:
      break;
    // Go where we decided earlier.
    case PLAN_ACTIVATION_RECONNECTING:
      new_state = post_reconnect_state_;
      break;
    // Activation completed/failed, ignore network changes.
    case PLAN_ACTIVATION_DONE:
    case PLAN_ACTIVATION_ERROR:
      break;
  }

  return new_state;
}

// Debugging helper function, will take it out at the end.
const char* MobileActivator::GetStateDescription(PlanActivationState state) {
  switch (state) {
    case PLAN_ACTIVATION_PAGE_LOADING:
      return "PAGE_LOADING";
    case PLAN_ACTIVATION_START:
      return "ACTIVATION_START";
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      return "INITIATING_ACTIVATION";
    case PLAN_ACTIVATION_TRYING_OTASP:
      return "TRYING_OTASP";
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
      return "PAYMENT_PORTAL_LOADING";
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      return "SHOWING_PAYMENT";
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
      return "RECONNECTING_PAYMENT";
    case PLAN_ACTIVATION_DELAY_OTASP:
      return "DELAY_OTASP";
    case PLAN_ACTIVATION_START_OTASP:
      return "START_OTASP";
    case PLAN_ACTIVATION_OTASP:
      return "OTASP";
    case PLAN_ACTIVATION_DONE:
      return "DONE";
    case PLAN_ACTIVATION_ERROR:
      return "ERROR";
    case PLAN_ACTIVATION_RECONNECTING:
      return "RECONNECTING";
    case PLAN_ACTIVATION_WAITING_FOR_CONNECTION:
      return "WAITING FOR CONNECTION";
  }
  return "UNKNOWN";
}


void MobileActivator::CompleteActivation() {
  // Remove observers, we are done with this page.
  NetworkHandler::Get()->network_state_handler()->
      RemoveObserver(this, FROM_HERE);
}

bool MobileActivator::RunningActivation() const {
  return !(state_ == PLAN_ACTIVATION_DONE ||
           state_ == PLAN_ACTIVATION_ERROR ||
           state_ == PLAN_ACTIVATION_PAGE_LOADING);
}

void MobileActivator::HandleActivationFailure(
    const std::string& service_path,
    PlanActivationState new_state,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  pending_activation_request_ = false;
  const NetworkState* network = GetNetworkState(service_path);
  if (!network) {
    NET_LOG_ERROR("Cellular service no longer exists", service_path);
    return;
  }
  UMA_HISTOGRAM_COUNTS_1M("Cellular.ActivationFailure", 1);
  NET_LOG_ERROR("Failed to call Activate() on service", service_path);
  if (new_state == PLAN_ACTIVATION_OTASP) {
    ChangeState(network, PLAN_ACTIVATION_DELAY_OTASP, ActivationError::kNone);
  } else {
    ChangeState(network, PLAN_ACTIVATION_ERROR,
                ActivationError::kActivationFailed);
  }
}

void MobileActivator::RequestCellularActivation(
    const NetworkState* network,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  DCHECK(network);
  NET_LOG_EVENT("Activating cellular service", network->path());
  UMA_HISTOGRAM_COUNTS_1M("Cellular.ActivationTry", 1);
  pending_activation_request_ = true;
  NetworkHandler::Get()->network_activation_handler()->
      Activate(network->path(),
               "",  // carrier
               success_callback,
               error_callback);
}

void MobileActivator::ChangeState(const NetworkState* network,
                                  PlanActivationState new_state,
                                  ActivationError error) {
  // Report an error, by transitioning into a PLAN_ACTIVATION_ERROR state with
  // a "no service" error instead, if no network state is available (e.g. the
  // cellular service no longer exists) when we are transitioning into certain
  // plan activation state.
  if (!network) {
    switch (new_state) {
      case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      case PLAN_ACTIVATION_TRYING_OTASP:
      case PLAN_ACTIVATION_OTASP:
      case PLAN_ACTIVATION_DONE:
        new_state = PLAN_ACTIVATION_ERROR;
        error = ActivationError::kNoCellularService;
        break;
      default:
        break;
    }
  }

  static bool first_time = true;
  VLOG(1) << "Activation state flip old = "
          << GetStateDescription(state_)
          << ", new = " << GetStateDescription(new_state);
  if (state_ == new_state && !first_time)
    return;
  first_time = false;
  VLOG(1) << "Transitioning...";

  // Kill all the possible timers and callbacks we might have outstanding.
  state_duration_timer_.Stop();
  continue_reconnect_timer_.Stop();
  reconnect_timeout_timer_.Stop();
  const PlanActivationState old_state = state_;
  state_ = new_state;

  // Signal to observers layer that the state is changing.
  for (auto& observer : observers_)
    observer.OnActivationStateChanged(network, state_, error);

  // Pick action that should happen on entering the new state.
  switch (new_state) {
    case PLAN_ACTIVATION_START:
      break;
    case PLAN_ACTIVATION_DELAY_OTASP: {
      UMA_HISTOGRAM_COUNTS_1M("Cellular.RetryOTASP", 1);
      base::PostDelayedTask(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&MobileActivator::RetryOTASP,
                         weak_ptr_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kOTASPRetryDelay));
      break;
    }
    case PLAN_ACTIVATION_START_OTASP:
      break;
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
    case PLAN_ACTIVATION_TRYING_OTASP:
    case PLAN_ACTIVATION_OTASP: {
      DCHECK(network);
      network_handler::ErrorCallback on_activation_error = base::BindRepeating(
          &MobileActivator::HandleActivationFailure,
          weak_ptr_factory_.GetWeakPtr(), network->path(), new_state);
      RequestCellularActivation(
          network,
          base::BindRepeating(&MobileActivator::StartOTASPTimer,
                              weak_ptr_factory_.GetWeakPtr()),
          on_activation_error);
      }
      break;
    case PLAN_ACTIVATION_PAGE_LOADING:
      return;
    case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
    case PLAN_ACTIVATION_RECONNECTING_PAYMENT:
      // Fix for fix SSL for the walled gardens where cert chain verification
      // might not work.
      break;
    case PLAN_ACTIVATION_WAITING_FOR_CONNECTION:
      post_reconnect_state_ = old_state;
      break;
    case PLAN_ACTIVATION_RECONNECTING: {
      PlanActivationState next_state = old_state;
      // Pick where we want to return to after we reconnect.
      switch (old_state) {
        case PLAN_ACTIVATION_PAYMENT_PORTAL_LOADING:
        case PLAN_ACTIVATION_SHOWING_PAYMENT:
          // We decide here what to do next based on the state of the modem.
          next_state = PLAN_ACTIVATION_RECONNECTING_PAYMENT;
          break;
        case PLAN_ACTIVATION_INITIATING_ACTIVATION:
        case PLAN_ACTIVATION_TRYING_OTASP:
          next_state = PLAN_ACTIVATION_START;
          break;
        case PLAN_ACTIVATION_START_OTASP:
        case PLAN_ACTIVATION_OTASP:
          if (!network || !network->IsConnectedState()) {
            next_state = PLAN_ACTIVATION_START_OTASP;
          } else {
            // We're online, which means we've conspired with
            // PickNextOnlineState to reconnect after activation (that's the
            // only way we see this transition).  Thus, after we reconnect, we
            // should be done.
            next_state = PLAN_ACTIVATION_DONE;
          }
          break;
        default:
          LOG(ERROR) << "Transitioned to RECONNECTING from an unexpected "
                     << "state.";
          break;
      }
      if (network)
        ForceReconnect(network, next_state);
      break;
    }
    case PLAN_ACTIVATION_DONE:
      DCHECK(network);
      CompleteActivation();
      UMA_HISTOGRAM_COUNTS_1M("Cellular.MobileSetupSucceeded", 1);
      break;
    case PLAN_ACTIVATION_ERROR:
      CompleteActivation();
      UMA_HISTOGRAM_COUNTS_1M("Cellular.PlanFailed", 1);
      break;
    default:
      break;
  }
}

void MobileActivator::SignalCellularPlanPayment() {
  DCHECK(!HasRecentCellularPlanPayment());
  cellular_plan_payment_time_ = base::Time::Now();
}

bool MobileActivator::HasRecentCellularPlanPayment() const {
  const int kRecentPlanPaymentHours = 6;
  return (base::Time::Now() -
          cellular_plan_payment_time_).InHours() < kRecentPlanPaymentHours;
}

}  // namespace chromeos
