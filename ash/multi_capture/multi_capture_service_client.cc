// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_capture/multi_capture_service_client.h"

#include "base/logging.h"

namespace ash {

MultiCaptureServiceClient::MultiCaptureServiceClient(
    mojo::PendingRemote<video_capture::mojom::MultiCaptureService>
        multi_capture_service)
    : multi_capture_service_(std::move(multi_capture_service)) {
  multi_capture_service_->AddObserver(
      multi_capture_service_observer_receiver_.BindNewPipeAndPassRemote());
}

MultiCaptureServiceClient::~MultiCaptureServiceClient() {
  for (Observer& observer : observers_)
    observer.MultiCaptureServiceClientDestroyed();
}

void MultiCaptureServiceClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MultiCaptureServiceClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MultiCaptureServiceClient::MultiCaptureStarted(const std::string& label,
                                                    const url::Origin& origin) {
  for (Observer& observer : observers_) {
    observer.MultiCaptureStarted(label, origin);
  }
}

void MultiCaptureServiceClient::MultiCaptureStartedFromApp(
    const std::string& label,
    const std::string& app_id,
    const std::string& app_short_name) {
  for (Observer& observer : observers_) {
    observer.MultiCaptureStartedFromApp(label, app_id, app_short_name);
  }
}

void MultiCaptureServiceClient::MultiCaptureStopped(const std::string& label) {
  for (Observer& observer : observers_) {
    observer.MultiCaptureStopped(label);
  }
}

}  // namespace ash
