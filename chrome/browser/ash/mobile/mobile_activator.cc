// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mobile/mobile_activator.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_activation_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace ash {

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
bool IsSimpleActivationFlow(const NetworkState* network) {
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
    : state_(PlanActivationState::kPageLoading),
      terminated_(true),
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

  network_state_handler_observer_.Reset();

  meid_.clear();
  iccid_.clear();
  service_path_.clear();
  device_path_.clear();
  state_ = PlanActivationState::kPageLoading;
  terminated_ = true;
}

void MobileActivator::DefaultNetworkChanged(const NetworkState* network) {
  RefreshCellularNetworks();
}

void MobileActivator::NetworkPropertiesUpdated(const NetworkState* network) {
  if (state_ == PlanActivationState::kPageLoading)
    return;

  if (!network || network->type() != shill::kTypeCellular)
    return;

  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          network->device_path());
  if (!device) {
    LOG(ERROR) << "Cellular device can't be found: " << network->device_path();
    return;
  }
  if (network->device_path() != device_path_) {
    LOG(WARNING) << "Ignoring property update for cellular service "
                 << network->path() << " on unknown device "
                 << network->device_path()
                 << " (Stored device path = " << device_path_ << ")";
    return;
  }

  // A modem reset leads to a new service path. Since we have verified that we
  // are a cellular service on a still valid stored device path, update it.
  service_path_ = network->path();

  EvaluateCellularNetwork(network);
}

void MobileActivator::OnShuttingDown() {
  network_state_handler_observer_.Reset();
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
  const NetworkState* network = GetNetworkState(service_path);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path;
    return;
  }
  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          network->device_path());
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

  ChangeState(network, PlanActivationState::kPageLoading,
              ActivationError::kNone);

  // We want shill to connect us after activations, so enable autoconnect.
  base::Value::Dict auto_connect_property;
  auto_connect_property.Set(shill::kAutoConnectProperty, true);
  NetworkHandler::Get()->network_configuration_handler()->SetShillProperties(
      service_path_, std::move(auto_connect_property), base::DoNothing(),
      network_handler::ErrorCallback());

  StartActivation();
}

void MobileActivator::GetPropertiesFailure(const std::string& error_name,
                                           base::Value error_data) {
  NET_LOG(ERROR) << "MobileActivator GetProperties failed for "
                 << NetworkPathId(service_path_) << " Error: " << error_name;
}

void MobileActivator::OnSetTransactionStatus(bool success) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MobileActivator::HandleSetTransactionStatus,
                                weak_ptr_factory_.GetWeakPtr(), success));
}

void MobileActivator::HandleSetTransactionStatus(bool success) {
  // The payment is received, try to reconnect and check the status all over
  // again.
  if (success && state_ == PlanActivationState::kShowingPayment) {
    SignalCellularPlanPayment();
    const NetworkState* network = GetNetworkState(service_path_);
    if (network && IsSimpleActivationFlow(network)) {
      state_ = PlanActivationState::kDone;
      NetworkHandler::Get()->network_activation_handler()->CompleteActivation(
          network->path(), base::DoNothing(), network_handler::ErrorCallback());
    } else {
      StartOTASP();
    }
  }
}

void MobileActivator::OnPortalLoaded(bool success) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MobileActivator::HandlePortalLoaded,
                                weak_ptr_factory_.GetWeakPtr(), success));
}

void MobileActivator::HandlePortalLoaded(bool success) {
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    ChangeState(nullptr, PlanActivationState::kError,
                ActivationError::kNoCellularService);
    return;
  }
  if (state_ == PlanActivationState::kPaymentPortalLoading ||
      state_ == PlanActivationState::kShowingPayment) {
    if (success) {
      payment_reconnect_count_ = 0;
      ChangeState(network, PlanActivationState::kShowingPayment,
                  ActivationError::kNone);
    } else {
      // There is no point in forcing reconnecting the cellular network if the
      // activation should not be done over it.
      if (network->activation_type() == shill::kActivationTypeNonCellular)
        return;

      payment_reconnect_count_++;
      if (payment_reconnect_count_ > kMaxPortalReconnectCount) {
        ChangeState(nullptr, PlanActivationState::kError,
                    ActivationError::kNoCellularService);
        return;
      }

      // Reconnect and try and load the frame again.
      ChangeState(network, PlanActivationState::kReconnecting,
                  ActivationError::kNone);
    }
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Called paymentPortalLoad while in unexpected state: "
        << GetStateDescription(state_);
  }
}

