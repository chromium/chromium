// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/management/fake_telemetry_management_service.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/management/fake_telemetry_management_service_factory.h"
#include "chromeos/ash/components/telemetry_extension/management/telemetry_management_service_ash.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
namespace crosapi = ::crosapi::mojom;
}  // namespace

class TelemetryExtensionManagementApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionManagementApiBrowserTest() {
    ash::TelemetryManagementServiceAsh::Factory::SetForTesting(
        &fake_telemetry_management_service_factory_);
  }

  ~TelemetryExtensionManagementApiBrowserTest() override = default;

  TelemetryExtensionManagementApiBrowserTest(
      const TelemetryExtensionManagementApiBrowserTest&) = delete;
  TelemetryExtensionManagementApiBrowserTest& operator=(
      const TelemetryExtensionManagementApiBrowserTest&) = delete;

  void SetServiceForTesting(std::unique_ptr<FakeTelemetryManagementService>
                                fake_telemetry_management_service_impl) {
    fake_telemetry_management_service_factory_.SetCreateInstanceResponse(
        std::move(fake_telemetry_management_service_impl));
  }

 private:
  FakeTelemetryManagementServiceFactory
      fake_telemetry_management_service_factory_;
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioGain) {
  {
    auto fake_service_impl = std::make_unique<FakeTelemetryManagementService>();
    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function setAudioGain() {
        const result = await chrome.os.management.setAudioGain({
          nodeId: 30054771072,
          gain: 100,
        });
        chrome.test.assertTrue(result);
        chrome.test.succeed();
      }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioVolume) {
  {
    auto fake_service_impl = std::make_unique<FakeTelemetryManagementService>();
    SetServiceForTesting(std::move(fake_service_impl));
  }

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function setAudioVolume() {
        const result = await chrome.os.management.setAudioVolume({
          nodeId: 21474836480,
          volume: 100,
          isMuted: false,
        });
        chrome.test.assertTrue(result);
        chrome.test.succeed();
      }
    ]);
    )");
}

}  // namespace chromeos
