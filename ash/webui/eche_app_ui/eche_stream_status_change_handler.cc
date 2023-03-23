// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-shared.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {
namespace eche_app {

EcheStreamStatusChangeHandler::EcheStreamStatusChangeHandler(
    AppsLaunchInfoProvider* apps_launch_info_provider,
    EcheConnectionStatusHandler* eche_connection_status_handler)
    : apps_launch_info_provider_(apps_launch_info_provider),
      eche_connection_status_handler_(eche_connection_status_handler) {
  eche_connection_status_handler_->AddObserver(this);
}

EcheStreamStatusChangeHandler::~EcheStreamStatusChangeHandler() {
  eche_connection_status_handler_->RemoveObserver(this);
}

void EcheStreamStatusChangeHandler::StartStreaming() {
  PA_LOG(INFO) << "echeapi EcheStreamStatusChangeHandler StartStreaming";
  NotifyStartStreaming();
}

void EcheStreamStatusChangeHandler::OnStreamStatusChanged(
    mojom::StreamStatus status) {
  PA_LOG(INFO) << "echeapi EcheStreamStatusChangeHandler OnStreamStatusChanged "
               << status;
  NotifyStreamStatusChanged(status);

  if (status == mojom::StreamStatus::kStreamStatusStarted) {
    if (features::IsEcheNetworkConnectionStateEnabled() &&
        apps_launch_info_provider_->GetConnectionStatusForUi() ==
            mojom::ConnectionStatus::kConnectionStatusFailed &&
        apps_launch_info_provider_->entry_point() ==
            mojom::AppStreamLaunchEntryPoint::NOTIFICATION) {
      base::UmaHistogramEnumeration(
          "Eche.StreamEvent.FromNotification.PreviousNetworkCheckFailed.Result",
          mojom::StreamStatus::kStreamStatusStarted);
    } else {
      base::UmaHistogramEnumeration("Eche.StreamEvent",
                                    mojom::StreamStatus::kStreamStatusStarted);
    }
  }
}

void EcheStreamStatusChangeHandler::SetStreamActionObserver(
    mojo::PendingRemote<mojom::StreamActionObserver> observer) {
  PA_LOG(INFO) << "echeapi EcheDisplayStreamHandler SetStreamActionObserver";
  observer_remote_.reset();
  observer_remote_.Bind(std::move(observer));
}

void EcheStreamStatusChangeHandler::OnRequestCloseConnnection() {
  CloseStream();
}

void EcheStreamStatusChangeHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EcheStreamStatusChangeHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EcheStreamStatusChangeHandler::Bind(
    mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver) {
  display_stream_receiver_.reset();
  display_stream_receiver_.Bind(std::move(receiver));
}

void EcheStreamStatusChangeHandler::CloseStream() {
  if (!observer_remote_.is_bound())
    return;
  observer_remote_->OnStreamAction(mojom::StreamAction::kStreamActionClose);
}

void EcheStreamStatusChangeHandler::StreamGoBack() {
  if (!observer_remote_.is_bound())
    return;
  observer_remote_->OnStreamAction(mojom::StreamAction::kStreamActionGoBack);
}

void EcheStreamStatusChangeHandler::NotifyStartStreaming() {
  for (auto& observer : observer_list_)
    observer.OnStartStreaming();
}

void EcheStreamStatusChangeHandler::NotifyStreamStatusChanged(
    mojom::StreamStatus status) {
  for (auto& observer : observer_list_)
    observer.OnStreamStatusChanged(status);
}

}  // namespace eche_app
}  // namespace ash
