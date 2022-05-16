// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/secure_channel/public/cpp/client/connection_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/services/device_sync/public/cpp/device_sync_client.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"

namespace ash::secure_channel {

namespace {

constexpr base::TimeDelta kConnectionTimeoutSeconds(base::Seconds(15u));

void RecordConnectionSuccessMetric(const std::string& metric_name_result,
                                   bool success) {
  base::UmaHistogramBoolean(metric_name_result, success);
}

}  // namespace

ConnectionManagerImpl::MetricsRecorder::MetricsRecorder(
    ConnectionManager* connection_manager,
    base::Clock* clock,
    const std::string& metric_name_result,
    const std::string& metric_name_latency,
    const std::string& metric_name_duration)
    : connection_manager_(connection_manager),
      status_(connection_manager->GetStatus()),
      clock_(clock),
      status_change_timestamp_(clock_->Now()),
      metric_name_result_(metric_name_result),
      metric_name_latency_(metric_name_latency),
      metric_name_duration_(metric_name_duration) {
  connection_manager_->AddObserver(this);
}

ConnectionManagerImpl::MetricsRecorder::~MetricsRecorder() {
  connection_manager_->RemoveObserver(this);
}

void ConnectionManagerImpl::MetricsRecorder::OnConnectionStatusChanged() {
  const ConnectionManager::Status prev_status = status_;
  status_ = connection_manager_->GetStatus();

  const base::TimeDelta delta = clock_->Now() - status_change_timestamp_;
  status_change_timestamp_ = clock_->Now();

  switch (status_) {
    case ConnectionManager::Status::kConnecting:
      break;

    case ConnectionManager::Status::kDisconnected:
      if (prev_status == ConnectionManager::Status::kConnected) {
        base::UmaHistogramLongTimes100(metric_name_duration_, delta);
      } else if (prev_status == ConnectionManager::Status::kConnecting) {
        RecordConnectionSuccessMetric(metric_name_result_, false);
      }
      break;

    case ConnectionManager::Status::kConnected:
      if (prev_status == ConnectionManager::Status::kConnecting) {
        base::UmaHistogramTimes(metric_name_latency_, delta);
        RecordConnectionSuccessMetric(metric_name_result_, true);
      }
      break;
  }
}

ConnectionManagerImpl::ConnectionManagerImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    device_sync::DeviceSyncClient* device_sync_client,
    SecureChannelClient* secure_channel_client,
    const std::string& feature_name,
    const std::string& metric_name_result,
    const std::string& metric_name_latency,
    const std::string& metric_name_duration)
    : ConnectionManagerImpl(multidevice_setup_client,
                            device_sync_client,
                            secure_channel_client,
                            std::make_unique<base::OneShotTimer>(),
                            feature_name,
                            metric_name_result,
                            metric_name_latency,
                            metric_name_duration,
                            base::DefaultClock::GetInstance()) {}

ConnectionManagerImpl::ConnectionManagerImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    device_sync::DeviceSyncClient* device_sync_client,
    SecureChannelClient* secure_channel_client,
    std::unique_ptr<base::OneShotTimer> timer,
    const std::string& feature_name,
    const std::string& metric_name_result,
    const std::string& metric_name_latency,
    const std::string& metric_name_duration,
    base::Clock* clock)
    : multidevice_setup_client_(multidevice_setup_client),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      timer_(std::move(timer)),
      feature_name_(feature_name),
      metrics_recorder_(
          std::make_unique<MetricsRecorder>(this,
                                            clock,
                                            metric_name_result,
                                            metric_name_latency,
                                            metric_name_duration)) {
  DCHECK(multidevice_setup_client_);
  DCHECK(device_sync_client_);
  DCHECK(secure_channel_client_);
  DCHECK(timer_);
  DCHECK(metrics_recorder_);
}

ConnectionManagerImpl::~ConnectionManagerImpl() {
  metrics_recorder_.reset();
  if (channel_)
    channel_->RemoveObserver(this);
}

ConnectionManager::Status ConnectionManagerImpl::GetStatus() const {
  // Connection attempt was successful and with an active channel between
  // devices.
  if (channel_)
    return Status::kConnected;

  // Initiated an connection attempt and awaiting result.
  if (connection_attempt_)
    return Status::kConnecting;

  // No connection attempt has been made or if either local or host device
  // has disconnected.
  return Status::kDisconnected;
}