void MobileActivator::StartOTASPTimer() {
  state_duration_timer_.Start(FROM_HERE, base::Milliseconds(kOTASPRetryDelay),
                              this, &MobileActivator::HandleOTASPTimeout);
}

void MobileActivator::StartActivation() {
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
    ChangeState(nullptr, PlanActivationState::kError, error);
    return;
  }

  // Start monitoring network property changes.
  network_state_handler_observer_.Observe(
      NetworkHandler::Get()->network_state_handler());

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
                  ? PlanActivationState::kDone
                  : PlanActivationState::kPaymentPortalLoading,
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

  ChangeState(network, PlanActivationState::kPaymentPortalLoading,
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
    state_ = PlanActivationState::kStartOTASP;
  } else {
    state_ = PlanActivationState::kStart;
  }

  EvaluateCellularNetwork(network);
}

void MobileActivator::RetryOTASP() {
  DCHECK(state_ == PlanActivationState::kDelayOTASP);
  StartOTASP();
}

void MobileActivator::StartOTASP() {
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  ChangeState(network, PlanActivationState::kStartOTASP,
              ActivationError::kNone);
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
  if (state_ == PlanActivationState::kInitiatingActivation) {
    ++initial_OTASP_attempts_;
    if (initial_OTASP_attempts_ <= kMaxOTASPTries) {
      ChangeState(network, PlanActivationState::kReconnecting,
                  ActivationError::kActivationFailed);
      return;
    }
  } else if (state_ == PlanActivationState::kTryingOTASP) {
    ++trying_OTASP_attempts_;
    if (trying_OTASP_attempts_ <= kMaxOTASPTries) {
      ChangeState(network, PlanActivationState::kReconnecting,
                  ActivationError::kActivationFailed);
      return;
    }
  } else if (state_ == PlanActivationState::kOTASP) {
    ++final_OTASP_attempts_;
    if (final_OTASP_attempts_ <= kMaxOTASPTries) {
      // Give the portal time to propagate all those magic bits.
      ChangeState(network, PlanActivationState::kDelayOTASP,
                  ActivationError::kActivationFailed);
      return;
    }
  } else {
    LOG(ERROR) << "OTASP timed out from a non-OTASP wait state?";
  }
  LOG(ERROR) << "OTASP failed too many times; aborting.";
  ChangeState(network, PlanActivationState::kError,
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
  continue_reconnect_timer_.Start(FROM_HERE,
                                  base::Milliseconds(kReconnectDelayMS), this,
                                  &MobileActivator::ContinueConnecting);
  // If we don't ever connect again, we're going to call this a failure.
  reconnect_timeout_timer_.Stop();
  reconnect_timeout_timer_.Start(FROM_HERE,
                                 base::Milliseconds(kMaxReconnectTime), this,
                                 &MobileActivator::ReconnectTimedOut);
}

void MobileActivator::ReconnectTimedOut() {
  LOG(ERROR) << "Ending activation attempt after failing to reconnect.";
  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  ChangeState(network, PlanActivationState::kError,
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
  if (state_ == PlanActivationState::kPageLoading ||
      state_ == PlanActivationState::kDone ||
      state_ == PlanActivationState::kError) {
    return;
  }

  const NetworkState* network = GetNetworkState(service_path_);
  if (!network) {
    LOG(WARNING) << "Cellular service can't be found: " << service_path_;
    return;
  }

  if (IsSimpleActivationFlow(network)) {
    bool waiting = (state_ == PlanActivationState::kWaitingForConnection);
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
      ChangeState(network, PlanActivationState::kWaitingForConnection,
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
  ActivationError error = new_state == PlanActivationState::kError
                              ? ActivationError::kActivationFailed
                              : ActivationError::kNone;
  ChangeState(network, new_state, error);
}

MobileActivator::PlanActivationState MobileActivator::PickNextState(
    const NetworkState* network) const {
  if (!network->IsConnectedState())
    return PickNextOfflineState(network);
  else
    return PickNextOnlineState(network);
}

MobileActivator::PlanActivationState MobileActivator::PickNextOfflineState(
    const NetworkState* network) const {
  PlanActivationState new_state = state_;
  const std::string& activation = network->activation_state();
  switch (state_) {
    case PlanActivationState::kPaymentPortalLoading:
    case PlanActivationState::kShowingPayment:
      if (!IsSimpleActivationFlow(network))
        new_state = PlanActivationState::kReconnecting;
      break;
    case PlanActivationState::kStart:
      if (activation == shill::kActivationStateActivated) {
        if (NetworkState::StateIsPortalled(network->connection_state()))
          new_state = PlanActivationState::kPaymentPortalLoading;
        else
          new_state = PlanActivationState::kDone;
      } else if (activation == shill::kActivationStatePartiallyActivated) {
        new_state = PlanActivationState::kTryingOTASP;
      } else {
        new_state = PlanActivationState::kInitiatingActivation;
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
    case PlanActivationState::kStart:
      if (activation == shill::kActivationStateActivated) {
        if (network->connection_state() == shill::kStateOnline)
          new_state = PlanActivationState::kDone;
        else
          new_state = PlanActivationState::kPaymentPortalLoading;
      } else if (activation == shill::kActivationStatePartiallyActivated) {
        new_state = PlanActivationState::kTryingOTASP;
      } else {
        new_state = PlanActivationState::kInitiatingActivation;
      }
      break;
    case PlanActivationState::kStartOTASP: {
      if (activation == shill::kActivationStatePartiallyActivated) {
        new_state = PlanActivationState::kOTASP;
      } else if (activation == shill::kActivationStateActivated) {
        new_state = PlanActivationState::kReconnecting;
      } else {
        LOG(WARNING) << "Unexpected activation state for device "
                     << network->path();
      }
      break;
    }
    case PlanActivationState::kDelayOTASP:
      // Just ignore any changes until the OTASP retry timer kicks in.
      break;
    case PlanActivationState::kInitiatingActivation: {
      if (activation == shill::kActivationStateActivated ||
          activation == shill::kActivationStatePartiallyActivated) {
        new_state = PlanActivationState::kStart;
      } else if (activation == shill::kActivationStateNotActivated ||
                 activation == shill::kActivationStateActivating) {
        // Wait in this state until activation state changes.
      } else {
        LOG(WARNING) << "Unknown transition";
      }
      break;
    }
    case PlanActivationState::kOTASP:
    case PlanActivationState::kTryingOTASP:
      if (activation == shill::kActivationStateNotActivated ||
          activation == shill::kActivationStateActivating) {
        VLOG(1) << "Waiting for the OTASP to finish and the service to "
                << "come back online";
      } else if (activation == shill::kActivationStateActivated) {
        new_state = PlanActivationState::kDone;
      } else {
        new_state = PlanActivationState::kPaymentPortalLoading;
      }
      break;
    case PlanActivationState::kReconnectingPayment:
      if (!NetworkState::StateIsPortalled(network->connection_state()) &&
          activation == shill::kActivationStateActivated)
        // We're not portalled, and we're already activated, so we're online!
        new_state = PlanActivationState::kDone;
      else
        new_state = PlanActivationState::kPaymentPortalLoading;
      break;
    // Initial state
    case PlanActivationState::kPageLoading:
      break;
    // Just ignore all signals until the site confirms payment.
    case PlanActivationState::kPaymentPortalLoading:
    case PlanActivationState::kShowingPayment:
    case PlanActivationState::kWaitingForConnection:
      break;
    // Go where we decided earlier.
    case PlanActivationState::kReconnecting:
      new_state = post_reconnect_state_;
      break;
    // Activation completed/failed, ignore network changes.
    case PlanActivationState::kDone:
    case PlanActivationState::kError:
      break;
  }

  return new_state;
}

// Debugging helper function, will take it out at the end.
const char* MobileActivator::GetStateDescription(PlanActivationState state) {
  switch (state) {
    case PlanActivationState::kPageLoading:
      return "PAGE_LOADING";
    case PlanActivationState::kStart:
      return "ACTIVATION_START";
    case PlanActivationState::kInitiatingActivation:
      return "INITIATING_ACTIVATION";
    case PlanActivationState::kTryingOTASP:
      return "TRYING_OTASP";
    case PlanActivationState::kPaymentPortalLoading:
      return "PAYMENT_PORTAL_LOADING";
    case PlanActivationState::kShowingPayment:
      return "SHOWING_PAYMENT";
    case PlanActivationState::kReconnectingPayment:
      return "RECONNECTING_PAYMENT";
    case PlanActivationState::kDelayOTASP:
      return "DELAY_OTASP";
    case PlanActivationState::kStartOTASP:
      return "START_OTASP";
    case PlanActivationState::kOTASP:
      return "OTASP";
    case PlanActivationState::kDone:
      return "DONE";
    case PlanActivationState::kError:
      return "ERROR";
    case PlanActivationState::kReconnecting:
      return "RECONNECTING";
    case PlanActivationState::kWaitingForConnection:
      return "WAITING FOR CONNECTION";
  }
  return "UNKNOWN";
}

void MobileActivator::CompleteActivation() {
  // Remove observers, we are done with this page.
  network_state_handler_observer_.Reset();
}

bool MobileActivator::RunningActivation() const {
  return !(state_ == PlanActivationState::kDone ||
           state_ == PlanActivationState::kError ||
           state_ == PlanActivationState::kPageLoading);
}

void MobileActivator::ChangeState(const NetworkState* network,
                                  PlanActivationState new_state,
                                  ActivationError error) {
  // Report an error, by transitioning into a kError state with
  // a "no service" error instead, if no network state is available (e.g. the
  // cellular service no longer exists) when we are transitioning into certain
  // plan activation state.
  if (!network) {
    switch (new_state) {
      case PlanActivationState::kInitiatingActivation:
      case PlanActivationState::kTryingOTASP:
      case PlanActivationState::kOTASP:
      case PlanActivationState::kDone:
        new_state = PlanActivationState::kError;
        error = ActivationError::kNoCellularService;
        break;
      default:
        break;
    }
  }

  static bool first_time = true;
  VLOG(1) << "Activation state flip old = " << GetStateDescription(state_)
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
    case PlanActivationState::kStart:
      break;
    case PlanActivationState::kDelayOTASP: {
      UMA_HISTOGRAM_COUNTS_1M("Cellular.RetryOTASP", 1);
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&MobileActivator::RetryOTASP,
                         weak_ptr_factory_.GetWeakPtr()),
          base::Milliseconds(kOTASPRetryDelay));
      break;
    }
    case PlanActivationState::kStartOTASP:
      break;
    case PlanActivationState::kInitiatingActivation:
    case PlanActivationState::kTryingOTASP:
    case PlanActivationState::kOTASP:
      // Starts the timer waiting for activation state changes.
      // https://crbug.com/1021688.
      StartOTASPTimer();
      break;
    case PlanActivationState::kPageLoading:
      return;
    case PlanActivationState::kPaymentPortalLoading:
    case PlanActivationState::kShowingPayment:
    case PlanActivationState::kReconnectingPayment:
      // Fix for fix SSL for the walled gardens where cert chain verification
      // might not work.
      break;
    case PlanActivationState::kWaitingForConnection:
      post_reconnect_state_ = old_state;
      break;
    case PlanActivationState::kReconnecting: {
      PlanActivationState next_state = old_state;
      // Pick where we want to return to after we reconnect.
      switch (old_state) {
        case PlanActivationState::kPaymentPortalLoading:
        case PlanActivationState::kShowingPayment:
          // We decide here what to do next based on the state of the modem.
          next_state = PlanActivationState::kReconnectingPayment;
          break;
        case PlanActivationState::kInitiatingActivation:
        case PlanActivationState::kTryingOTASP:
          next_state = PlanActivationState::kStart;
          break;
        case PlanActivationState::kStartOTASP:
        case PlanActivationState::kOTASP:
          if (!network || !network->IsConnectedState()) {
            next_state = PlanActivationState::kStartOTASP;
          } else {
            // We're online, which means we've conspired with
            // PickNextOnlineState to reconnect after activation (that's the
            // only way we see this transition).  Thus, after we reconnect, we
            // should be done.
            next_state = PlanActivationState::kDone;
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
    case PlanActivationState::kDone:
      DCHECK(network);
      CompleteActivation();
      break;
    case PlanActivationState::kError:
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
  return (base::Time::Now() - cellular_plan_payment_time_).InHours() <
         kRecentPlanPaymentHours;
}

}  // namespace ash
