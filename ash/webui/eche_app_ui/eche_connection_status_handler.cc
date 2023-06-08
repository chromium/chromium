// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"

#include "ash/constants/ash_features.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::eche_app {

EcheConnectionStatusHandler::EcheConnectionStatusHandler() = default;

EcheConnectionStatusHandler::~EcheConnectionStatusHandler() = default;

void EcheConnectionStatusHandler::Observer::OnConnectionStatusChanged(
    mojom::ConnectionStatus connection_status) {}
void EcheConnectionStatusHandler::Observer::OnConnectionStatusForUiChanged(
    mojom::ConnectionStatus connection_status) {}
void EcheConnectionStatusHandler::Observer::
    OnRequestBackgroundConnectionAttempt() {}
void EcheConnectionStatusHandler::Observer::OnRequestCloseConnection() {}
void EcheConnectionStatusHandler::Observer::OnConnectionClosed() {}

void EcheConnectionStatusHandler::OnConnectionStatusChanged(
    mojom::ConnectionStatus connection_status) {
  if (!features::IsEcheNetworkConnectionStateEnabled()) {
    return;
  }

  PA_LOG(INFO) << "echeapi EcheConnectionStatusHandler "
               << " OnConnectionStatusChanged " << connection_status;
  NotifyConnectionStatusChanged(connection_status);

  // Anytime we have a successful connection to the phone (app stream or
  // prewarm) we should make sure the UI is enabled.  Failures triggered from
  // background connection attempts as app stream failures can happen for other
  // reasons, these are updated from EcheTray.
  switch (connection_status) {
    case eche_app::mojom::ConnectionStatus::kConnectionStatusConnected:
      SetConnectionStatusForUi(connection_status);
      [[fallthrough]];
    case eche_app::mojom::ConnectionStatus::kConnectionStatusConnecting:
      is_connecting_or_connected_ = true;
      break;

    case eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected:
    case eche_app::mojom::ConnectionStatus::kConnectionStatusFailed:
      is_connecting_or_connected_ = false;
      break;
  }
}

void EcheConnectionStatusHandler::OnFeatureStatusChanged(
    FeatureStatus feature_status) {
  PA_LOG(INFO) << __func__ << ": " << feature_status;
  feature_status_ = feature_status;
  switch (feature_status) {
    case FeatureStatus::kIneligible:
    case FeatureStatus::kDisabled:
    case FeatureStatus::kDisconnected:
    case FeatureStatus::kDependentFeature:
    case FeatureStatus::kDependentFeaturePending:
      ResetConnectionStatus();
      break;

    case FeatureStatus::kConnecting:
      break;

    case FeatureStatus::kConnected:
      status_check_delay_timer_ = std::make_unique<base::OneShotTimer>();
      status_check_delay_timer_->Start(
          FROM_HERE, base::Seconds(1),
          base::BindOnce(
              &EcheConnectionStatusHandler::CheckConnectionStatusForUi,
              base::Unretained(this)));
      break;
  }
}

void EcheConnectionStatusHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EcheConnectionStatusHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EcheConnectionStatusHandler::SetConnectionStatusForUi(
    mojom::ConnectionStatus connection_status) {
  PA_LOG(INFO) << __func__ << ": " << connection_status;
  last_update_timestamp_ = base::Time::Now();
  if (connection_status_for_ui_ == connection_status) {
    return;
  }
  connection_status_for_ui_ = connection_status;
  NotifyConnectionStatusForUiChanged(connection_status);
}

void EcheConnectionStatusHandler::ResetConnectionStatus() {
  is_connecting_or_connected_ = false;
  last_update_timestamp_ = base::Time();
  connection_status_for_ui_ =
      mojom::ConnectionStatus::kConnectionStatusConnecting;
  NotifyConnectionStatusForUiChanged(connection_status_for_ui_);
}

void EcheConnectionStatusHandler::CheckConnectionStatusForUi() {
  if (feature_status_ != FeatureStatus::kConnected) {
    return;
  }

  if (is_connecting_or_connected_) {
    PA_LOG(INFO)
        << "Already have an active connection (connecting or connected), new "
           "background connection attempt is not required.";
    return;
  }

  if (status_check_delay_timer_) {
    status_check_delay_timer_.reset();
  }

  base::TimeDelta time_since_last_check =
      base::Time::Now() - last_update_timestamp_;
  if (time_since_last_check >
      features::kEcheBackgroundConnectionAttemptThrottleTimeout.Get()) {
    PA_LOG(INFO) << __func__ << ": Requesting background connection attempt";
    NotifyRequestBackgroundConnectionAttempt();
  }
  if (time_since_last_check >
          features::kEcheConnectionStatusResetTimeout.Get() &&
      connection_status_for_ui_ ==
          mojom::ConnectionStatus::kConnectionStatusConnected) {
    PA_LOG(INFO) << __func__ << ": blocking ui";
    connection_status_for_ui_ =
        mojom::ConnectionStatus::kConnectionStatusConnecting;
  }
  NotifyConnectionStatusForUiChanged(connection_status_for_ui_);
}

void EcheConnectionStatusHandler::Bind(
    mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver) {
  connection_status_receiver_.reset();
  connection_status_receiver_.Bind(std::move(receiver));
}

void EcheConnectionStatusHandler::NotifyConnectionStatusChanged(
    mojom::ConnectionStatus connection_status) {
  for (auto& observer : observer_list_) {
    observer.OnConnectionStatusChanged(connection_status);
  }
}

void EcheConnectionStatusHandler::NotifyConnectionStatusForUiChanged(
    mojom::ConnectionStatus connection_status) {
  for (auto& observer : observer_list_) {
    observer.OnConnectionStatusForUiChanged(connection_status);
  }
}

void EcheConnectionStatusHandler::NotifyRequestCloseConnection() {
  for (auto& observer : observer_list_) {
    observer.OnRequestCloseConnection();
    observer.OnConnectionClosed();
  }
}

void EcheConnectionStatusHandler::NotifyConnectionClosed() {
  for (auto& observer : observer_list_) {
    observer.OnConnectionClosed();
  }
}

void EcheConnectionStatusHandler::NotifyRequestBackgroundConnectionAttempt() {
  for (auto& observer : observer_list_) {
    observer.OnRequestBackgroundConnectionAttempt();
  }
}

}  // namespace ash::eche_app
