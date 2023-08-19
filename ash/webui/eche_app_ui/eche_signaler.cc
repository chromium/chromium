// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_signaler.h"

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-shared.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "ash/webui/eche_app_ui/system_info_provider.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "crypto/sha2.h"

namespace ash {

namespace {

using ConnectionStatus = secure_channel::ConnectionManager::Status;

// From google3: typescript/webrtc/webrtc_peer_connection.ts
constexpr base::TimeDelta kSignalingTimeoutDuration = base::Milliseconds(10000);

EcheTray* GetEcheTray() {
  return Shell::GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->eche_tray();
}

}  // namespace

namespace eche_app {

EcheSignaler::EcheSignaler(
    EcheConnector* eche_connector,
    secure_channel::ConnectionManager* connection_manager,
    AppsLaunchInfoProvider* apps_launch_info_provider,
    EcheConnectionStatusHandler* eche_connection_status_handler)
    : eche_connector_(eche_connector),
      apps_launch_info_provider_(apps_launch_info_provider),
      eche_connection_status_handler_(eche_connection_status_handler),
      connection_manager_(connection_manager) {
  connection_manager_->AddObserver(this);
  eche_connection_status_handler_->AddObserver(this);
}

EcheSignaler::~EcheSignaler() {
  eche_connection_status_handler_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
  signaling_timeout_timer_.reset();
}

void EcheSignaler::SendSignalingMessage(const std::vector<uint8_t>& signal) {
  PA_LOG(INFO) << "echeapi EcheSignaler SendSignalingMessage";
  std::string encoded_signal(signal.begin(), signal.end());
  proto::SignalingRequest request;
  request.set_data(encoded_signal);
  proto::ExoMessage message;
  *message.mutable_request() = std::move(request);
  eche_connector_->SendMessage(message);

  if (!signaling_timeout_timer_) {
    signaling_timeout_timer_ = std::make_unique<base::DelayTimer>(
        FROM_HERE, kSignalingTimeoutDuration, this,
        &EcheSignaler::RecordSignalingTimeout);
    signaling_timeout_timer_->Reset();
  }
}

void EcheSignaler::SetSignalingMessageObserver(
    mojo::PendingRemote<mojom::SignalingMessageObserver> observer) {
  PA_LOG(INFO) << "echeapi EcheSignaler SetSignalingMessageObserver";
  observer_.reset();
  observer_.Bind(std::move(observer));
  signaling_timeout_timer_.reset();
  probably_connection_failed_reason_ = EcheTray::ConnectionFailReason::kUnknown;
}

void EcheSignaler::SetSystemInfoProvider(
    SystemInfoProvider* system_info_provider) {
  PA_LOG(INFO) << "echeapi EcheSignaler SetSystemInfoProvider";
  system_info_provider_ = system_info_provider;
}

void EcheSignaler::TearDownSignaling() {
  PA_LOG(INFO) << "echeapi EcheSignaler TearDownSignaling";
  proto::SignalingAction action;
  action.set_action_type(proto::ActionType::ACTION_TEAR_DOWN);
  proto::ExoMessage message;
  *message.mutable_action() = std::move(action);
  eche_connector_->SendMessage(message);
  signaling_timeout_timer_.reset();
  probably_connection_failed_reason_ = EcheTray::ConnectionFailReason::kUnknown;
}

void EcheSignaler::Bind(
    mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver) {
  exchanger_.reset();
  exchanger_.Bind(std::move(receiver));
}

void EcheSignaler::OnMessageReceived(const std::string& payload) {
  if (!observer_.is_bound())
    return;

  proto::ExoMessage message;
  message.ParseFromString(payload);
  std::string signal;
  if (message.has_request()) {
    PA_LOG(INFO) << "echeapi EcheSignaler OnMessageReceived has request";
    signal = message.request().data();
    probably_connection_failed_reason_ =
        EcheTray::ConnectionFailReason::kSignalingHasLateRequest;
  } else if (message.has_response()) {
    PA_LOG(INFO) << "echeapi EcheSignaler OnMessageReceived has response";
    signal = message.response().data();

    if (base::FeatureList::IsEnabled(
            features::kEcheSWACheckAndroidNetworkInfo)) {
      ProcessAndroidNetworkInfo(message);
    } else {
      probably_connection_failed_reason_ =
          EcheTray::ConnectionFailReason::kSignalingHasLateResponse;
    }

  } else {
    PA_LOG(INFO) << "echeapi EcheSignaler OnMessageReceived return";
    return;
  }
  PA_LOG(INFO) << "echeapi EcheSignaler OnMessageReceived";
  std::vector<uint8_t> encoded_signal(signal.begin(), signal.end());
  observer_->OnReceivedSignalingMessage(encoded_signal);
}

void EcheSignaler::ProcessAndroidNetworkInfo(const proto::ExoMessage& message) {
  if (!message.response().has_network_info()) {
    probably_connection_failed_reason_ =
        EcheTray::ConnectionFailReason::kSignalingHasLateResponse;
    return;
  }

  system_info_provider_->FetchWifiNetworkSsidHash();

  const proto::NetworkInfo& network_info = message.response().network_info();

  bool remote_on_cellular = network_info.mobile_network();
  bool is_different_network = false;

  if (!remote_on_cellular) {
    std::string local_hashed_ssid = system_info_provider_->GetHashedWiFiSsid();
    std::string remote_hashed_ssid = network_info.ssid();

    if (local_hashed_ssid.compare(remote_hashed_ssid) == 0) {
      probably_connection_failed_reason_ =
          EcheTray::ConnectionFailReason::kSignalingHasLateResponse;
    } else {
      is_different_network = true;
      probably_connection_failed_reason_ =
          EcheTray::ConnectionFailReason::kConnectionFailSsidDifferent;
    }
  } else {
    probably_connection_failed_reason_ =
        EcheTray::ConnectionFailReason::kConnectionFailRemoteDeviceOnCellular;
  }

  system_info_provider_->SetAndroidDeviceNetworkInfoChanged(
      is_different_network, remote_on_cellular);
}

void EcheSignaler::OnConnectionClosed() {
  // When a background connection is closed, reset the timer so no data will be
  // recoreded into ConnectionFail bucket.
  signaling_timeout_timer_.reset();
}

void EcheSignaler::RecordSignalingTimeout() {
  if (connection_manager_->GetStatus() == ConnectionStatus::kDisconnected) {
    probably_connection_failed_reason_ =
        EcheTray::ConnectionFailReason::kSecurityChannelDisconnected;
  } else if (probably_connection_failed_reason_ ==
             EcheTray::ConnectionFailReason::kUnknown) {
    probably_connection_failed_reason_ =
        EcheTray::ConnectionFailReason::kSignalingNotTriggered;
  }

  PA_LOG(INFO) << "echeapi EcheSignaler timeout: "
               << probably_connection_failed_reason_;
  if (!features::IsEcheNetworkConnectionStateEnabled()) {
    base::UmaHistogramEnumeration("Eche.StreamEvent.ConnectionFail",
                                  probably_connection_failed_reason_);
    return;
  }

  EcheTray* eche_tray = GetEcheTray();
  if (eche_tray && eche_tray->IsBackgroundConnectionAttemptInProgress()) {
    base::UmaHistogramEnumeration("Eche.NetworkCheck.FailureReason",
                                  probably_connection_failed_reason_);
  } else if (apps_launch_info_provider_->GetConnectionStatusFromLastAttempt() ==
                 mojom::ConnectionStatus::kConnectionStatusFailed &&
             apps_launch_info_provider_->entry_point() ==
                 mojom::AppStreamLaunchEntryPoint::NOTIFICATION) {
    base::UmaHistogramEnumeration(
        "Eche.StreamEvent.FromNotification.PreviousNetworkCheckFailed."
        "ConnectionFail",
        probably_connection_failed_reason_);
  } else {
    base::UmaHistogramEnumeration("Eche.StreamEvent.ConnectionFail",
                                  probably_connection_failed_reason_);
  }
}

std::ostream& operator<<(
    std::ostream& stream,
    EcheTray::ConnectionFailReason connection_fail_reason) {
  switch (connection_fail_reason) {
    case EcheTray::ConnectionFailReason::kUnknown:
      stream << "[Unknown]";
      break;
    case EcheTray::ConnectionFailReason::kSignalingNotTriggered:
      stream << "[Signaling Not Triggered]";
      break;
    case EcheTray::ConnectionFailReason::kSignalingHasLateResponse:
      stream << "[Signaling Has Late Response]";
      break;
    case EcheTray::ConnectionFailReason::kSignalingHasLateRequest:
      stream << "[Signaling Has Late Request]";
      break;
    case EcheTray::ConnectionFailReason::kSecurityChannelDisconnected:
      stream << "[Security Channel Disconnected]";
      break;
    case EcheTray::ConnectionFailReason::kConnectionFailInTabletMode:
      stream << "[Connection Fail In Tablet Mode]";
      break;
    case EcheTray::ConnectionFailReason::kConnectionFailSsidDifferent:
      stream << "[Connection Fail Different Network]";
      break;
    case EcheTray::ConnectionFailReason::kConnectionFailRemoteDeviceOnCellular:
      stream << "[Connection Fail Remote Device On Cellular]";
      break;
  }
  return stream;
}

}  // namespace eche_app
}  // namespace ash
