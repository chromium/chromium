// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/system/video_conference/video_conference_common.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class FakeVideoConferenceManagerAsh : public VideoConferenceManagerAsh {
 public:
  VideoConferenceMediaState& state() { return state_; }

 protected:
  void SendUpdatedState() override { state_ = GetAggregatedState(); }

 private:
  VideoConferenceMediaState state_;
};

class FakeVcManagerCppClient
    : public crosapi::mojom::VideoConferenceManagerClient {
 public:
  explicit FakeVcManagerCppClient(FakeVideoConferenceManagerAsh& vc_manager)
      : id_(base::UnguessableToken::Create()), vc_manager_(vc_manager) {}
  FakeVcManagerCppClient(const FakeVcManagerCppClient&) = delete;
  FakeVcManagerCppClient& operator=(const FakeVcManagerCppClient&) = delete;
  ~FakeVcManagerCppClient() override { vc_manager_->UnregisterClient(id_); }

  // crosapi::mojom::VideoConferenceManagerClient overrides
  void GetMediaApps(
      crosapi::mojom::VideoConferenceManagerClient::GetMediaAppsCallback
          callback) override {
    std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> apps;

    for (auto& app : apps_) {
      apps.push_back(app->Clone());
    }

    std::move(callback).Run(std::move(apps));
  }

  void ReturnToApp(const base::UnguessableToken& id,
                   ReturnToAppCallback callback) override {}

  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled,
      SetSystemMediaDeviceStatusCallback callback) override {}

  void StopAllScreenShare() override {}

  // Public for testing.
  base::UnguessableToken id_;
  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> apps_;
  const raw_ref<FakeVideoConferenceManagerAsh> vc_manager_;
};

class VideoConferenceManagerAshTest : public testing::Test {
 public:
  struct CaptureStatus {
    bool is_capturing_camera = false;
    bool is_capturing_microphone = false;
    bool is_capturing_screen = false;
  };

  FakeVideoConferenceManagerAsh& vc_manager() { return vc_manager_; }

  // Aggregates capturing statuses from all media apps into a |CaptureStatus|
  // and returns it.
  CaptureStatus GetAggregatedCaptureStatus(
      VideoConferenceManagerAsh::MediaApps apps) {
    bool is_capturing_camera = false;
    bool is_capturing_microphone = false;
    bool is_capturing_screen = false;

    for (auto& app : apps) {
      is_capturing_camera |= app->is_capturing_camera;
      is_capturing_microphone |= app->is_capturing_microphone;
      is_capturing_screen |= app->is_capturing_screen;
    }

    return {is_capturing_camera, is_capturing_microphone, is_capturing_screen};
  }

  // Public because this is test code.
  base::test::TaskEnvironment task_environment_;

 private:
  FakeVideoConferenceManagerAsh vc_manager_;
};

