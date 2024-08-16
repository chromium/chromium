// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/fake_kiosk_controller.h"

#include <optional>
#include <vector>

#include "ash/public/cpp/login_accelerators.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chromeos/ash/components/kiosk/vision/internals_page_processor.h"
#include "content/public/browser/web_contents.h"

namespace ash {

std::vector<KioskApp> FakeKioskController::GetApps() const {
  return {};
}
std::optional<KioskApp> FakeKioskController::GetAppById(
    const KioskAppId& app_id) const {
  return std::nullopt;
}
std::optional<KioskApp> FakeKioskController::GetAutoLaunchApp() const {
  return std::nullopt;
}

void FakeKioskController::StartSession(const KioskAppId& app,
                                       bool is_auto_launch,
                                       LoginDisplayHost* host) {}

void FakeKioskController::StartSessionAfterCrash(const KioskAppId& app,
                                                 Profile* profile) {}

bool FakeKioskController::IsSessionStarting() const {
  return false;
}

void FakeKioskController::CancelSessionStart() {}

void FakeKioskController::AddProfileLoadFailedObserver(
    KioskProfileLoadFailedObserver* observer) {}
void FakeKioskController::RemoveProfileLoadFailedObserver(
    KioskProfileLoadFailedObserver* observer) {}

bool FakeKioskController::HandleAccelerator(LoginAcceleratorAction action) {
  return false;
}

void FakeKioskController::OnGuestAdded(
    content::WebContents* guest_web_contents) {}

KioskSystemSession* FakeKioskController::GetKioskSystemSession() {
  return nullptr;
}

kiosk_vision::TelemetryProcessor*
FakeKioskController::GetKioskVisionTelemetryProcessor() {
  return telemetry_processor_;
}

kiosk_vision::InternalsPageProcessor*
FakeKioskController::GetKioskVisionInternalsPageProcessor() {
  return nullptr;
}

void FakeKioskController::SetKioskVisionTelemetryProcessor(
    kiosk_vision::TelemetryProcessor* telemetry_processor) {
  telemetry_processor_ = telemetry_processor;
}

}  // namespace ash
