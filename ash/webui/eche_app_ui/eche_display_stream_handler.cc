// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_display_stream_handler.h"

#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace ash {
namespace eche_app {

EcheDisplayStreamHandler::EcheDisplayStreamHandler() = default;

EcheDisplayStreamHandler::~EcheDisplayStreamHandler() = default;

void EcheDisplayStreamHandler::StartStreaming() {
  PA_LOG(INFO) << "echeapi EcheDisplayStreamHandler StartStreaming";
  NotifyStartStreaming();
}

void EcheDisplayStreamHandler::OnStreamStatusChanged(
    mojom::StreamStatus status) {
  PA_LOG(INFO) << "echeapi EcheDisplayStreamHandler OnStreamStatusChanged "
               << status;
  NotifyStreamStatusChanged(status);
}

void EcheDisplayStreamHandler::Bind(
    mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver) {
  display_stream_receiver_.reset();
  display_stream_receiver_.Bind(std::move(receiver));
}

void EcheDisplayStreamHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EcheDisplayStreamHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EcheDisplayStreamHandler::NotifyStartStreaming() {
  for (auto& observer : observer_list_)
    observer.OnStartStreaming();
}

void EcheDisplayStreamHandler::NotifyStreamStatusChanged(
    mojom::StreamStatus status) {
  for (auto& observer : observer_list_)
    observer.OnStreamStatusChanged(status);
}

}  // namespace eche_app
}  // namespace ash
