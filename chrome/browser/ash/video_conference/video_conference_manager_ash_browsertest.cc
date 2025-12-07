// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class FakeVideoConferenceManagerClient
    : public crosapi::mojom::VideoConferenceManagerClient {
 public:
  FakeVideoConferenceManagerClient() = default;
  FakeVideoConferenceManagerClient(const FakeVideoConferenceManagerClient&) =
      delete;
  FakeVideoConferenceManagerClient& operator=(
      const FakeVideoConferenceManagerClient&) = delete;
  ~FakeVideoConferenceManagerClient() override = default;

  // crosapi::mojom::VideoConferenceManagerClient overrides
  void GetMediaApps(GetMediaAppsCallback callback) override {
    std::move(callback).Run(
        std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>());
  }

  void ReturnToApp(const base::UnguessableToken& id,
                   ReturnToAppCallback callback) override {}

  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled,
      SetSystemMediaDeviceStatusCallback callback) override {}

  void StopAllScreenShare() override {}

  base::UnguessableToken id_{base::UnguessableToken::Create()};
};

// Calls all crosapi::mojom::VideoConference methods directly.
void VerifyVideoConferenceManagerAsh(
    FakeVideoConferenceManagerClient& client,
    ash::VideoConferenceManagerAsh* vc_manager) {
  base::test::TestFuture<bool> future1;
  vc_manager->NotifyMediaUsageUpdate(
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          client.id_, true, true, false, false, true, true),
      future1.GetCallback());
  EXPECT_TRUE(future1.Take());

  base::test::TestFuture<bool> future2;
  vc_manager->NotifyDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera, u"Test App",
      future2.GetCallback());
  EXPECT_TRUE(future2.Take());
}

class VideoConferenceManagerAshBrowserTest : public InProcessBrowserTest {
 public:
  VideoConferenceManagerAshBrowserTest() = default;

  VideoConferenceManagerAshBrowserTest(
      const VideoConferenceManagerAshBrowserTest&) = delete;
  VideoConferenceManagerAshBrowserTest& operator=(
      const VideoConferenceManagerAshBrowserTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kFeatureManagementVideoConference);

    InProcessBrowserTest::SetUp();
  }

  ~VideoConferenceManagerAshBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that VideoConferenceManagerAsh API calls do not crash when multiple Cpp
// clients are registered and unregistered.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerAshBrowserTest, Basics) {
  auto* vc_manager = ash::VideoConferenceManagerAsh::Get();
  {
    FakeVideoConferenceManagerClient cpp_client1;
    vc_manager->RegisterCppClient(&cpp_client1, cpp_client1.id_);
    VerifyVideoConferenceManagerAsh(cpp_client1, vc_manager);
    vc_manager->UnregisterClient(cpp_client1.id_);
  }
  {
    FakeVideoConferenceManagerClient cpp_client2;
    vc_manager->RegisterCppClient(&cpp_client2, cpp_client2.id_);
    VerifyVideoConferenceManagerAsh(cpp_client2, vc_manager);
    vc_manager->UnregisterClient(cpp_client2.id_);
  }
}

}  // namespace
}  // namespace ash