// Tests |VideoConferenceManagerAsh::GetMediaApps| returns correct aggregated
// results from all VcClients.
TEST_F(VideoConferenceManagerAshTest, VcManagerGetMediaApps) {
  const auto now = base::Time::Now();
  const auto duration = base::Seconds(1);

  std::unique_ptr<FakeVcManagerCppClient> client1 =
      std::make_unique<FakeVcManagerCppClient>(vc_manager());
  client1->apps_.push_back(crosapi::mojom::VideoConferenceMediaAppInfo::New(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/now,
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/true, /*title=*/u"Test App0",
      /*url=*/std::nullopt));

  vc_manager().RegisterCppClient(client1.get(), client1->id_);

  // Basic functioning.
  base::RunLoop run_loop1;
  vc_manager().GetMediaApps(base::BindLambdaForTesting(
      [&](VideoConferenceManagerAsh::MediaApps apps) {
        EXPECT_EQ(apps.size(), 1u);
        EXPECT_EQ(apps[0]->title, u"Test App0");

        auto status = GetAggregatedCaptureStatus(std::move(apps));

        EXPECT_FALSE(status.is_capturing_camera);
        EXPECT_FALSE(status.is_capturing_microphone);
        EXPECT_TRUE(status.is_capturing_screen);
        run_loop1.Quit();
      }));
  run_loop1.Run();

  client1->apps_.push_back(crosapi::mojom::VideoConferenceMediaAppInfo::New(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/now + duration * 10,
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/true, /*title=*/u"Test App1",
      /*url=*/std::nullopt));

  base::RunLoop run_loop2;
  vc_manager().GetMediaApps(base::BindLambdaForTesting(
      [&](VideoConferenceManagerAsh::MediaApps apps) {
        EXPECT_EQ(apps.size(), 2UL);
        EXPECT_EQ(apps[0]->title, u"Test App1");
        EXPECT_EQ(apps[1]->title, u"Test App0");

        auto status = GetAggregatedCaptureStatus(std::move(apps));

        EXPECT_TRUE(status.is_capturing_camera);
        EXPECT_FALSE(status.is_capturing_microphone);
        EXPECT_TRUE(status.is_capturing_screen);
        run_loop2.Quit();
      }));
  run_loop2.Run();

  // Multiple clients.
  {
    std::unique_ptr<FakeVcManagerCppClient> client2 =
        std::make_unique<FakeVcManagerCppClient>(vc_manager());
    client2->apps_.push_back(crosapi::mojom::VideoConferenceMediaAppInfo::New(
        /*id=*/base::UnguessableToken::Create(),
        /*last_activity_time=*/now + duration * 2,
        /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
        /*is_capturing_screen=*/false, /*title=*/u"Test App2",
        /*url=*/std::nullopt));

    vc_manager().RegisterCppClient(client2.get(), client2->id_);

    base::RunLoop run_loop3;
    vc_manager().GetMediaApps(base::BindLambdaForTesting(
        [&](VideoConferenceManagerAsh::MediaApps apps) {
          EXPECT_EQ(apps.size(), 3UL);
          EXPECT_EQ(apps[0]->title, u"Test App1");
          EXPECT_EQ(apps[1]->title, u"Test App2");
          EXPECT_EQ(apps[2]->title, u"Test App0");

          auto status = GetAggregatedCaptureStatus(std::move(apps));

          EXPECT_TRUE(status.is_capturing_camera);
          EXPECT_TRUE(status.is_capturing_microphone);
          EXPECT_TRUE(status.is_capturing_screen);
          run_loop3.Quit();
        }));
    run_loop3.Run();
  }

  client1->apps_.pop_back();

  // Expect disconnecting clients and modifying internal client state to work
  // correctly.
  base::RunLoop run_loop4;
  vc_manager().GetMediaApps(base::BindLambdaForTesting(
      [&](VideoConferenceManagerAsh::MediaApps apps) {
        EXPECT_EQ(apps.size(), 1u);

        auto status = GetAggregatedCaptureStatus(std::move(apps));

        EXPECT_FALSE(status.is_capturing_camera);
        EXPECT_FALSE(status.is_capturing_microphone);
        EXPECT_TRUE(status.is_capturing_screen);
        run_loop4.Quit();
      }));
  run_loop4.Run();
}

