// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

class FakeVcManagerMojoClient : public mojom::VideoConferenceManagerClient {
 public:
  FakeVcManagerMojoClient() = default;
  FakeVcManagerMojoClient(const FakeVcManagerMojoClient&) = delete;
  FakeVcManagerMojoClient& operator=(const FakeVcManagerMojoClient&) = delete;
  ~FakeVcManagerMojoClient() override = default;

  // crosapi::mojom::VideoConferenceManagerClient overrides
  void GetMediaApps(GetMediaAppsCallback callback) override {
    std::move(callback).Run(
        std::vector<mojom::VideoConferenceMediaAppInfoPtr>());
  }

  void ReturnToApp(const base::UnguessableToken& id,
                   ReturnToAppCallback callback) override {}

  void SetSystemMediaDeviceStatus(
      mojom::VideoConferenceMediaDevice device,
      bool disabled,
      SetSystemMediaDeviceStatusCallback callback) override {}

  void StopAllScreenShare() override {}

  mojo::Receiver<mojom::VideoConferenceManagerClient> receiver_{this};
  mojo::Remote<mojom::VideoConferenceManager> remote_;
  base::UnguessableToken id_{base::UnguessableToken::Create()};
};

class FakeVcManagerCppClient : public mojom::VideoConferenceManagerClient {
 public:
  FakeVcManagerCppClient() = default;
  FakeVcManagerCppClient(const FakeVcManagerCppClient&) = delete;
  FakeVcManagerCppClient& operator=(const FakeVcManagerCppClient&) = delete;
  ~FakeVcManagerCppClient() override = default;

  // crosapi::mojom::VideoConferenceManagerClient overrides
  void GetMediaApps(GetMediaAppsCallback callback) override {
    std::move(callback).Run(
        std::vector<mojom::VideoConferenceMediaAppInfoPtr>());
  }

  void ReturnToApp(const base::UnguessableToken& id,
                   ReturnToAppCallback callback) override {}

  void SetSystemMediaDeviceStatus(
      mojom::VideoConferenceMediaDevice device,
      bool disabled,
      SetSystemMediaDeviceStatusCallback callback) override {}

  void StopAllScreenShare() override {}

  base::UnguessableToken id_{base::UnguessableToken::Create()};
};

// Calls all crosapi::mojom::VideoConference methods over mojo.
void CallVcManagerAshMethods(FakeVcManagerMojoClient& client) {
  base::test::TestFuture<bool> future1;
  client.remote_->NotifyMediaUsageUpdate(
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          client.id_, true, false, true, false, true, false),
      future1.GetCallback());
  EXPECT_TRUE(future1.Take());

  base::test::TestFuture<bool> future2;
  client.remote_->NotifyDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera, u"Test App",
      future2.GetCallback());
  EXPECT_TRUE(future2.Take());
}

// Calls all crosapi::mojom::VideoConference methods directly.
void CallVcManagerAshMethods(FakeVcManagerCppClient& client,
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

class VideoConferenceAshBrowserTest : public InProcessBrowserTest {
 public:
  VideoConferenceAshBrowserTest() = default;

  VideoConferenceAshBrowserTest(const VideoConferenceAshBrowserTest&) = delete;
  VideoConferenceAshBrowserTest& operator=(
      const VideoConferenceAshBrowserTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kFeatureManagementVideoConference);

    InProcessBrowserTest::SetUp();
  }

  ~VideoConferenceAshBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests |VideoConferenceManagerAsh| api calls don't crash. Tests calls over
// both mojo and cpp clients.
IN_PROC_BROWSER_TEST_F(VideoConferenceAshBrowserTest, Basics) {
  ASSERT_TRUE(CrosapiManager::IsInitialized());

  auto* vc_manager =
      CrosapiManager::Get()->crosapi_ash()->video_conference_manager_ash();
  {
    FakeVcManagerMojoClient mojo_client1;
    vc_manager->BindReceiver(mojo_client1.remote_.BindNewPipeAndPassReceiver());

    base::test::TestFuture<bool> future1;
    mojo_client1.remote_->RegisterMojoClient(
        mojo_client1.receiver_.BindNewPipeAndPassRemote(), mojo_client1.id_,
        future1.GetCallback());
    EXPECT_TRUE(future1.Take());

    FakeVcManagerCppClient cpp_client1;
    vc_manager->RegisterCppClient(&cpp_client1, cpp_client1.id_);

    CallVcManagerAshMethods(mojo_client1);
    CallVcManagerAshMethods(cpp_client1, vc_manager);
  }

  // Disconnect old clients and try again to ensure manager's API doesn't crash
  // after any client disconnects.
  FakeVcManagerMojoClient mojo_client2;
  vc_manager->BindReceiver(mojo_client2.remote_.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> future2;
  mojo_client2.remote_->RegisterMojoClient(
      mojo_client2.receiver_.BindNewPipeAndPassRemote(), mojo_client2.id_,
      future2.GetCallback());
  EXPECT_TRUE(future2.Take());

  FakeVcManagerCppClient cpp_client2;
  vc_manager->RegisterCppClient(&cpp_client2, cpp_client2.id_);

  CallVcManagerAshMethods(mojo_client2);
  CallVcManagerAshMethods(cpp_client2, vc_manager);
}

}  // namespace
}  // namespace crosapi
