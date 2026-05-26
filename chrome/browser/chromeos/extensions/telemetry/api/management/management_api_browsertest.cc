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

class TelemetryExtensionManagementApiBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionManagementApiBrowserTest() = default;
  ~TelemetryExtensionManagementApiBrowserTest() override = default;

  enum class NodeKind {
    kInput,
    kOutput,
  };
  bool CreateAudioNode(NodeKind node_kind, uint64_t node_id) {
    ash::FakeCrasAudioClient::Get()->InsertAudioNodeToList(ash::AudioNode(
        /*is_input=*/(node_kind == NodeKind::kInput),
        /*id=*/node_id,
        /*has_v2_stable_device_id=*/false,
        /*stable_device_id_v1=*/0,
        /*stable_device_id_v2=*/0,
        /*device_name=*/std::string(),
        /*type=*/"HEADPHONE",
        /*name=*/std::string(),
        /*active=*/true,
        /*plugged_time=*/0,
        /*max_supported_channels=*/0,
        /*audio_effect=*/0,
        /*number_of_volume_steps=*/0));
    return base::test::RunUntil([&]() {
      return bool(ash::CrasAudioHandler::Get()->GetDeviceFromId(node_id));
    });
  }
};

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioGainSuccess) {
  constexpr uint64_t kNodeId = 30054771072;
  ASSERT_TRUE(CreateAudioNode(NodeKind::kInput, kNodeId));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function setAudioGain() {
        const result = await chrome.os.management.setAudioGain({
          nodeId: 30054771072,
          gain: 60,
        });
        chrome.test.assertTrue(result);
        chrome.test.succeed();
      }
    ]);
    )");

  EXPECT_EQ(
      60, ash::CrasAudioHandler::Get()->GetInputGainPercentForDevice(kNodeId));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioGainInvalidGainAboveMax) {
  constexpr uint64_t kNodeId = 30054771072;
  ASSERT_TRUE(CreateAudioNode(NodeKind::kInput, kNodeId));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function setAudioGain() {
        const result = await chrome.os.management.setAudioGain({
          nodeId: 30054771072,
          gain: 999,
        });
        chrome.test.assertTrue(result);
        chrome.test.succeed();
      }
    ]);
    )");

  EXPECT_EQ(
      100,  // Floored to 100.
      ash::CrasAudioHandler::Get()->GetInputGainPercentForDevice(kNodeId));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioGainInvalidGainBelowMin) {
  constexpr uint64_t kNodeId = 30054771072;
  ASSERT_TRUE(CreateAudioNode(NodeKind::kInput, kNodeId));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function setAudioGain() {
        const result = await chrome.os.management.setAudioGain({
          nodeId: 30054771072,
          gain: -100,
        });
        chrome.test.assertTrue(result);
        chrome.test.succeed();
      }
    ]);
    )");

  EXPECT_EQ(
      0,  // Ceiled to 0.
      ash::CrasAudioHandler::Get()->GetInputGainPercentForDevice(kNodeId));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioVolumeSuccess) {
  constexpr uint64_t kNodeId = 21474836480;
  ASSERT_TRUE(CreateAudioNode(NodeKind::kOutput, kNodeId));

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

  EXPECT_EQ(100, ash::CrasAudioHandler::Get()->GetOutputVolumePercentForDevice(
                     kNodeId));
  EXPECT_FALSE(ash::CrasAudioHandler::Get()->IsOutputMutedForDevice(kNodeId));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioVolumeInvalidVolumeAboveMax) {
  constexpr uint64_t kNodeId = 21474836480;
  ASSERT_TRUE(CreateAudioNode(NodeKind::kOutput, kNodeId));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function setAudioVolume() {
        const result = await chrome.os.management.setAudioVolume({
          nodeId: 21474836480,
          volume: 999,
          isMuted: false,
        });
        chrome.test.assertTrue(result);
        chrome.test.succeed();
      }
    ]);
    )");

  EXPECT_EQ(
      100,  // Floored to 100.
      ash::CrasAudioHandler::Get()->GetOutputVolumePercentForDevice(kNodeId));
  EXPECT_FALSE(ash::CrasAudioHandler::Get()->IsOutputMutedForDevice(kNodeId));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioVolumeInvalidVolumeBelowMin) {
  constexpr uint64_t kNodeId = 21474836480;
  ASSERT_TRUE(CreateAudioNode(NodeKind::kOutput, kNodeId));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function setAudioVolume() {
        const result = await chrome.os.management.setAudioVolume({
          nodeId: 21474836480,
          volume: -100,
          isMuted: false,
        });
        chrome.test.assertTrue(result);
        chrome.test.succeed();
      }
    ]);
    )");

  EXPECT_EQ(
      0,  // Ceiled to 100.
      ash::CrasAudioHandler::Get()->GetOutputVolumePercentForDevice(kNodeId));
  EXPECT_FALSE(ash::CrasAudioHandler::Get()->IsOutputMutedForDevice(kNodeId));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionManagementApiBrowserTest,
                       SetAudioVolumeMute) {
  constexpr uint64_t kNodeId = 21474836480;
  ASSERT_TRUE(CreateAudioNode(NodeKind::kOutput, kNodeId));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function setAudioVolume() {
        const result = await chrome.os.management.setAudioVolume({
          nodeId: 21474836480,
          volume: 100,
          isMuted: true,
        });
        chrome.test.assertTrue(result);
        chrome.test.succeed();
      }
    ]);
    )");

  EXPECT_EQ(100, ash::CrasAudioHandler::Get()->GetOutputVolumePercentForDevice(
                     kNodeId));
  EXPECT_TRUE(ash::CrasAudioHandler::Get()->IsOutputMutedForDevice(kNodeId));
}

}  // namespace chromeos
