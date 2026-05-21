// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/test/run_until.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

using TelemetryExtensionManagementApiBrowserTest =
    BaseTelemetryExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioGain) {
  constexpr uint64_t kNodeId = 30054771072;
  ash::FakeCrasAudioClient::Get()->InsertAudioNodeToList(ash::AudioNode(
      /*is_input=*/true,
      /*id=*/kNodeId,
      /*has_v2_stable_device_id=*/false,
      /*stable_device_id_v1=*/0,
      /*stable_device_id_v2=*/0,
      /*device_name=*/std::string(),
      /*type=*/"HEADPHONE",
      /*name=*/std::string(),
      /*active=*/false,
      /*plugged_time=*/0,
      /*max_supported_channels=*/0,
      /*audio_effect=*/0,
      /*number_of_volume_steps=*/0));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return bool(ash::CrasAudioHandler::Get()->GetDeviceFromId(kNodeId));
  }));

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
  constexpr uint64_t kNodeId = 21474836480;
  ash::FakeCrasAudioClient::Get()->InsertAudioNodeToList(ash::AudioNode(
      /*is_input=*/false,
      /*id=*/kNodeId,
      /*has_v2_stable_device_id=*/false,
      /*stable_device_id_v1=*/0,
      /*stable_device_id_v2=*/0,
      /*device_name=*/std::string(),
      /*type=*/"HEADPHONE",
      /*name=*/std::string(),
      /*active=*/false,
      /*plugged_time=*/0,
      /*max_supported_channels=*/0,
      /*audio_effect=*/0,
      /*number_of_volume_steps=*/0));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return bool(ash::CrasAudioHandler::Get()->GetDeviceFromId(kNodeId));
  }));

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