// Tests VcManager state correctly updates after |NotifyMediaUsageUpdate| calls.
TEST_F(VideoConferenceManagerAshTest, VcManagerNotifyMediaUsageUpdate) {
  // crosapi::mojom::VideoConferenceMediaUsageStatus
  std::unique_ptr<FakeVcManagerCppClient> client1 =
      std::make_unique<FakeVcManagerCppClient>(vc_manager());
  vc_manager().RegisterCppClient(client1.get(), client1->id_);

  // Expect initial values are all false.
  EXPECT_FALSE(vc_manager().state().has_media_app);
  EXPECT_FALSE(vc_manager().state().has_camera_permission);
  EXPECT_FALSE(vc_manager().state().has_microphone_permission);
  EXPECT_FALSE(vc_manager().state().is_capturing_camera);
  EXPECT_FALSE(vc_manager().state().is_capturing_microphone);
  EXPECT_FALSE(vc_manager().state().is_capturing_screen);

  auto success_callback =
      base::BindRepeating([](bool success) { EXPECT_TRUE(success); });

  // Basic functioning.

  base::RunLoop run_loop1;
  vc_manager().NotifyMediaUsageUpdate(
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          /*client_id=*/client1->id_, /*has_media_app=*/true,
          /*has_camera_permission=*/false,
          /*has_microphone_permission=*/true, /*is_capturing_camera=*/false,
          /*is_capturing_microphone=*/true, /*is_capturing_screen=*/false),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop1.Quit();
      }));
  run_loop1.Run();

  EXPECT_TRUE(vc_manager().state().has_media_app);
  EXPECT_FALSE(vc_manager().state().has_camera_permission);
  EXPECT_TRUE(vc_manager().state().has_microphone_permission);
  EXPECT_FALSE(vc_manager().state().is_capturing_camera);
  EXPECT_TRUE(vc_manager().state().is_capturing_microphone);
  EXPECT_FALSE(vc_manager().state().is_capturing_screen);

  // Multiple clients.
  {
    std::unique_ptr<FakeVcManagerCppClient> client2 =
        std::make_unique<FakeVcManagerCppClient>(vc_manager());
    vc_manager().RegisterCppClient(client2.get(), client2->id_);

    base::RunLoop run_loop2;
    vc_manager().NotifyMediaUsageUpdate(
        crosapi::mojom::VideoConferenceMediaUsageStatus::New(
            /*client_id=*/client2->id_, /*has_media_app=*/true,
            /*has_camera_permission=*/true,
            /*has_microphone_permission=*/true,
            /*is_capturing_camera=*/true,
            /*is_capturing_microphone=*/true,
            /*is_capturing_screen=*/true),
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop2.Quit();
        }));
    run_loop2.Run();

    EXPECT_TRUE(vc_manager().state().has_media_app);
    EXPECT_TRUE(vc_manager().state().has_camera_permission);
    EXPECT_TRUE(vc_manager().state().has_microphone_permission);
    EXPECT_TRUE(vc_manager().state().is_capturing_camera);
    EXPECT_TRUE(vc_manager().state().is_capturing_microphone);
    EXPECT_TRUE(vc_manager().state().is_capturing_screen);
  }

  // Expect status is updated after a client disconnects.
  EXPECT_TRUE(vc_manager().state().has_media_app);
  EXPECT_FALSE(vc_manager().state().has_camera_permission);
  EXPECT_TRUE(vc_manager().state().has_microphone_permission);
  EXPECT_FALSE(vc_manager().state().is_capturing_camera);
  EXPECT_TRUE(vc_manager().state().is_capturing_microphone);
  EXPECT_FALSE(vc_manager().state().is_capturing_screen);

  // Expect previously true fields are correctly reset on later
  // |NotifyMediaUsageUpdate| calls.
  base::RunLoop run_loop3;
  vc_manager().NotifyMediaUsageUpdate(
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          /*client_id=*/client1->id_, /*has_media_app=*/true,
          /*has_camera_permission=*/false,
          /*has_microphone_permission=*/true, /*is_capturing_camera=*/false,
          /*is_capturing_microphone=*/false,
          /*is_capturing_screen=*/false),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop3.Quit();
      }));
  run_loop3.Run();

  EXPECT_TRUE(vc_manager().state().has_media_app);
  EXPECT_FALSE(vc_manager().state().has_camera_permission);
  EXPECT_TRUE(vc_manager().state().has_microphone_permission);
  EXPECT_FALSE(vc_manager().state().is_capturing_camera);
  EXPECT_FALSE(vc_manager().state().is_capturing_microphone);
  EXPECT_FALSE(vc_manager().state().is_capturing_screen);
}

}  // namespace ash
