// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "chromeos/lacros/lacros_service.h"
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

// Calls all crosapi::mojom::VideoConference methods.
void CallVcManagerAshMethods(FakeVcManagerMojoClient& client) {
  base::RunLoop run_loop1;
  client.remote_->RegisterMojoClient(
      client.receiver_.BindNewPipeAndPassRemote(), client.id_,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop1.Quit();
      }));
  run_loop1.Run();

  base::RunLoop run_loop2;
  client.remote_->NotifyMediaUsageUpdate(
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          client.id_, true, false, true, false, true, false),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop2.Quit();
      }));
  run_loop2.Run();

  base::RunLoop run_loop3;
  client.remote_->NotifyDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera, u"Test App",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop3.Quit();
      }));
  run_loop3.Run();
}

using VideoConferenceLacrosBrowserTest = InProcessBrowserTest;

// Tests |VideoConferenceManagerAsh| api calls over mojo don't crash.
IN_PROC_BROWSER_TEST_F(VideoConferenceLacrosBrowserTest, Basics) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(
      lacros_service->IsRegistered<crosapi::mojom::VideoConferenceManager>());

  if (!lacros_service->IsAvailable<crosapi::mojom::VideoConferenceManager>()) {
    GTEST_SKIP();
  }

  FakeVcManagerMojoClient client1;
  lacros_service->BindVideoConferenceManager(
      client1.remote_.BindNewPipeAndPassReceiver());

  {
    FakeVcManagerMojoClient client2;
    lacros_service->BindVideoConferenceManager(
        client2.remote_.BindNewPipeAndPassReceiver());

    // Call and verify that VideoConferenceManagerAsh methods don't crash.
    CallVcManagerAshMethods(client1);
    CallVcManagerAshMethods(client2);
  }

  // Call and verify that VideoConferenceManagerAsh methods don't crash after
  // a client has disconnected.
  FakeVcManagerMojoClient client3;
  lacros_service->BindVideoConferenceManager(
      client3.remote_.BindNewPipeAndPassReceiver());
  CallVcManagerAshMethods(client3);
}

}  // namespace
}  // namespace crosapi
