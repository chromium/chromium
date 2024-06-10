// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_FAKE_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_FAKE_KIOSK_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"

namespace ash {

// Fake implementation of the `KioskController`. Instantiating this class
// automatically sets the singleton returned by `KioskController::Get()`.
class FakeKioskController : public KioskController {
 public:
  FakeKioskController() = default;
  ~FakeKioskController() override = default;

  // `KioskController` implementation:
  std::vector<KioskApp> GetApps() const override;
  std::optional<KioskApp> GetAppById(const KioskAppId& app_id) const override;
  std::optional<KioskApp> GetAutoLaunchApp() const override;
  void StartSession(const KioskAppId& app,
                    bool is_auto_launch,
                    LoginDisplayHost* host) override;
  bool IsSessionStarting() const override;
  void CancelSessionStart() override;
  void AddProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer) override;
  void RemoveProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;
  void InitializeKioskSystemSession(
      Profile* profile,
      const KioskAppId& kiosk_app_id,
      const std::optional<std::string>& app_name) override;
  KioskSystemSession* GetKioskSystemSession() override;
  kiosk_vision::TelemetryProcessor* GetKioskVisionTelemetryProcessor() override;

  void SetKioskVisionTelemetryProcessor(
      kiosk_vision::TelemetryProcessor* telemetry_processor);

 private:
  raw_ptr<kiosk_vision::TelemetryProcessor> telemetry_processor_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_FAKE_KIOSK_CONTROLLER_H_
