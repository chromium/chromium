// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_presence_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/eche_connector.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client.h"

namespace ash::eche_app {

namespace {

// How often to check whether the last seen time is below the maximum age.
constexpr base::TimeDelta kTimerInterval = base::Seconds(30);
// The maximum age of the last seen time before the connection must be
// terminated.
constexpr base::TimeDelta kMaximumLastSeenAge = base::Minutes(5);

}  // namespace

EchePresenceManager::EchePresenceManager(
    FeatureStatusProvider* eche_feature_status_provider,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    std::unique_ptr<secure_channel::PresenceMonitorClient>
        presence_monitor_client,
    EcheConnector* eche_connector,
    EcheMessageReceiver* eche_message_receiver)
    : eche_feature_status_provider_(eche_feature_status_provider),
      device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client),
      presence_monitor_client_(std::move(presence_monitor_client)),
      eche_connector_(eche_connector),
      eche_message_receiver_(eche_message_receiver) {
  eche_feature_status_provider_->AddObserver(this);
  eche_message_receiver_->AddObserver(this);
  presence_monitor_client_->SetPresenceMonitorCallbacks(
      base::BindRepeating(&EchePresenceManager::OnReady,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&EchePresenceManager::OnDeviceSeen,
                          weak_ptr_factory_.GetWeakPtr()));
}

EchePresenceManager::~EchePresenceManager() {
  eche_feature_status_provider_->RemoveObserver(this);
}

void EchePresenceManager::OnFeatureStatusChanged() {
  UpdateMonitoringStatus();
}

void EchePresenceManager::OnStatusChange(
    proto::StatusChangeType status_change_type) {
  PA_LOG(INFO) << "Stream status changed";
  stream_running_ =
      status_change_type == proto::StatusChangeType::TYPE_STREAM_START;
  UpdateMonitoringStatus();
}

void EchePresenceManager::OnReady() {
  UpdateMonitoringStatus();
}

void EchePresenceManager::UpdateMonitoringStatus() {
  const FeatureStatus feature_status =
      eche_feature_status_provider_->GetStatus();
  switch (feature_status) {
    case FeatureStatus::kIneligible:
      ABSL_FALLTHROUGH_INTENDED;
    case FeatureStatus::kDisabled:
      ABSL_FALLTHROUGH_INTENDED;
    case FeatureStatus::kDependentFeature:
      ABSL_FALLTHROUGH_INTENDED;
    case FeatureStatus::kDependentFeaturePending:
      ABSL_FALLTHROUGH_INTENDED;
    case FeatureStatus::kDisconnected:
      stream_running_ = false;
      ABSL_FALLTHROUGH_INTENDED;
    case FeatureStatus::kConnecting:
      StopMonitoring();
      break;

    case FeatureStatus::kConnected:
      if (stream_running_) {
        InitializeMonitoring();
      } else {
        StopMonitoring();
      }
      break;
  }
}

void EchePresenceManager::InitializeMonitoring() {
  if (is_monitoring_) {
    return;
  }

  // Assume a successful proximity check at the beginning. It gives us a cushon
  // in case the first one or two pings get lost.
  device_last_seen_time_ = base::TimeTicks::Now();

  StartMonitoring();
}

void EchePresenceManager::StartMonitoring() {
  if (is_monitoring_) {
    return;
  }

  const std::optional<multidevice::RemoteDeviceRef> remote_device_ref =
      multidevice_setup_client_->GetHostStatus().second;
  const std::optional<multidevice::RemoteDeviceRef> local_device_ref =
      device_sync_client_->GetLocalDeviceMetadata();
  if (!remote_device_ref || !local_device_ref) {
    return;
  }

  presence_monitor_client_->StartMonitoring(remote_device_ref.value(),
                                            local_device_ref.value());

  is_monitoring_ = true;

  if (features::IsEcheShorterScanningDutyCycleEnabled()) {
    if (shorter_duty_cycle_timer_.IsRunning()) {
      // We cannot simply reset the timer here because the timer could be used
      // to restart the monitoring.
      shorter_duty_cycle_timer_.Stop();
    }

    shorter_duty_cycle_timer_.Start(
        FROM_HERE, features::kEcheScanningCycleOnTime.Get(),
        base::BindRepeating(&EchePresenceManager::OnTimerExpired,
                            weak_ptr_factory_.GetWeakPtr()));

  } else {
    if (timer_.IsRunning()) {
      timer_.Reset();
    } else {
      timer_.Start(FROM_HERE, kTimerInterval,
                   base::BindRepeating(&EchePresenceManager::OnTimerExpired,
                                       weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void EchePresenceManager::StopMonitoring() {
  if (!is_monitoring_) {
    return;
  }

  if (features::IsEcheShorterScanningDutyCycleEnabled()) {
    shorter_duty_cycle_timer_.Stop();
  } else {
    timer_.Stop();
  }

  presence_monitor_client_->StopMonitoring();
  is_monitoring_ = false;
}

void EchePresenceManager::OnTimerExpired() {
  if ((base::TimeTicks::Now() - device_last_seen_time_) >=
      kMaximumLastSeenAge) {
    PA_LOG(INFO) << "Proximity has not been maintained; stopping monitoring";
    StopMonitoring();
  } else {
    PA_LOG(INFO) << "Proximity has been maintained; sending ping";
    proto::ProximityPing ping;
    proto::ExoMessage message;
    *message.mutable_proximity_ping() = std::move(ping);
    eche_connector_->SendMessage(message);

    if (features::IsEcheShorterScanningDutyCycleEnabled()) {
      PA_LOG(INFO)
          << "Stopping persistent monitoring, restarting after timeout";
      StopMonitoring();
      shorter_duty_cycle_timer_.Start(
          FROM_HERE, features::kEcheScanningCycleOffTime.Get(),
          base::BindOnce(&EchePresenceManager::StartMonitoring,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void EchePresenceManager::OnDeviceSeen() {
  // It is the responsibility of the scanner to ensure outdated advertisements
  // are not forwarded through, so we will treat all received advertisements as
  // valid.
  PA_LOG(INFO) << "Device advertisement found. Updating device last seen time";
  device_last_seen_time_ = base::TimeTicks::Now();
}

}  // namespace ash::eche_app
