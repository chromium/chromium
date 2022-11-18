// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
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

  base::UnguessableToken id_{base::UnguessableToken::Create()};
};

// Calls all crosapi::mojom::VideoConference methods over mojo.
void CallVcManagerAshMethods(FakeVcManagerMojoClient& client) {
  base::RunLoop run_loop1;
  client.remote_->NotifyMediaUsageUpdate(
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          client.id_, true, false, true, false, true, false),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop1.Quit();
      }));
  run_loop1.Run();

  base::RunLoop run_loop2;
  client.remote_->NotifyDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera, u"Test App",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

// Calls all crosapi::mojom::VideoConference methods directly.
void CallVcManagerAshMethods(FakeVcManagerCppClient& client,
                             ash::VideoConferenceManagerAsh* vc_manager) {
  base::RunLoop run_loop1;
  vc_manager->NotifyMediaUsageUpdate(
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          client.id_, true, true, false, false, true, true),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop1.Quit();
      }));
  run_loop1.Run();

  base::RunLoop run_loop2;
  vc_manager->NotifyDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera, u"Test App",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

using VideoConferenceAshBrowserTest = InProcessBrowserTest;

// Tests |VideoConferenceManagerAsh| api calls don't crash. Tests calls over
// both mojo and cpp clients.
IN_PROC_BROWSER_TEST_F(VideoConferenceAshBrowserTest, Basics) {
  ASSERT_TRUE(CrosapiManager::IsInitialized());

  auto* vc_manager = crosapi::CrosapiManager::Get()
                         ->crosapi_ash()
                         ->video_conference_manager_ash();

  {
    FakeVcManagerMojoClient mojo_client1;
    vc_manager->BindReceiver(mojo_client1.remote_.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop1;
    mojo_client1.remote_->RegisterMojoClient(
        mojo_client1.receiver_.BindNewPipeAndPassRemote(), mojo_client1.id_,
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop1.Quit();
        }));
    run_loop1.Run();

    FakeVcManagerCppClient cpp_client1;
    vc_manager->RegisterCppClient(&cpp_client1, cpp_client1.id_);

    CallVcManagerAshMethods(mojo_client1);
    CallVcManagerAshMethods(cpp_client1, vc_manager);
  }

  // Disconnect old clients and try again to ensure manager's API doesn't crash
  // after any client disconnects.
  FakeVcManagerMojoClient mojo_client2;
  vc_manager->BindReceiver(mojo_client2.remote_.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop1;
  mojo_client2.remote_->RegisterMojoClient(
      mojo_client2.receiver_.BindNewPipeAndPassRemote(), mojo_client2.id_,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop1.Quit();
      }));
  run_loop1.Run();

  FakeVcManagerCppClient cpp_client2;
  vc_manager->RegisterCppClient(&cpp_client2, cpp_client2.id_);

  CallVcManagerAshMethods(mojo_client2);
  CallVcManagerAshMethods(cpp_client2, vc_manager);
}

}  // namespace
}  // namespace crosapi
