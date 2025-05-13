// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_capture/multi_capture_service.h"

#include "base/logging.h"
#include "url/origin.h"

namespace ash {

MultiCaptureService::MultiCaptureService() = default;

MultiCaptureService::~MultiCaptureService() {
  observers_.Notify(&Observer::MultiCaptureServiceDestroyed);
}

void MultiCaptureService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MultiCaptureService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MultiCaptureService::NotifyMultiCaptureStarted(const std::string& label,
                                                    const url::Origin& origin) {
  observers_.Notify(&Observer::MultiCaptureStarted, label, origin);
}

void MultiCaptureService::NotifyMultiCaptureStartedFromApp(
    const std::string& label,
    const std::string& app_id,
    const std::string& app_short_name,
    const url::Origin& app_origin) {
  observers_.Notify(&Observer::MultiCaptureStartedFromApp, label, app_id,
                    app_short_name, app_origin);
}

void MultiCaptureService::NotifyMultiCaptureStopped(const std::string& label) {
  observers_.Notify(&Observer::MultiCaptureStopped, label);
}

}  // namespace ash