void ConnectionManagerImpl::AttemptNearbyConnection() {
  if (GetStatus() != Status::kDisconnected) {
    PA_LOG(WARNING) << "Connection to host already established or is "
                    << "currently attempting to establish, exiting "
                    << "AttemptConnection().";
    return;
  }

  const absl::optional<multidevice::RemoteDeviceRef> remote_device =
      multidevice_setup_client_->GetHostStatus().second;
  const absl::optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();

  if (!remote_device || !local_device) {
    PA_LOG(ERROR) << "AttemptConnection() failed because either remote or "
                  << "local device is null.";
    return;
  }

  connection_attempt_ = secure_channel_client_->InitiateConnectionToDevice(
      *remote_device, *local_device, feature_name_,
      ConnectionMedium::kNearbyConnections, ConnectionPriority::kMedium);
  connection_attempt_->SetDelegate(this);

  PA_LOG(INFO) << "ConnectionManager status updated to: " << GetStatus();
  NotifyStatusChanged();

  timer_->Start(FROM_HERE, kConnectionTimeoutSeconds,
                base::BindOnce(&ConnectionManagerImpl::OnConnectionTimeout,
                               weak_ptr_factory_.GetWeakPtr()));
}

void ConnectionManagerImpl::Disconnect() {
  PA_LOG(INFO) << "ConnectionManager disconnecting connection.";
  TearDownConnection();
}

void ConnectionManagerImpl::SendMessage(const std::string& payload) {
  if (!channel_) {
    PA_LOG(ERROR) << "SendMessage() failed because channel is null.";
    return;
  }

  channel_->SendMessage(payload, base::DoNothing());
}

void ConnectionManagerImpl::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
        file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  if (!channel_) {
    PA_LOG(ERROR) << "RegisterPayloadFile() failed because channel is null.";
    std::move(registration_result_callback).Run(/*success=*/false);
    return;
  }

  channel_->RegisterPayloadFile(payload_id, std::move(payload_files),
                                std::move(file_transfer_update_callback),
                                std::move(registration_result_callback));
}

void ConnectionManagerImpl::GetHostLastSeenTimestamp(
    base::OnceCallback<void(absl::optional<base::Time>)> callback) {
  const absl::optional<multidevice::RemoteDeviceRef> remote_device =
      multidevice_setup_client_->GetHostStatus().second;
  if (!remote_device) {
    std::move(callback).Run(/*timestamp=*/absl::nullopt);
    return;
  }

  secure_channel_client_->GetLastSeenTimestamp(remote_device->GetDeviceId(),
                                               std::move(callback));
}

void ConnectionManagerImpl::OnConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  PA_LOG(WARNING) << "AttemptConnection() failed to establish connection with "
                  << "error: " << reason << ".";
  timer_->Stop();
  connection_attempt_.reset();
  NotifyStatusChanged();
}

void ConnectionManagerImpl::OnConnection(
    std::unique_ptr<ClientChannel> channel) {
  PA_LOG(VERBOSE) << "AttemptConnection() successfully established a "
                  << "connection between local and remote device.";
  timer_->Stop();
  channel_ = std::move(channel);
  channel_->AddObserver(this);
  NotifyStatusChanged();
}

void ConnectionManagerImpl::OnDisconnected() {
  TearDownConnection();
}

void ConnectionManagerImpl::OnMessageReceived(const std::string& payload) {
  NotifyMessageReceived(payload);
}

void ConnectionManagerImpl::OnConnectionTimeout() {
  PA_LOG(WARNING) << "AttemptConnection() has timed out. Closing connection "
                  << "attempt.";

  connection_attempt_.reset();
  NotifyStatusChanged();
}

void ConnectionManagerImpl::TearDownConnection() {
  // Stop timer in case we are disconnected before the connection timed out.
  timer_->Stop();
  connection_attempt_.reset();
  if (channel_)
    channel_->RemoveObserver(this);
  channel_.reset();
  NotifyStatusChanged();
}

}  // namespace ash::secure_channel
