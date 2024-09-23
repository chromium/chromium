// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_app_service_client.h"

#include <cstdlib>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/test/test_window_builder.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/chromeos/video_conference/video_conference_ukm_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

using AppIdString = std::string;
using UkmEntry = ukm::builders::VideoConferencingEvent;

constexpr char kAppId1[] = "random_app_id_1";
constexpr char kAppName1[] = "random_app_name_1";
constexpr char kAppId2[] = "random_app_id_2";
constexpr char kAppName2[] = "random_app_name_2";

// Creates an app with given id and permissions.
apps::AppPtr MakeApp(const AppIdString& app_id,
                     bool has_camera_permission,
                     bool has_microphone_permission,
                     apps::AppType app_type) {
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  if (app_id == kAppId1) {
    app->name = kAppName1;
  }
  if (app_id == kAppId2) {
    app->name = kAppName2;
  }
  if (base::Contains(::video_conference::kSkipAppIds, app_id)) {
    app->name = base::StrCat({"AppName-", app_id});
  }

  app->publisher_id = base::StrCat({"PublisherId-", app_id});

  // Set camera_permission_value as apps::TriState (kAsk only for Arc++) for
  // better coverage.
  apps::TriState camera_permission_state =
      !has_camera_permission            ? apps::TriState::kBlock
      : app_type == apps::AppType::kArc ? apps::TriState::kAsk
                                        : apps::TriState::kAllow;

  app->permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kCamera, camera_permission_state,
      /*is_managed=*/false));
  app->permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kMicrophone, has_microphone_permission,
      /*is_managed=*/false));
  return app;
}

// This fake instance class simulates the creation, destruction, hiding and
// showing an app instance.
class FakeAppInstance {
 public:
  FakeAppInstance(apps::InstanceRegistry* instance_registry,
                  const AppIdString& app_id) {
    instance_registry_ = instance_registry;
    window_ = TestWindowBuilder().Build();
    instance_ = std::make_unique<apps::Instance>(
        app_id, base::UnguessableToken::Create(), window_.get());
  }

  aura::Window* window() { return window_.get(); }

  void Start() {
    auto instance = instance_->Clone();
    instance->UpdateState(apps::InstanceState::kStarted, base::Time::Now());
    instance_registry_->OnInstance(std::move(instance));
  }

  void Close() {
    auto instance = instance_->Clone();
    instance->UpdateState(apps::InstanceState::kDestroyed, base::Time::Now());
    instance_registry_->OnInstance(std::move(instance));
  }

  void Show() {
    window_->Show();
    // Ideally, the following should be automatically triggered by showing the
    // window_; but that is not the case for now.
    auto instance = instance_->Clone();
    instance->UpdateState(apps::InstanceState::kActive, base::Time::Now());
    instance_registry_->OnInstance(std::move(instance));
  }

  void Hide() {
    window_->Hide();
    // Ideally, the following should be automatically triggered by hiding the
    // window_; but that is not the case for now.
    auto instance = instance_->Clone();
    instance->UpdateState(apps::InstanceState::kHidden, base::Time::Now());
    instance_registry_->OnInstance(std::move(instance));
  }

 private:
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<apps::Instance> instance_;
  raw_ptr<apps::InstanceRegistry> instance_registry_;
};

}  // namespace

class VideoConferenceAppServiceClientTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kFeatureManagementVideoConference);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    client_ = VideoConferenceAppServiceClient::GetForTesting();

    test_ukm_recorder_ = std::make_unique<ukm::TestUkmRecorder>();
    client_->test_ukm_recorder_ = test_ukm_recorder_.get();

    Profile* profile = ProfileManager::GetActiveUserProfile();
    app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile);
    instance_registry_ = &app_service_proxy_->InstanceRegistry();
    capability_cache_ =
        apps::AppCapabilityAccessCacheWrapper::Get()
            .GetAppCapabilityAccessCache(user_manager::UserManager::Get()
                                             ->GetActiveUser()
                                             ->GetAccountId());
  }

  // This function creates an app with given id and name, and adds the app into
  // AppRegistryCache of current profile.
  void InstallApp(const std::string& app_id,
                  apps::AppType app_type = apps::AppType::kArc) {
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(MakeApp(app_id, /*has_camera_permission=*/false,
                             /*has_microphone_permission=*/false, app_type));
    app_service_proxy_->OnApps(std::move(deltas), apps::AppType::kUnknown,
                               /*should_notify_initialized=*/false);
  }

  // Update the permission of current `app_id`.
  void UpdateAppPermision(const std::string& app_id,
                          bool has_camera_permission,
                          bool has_microphone_permission) {
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(MakeApp(app_id, has_camera_permission,
                             has_microphone_permission, GetAppType(app_id)));
    app_service_proxy_->OnApps(std::move(deltas), apps::AppType::kUnknown,
                               /*should_notify_initialized=*/false);
  }

  // Set the camera/michrophone accessing info for app with `app_id`.
  void SetAppCapabilityAccess(const AppIdString& app_id,
                              bool is_capturing_camera,
                              bool is_capturing_microphone) {
    auto delta = std::make_unique<apps::CapabilityAccess>(app_id);
    delta->camera = is_capturing_camera;
    delta->microphone = is_capturing_microphone;

    std::vector<apps::CapabilityAccessPtr> deltas;
    deltas.push_back(std::move(delta));

    capability_cache_->OnCapabilityAccesses(std::move(deltas));
  }

  // Adds {id, state} pair to client_->id_to_app_state_.
  void AddAppState(const AppIdString& app_id,
                   const VideoConferenceAppServiceClient::AppState& state) {
    (client_->id_to_app_state_)[app_id] = state;
  }

  std::string GetAppName(const AppIdString& app_id) {
    return client_->GetAppName(app_id);
  }

  apps::AppType GetAppType(const AppIdString& app_id) {
    return client_->GetAppType(app_id);
  }

  VideoConferenceAppServiceClient::VideoConferencePermissions GetAppPermission(
      const AppIdString& app_id) {
    return client_->GetAppPermission(app_id);
  }

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> GetMediaApps() {
    std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

    client_->GetMediaApps(base::BindLambdaForTesting(
        [&media_app_info](
            std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>
                result) { media_app_info = std::move(result); }));

    return media_app_info;
  }

  // Returns current VideoConferenceMediaState in the VideoConferenceManagerAsh
  VideoConferenceMediaState GetMediaStateInVideoConferenceManagerAsh() {
    return crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->video_conference_manager_ash()
        ->GetAggregatedState();
  }

 protected:
  raw_ptr<apps::AppServiceProxy, DanglingUntriaged> app_service_proxy_ =
      nullptr;
  raw_ptr<apps::InstanceRegistry, DanglingUntriaged> instance_registry_ =
      nullptr;
  raw_ptr<apps::AppCapabilityAccessCache, DanglingUntriaged> capability_cache_ =
      nullptr;
  raw_ptr<VideoConferenceAppServiceClient, DanglingUntriaged> client_ = nullptr;
  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, GetAppName) {
  // AppName should be empty if it is not installed.
  EXPECT_EQ(GetAppName(kAppId1), std::string());

  InstallApp(kAppId1);

  // AppName should be correct if installed.
  EXPECT_EQ(GetAppName(kAppId1), kAppName1);
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, GetAppType) {
  // AppType should be kUnknown if it is not installed.
  EXPECT_EQ(GetAppType(kAppId1), apps::AppType::kUnknown);

  InstallApp(kAppId1);

  // AppType for the test app installed should be Arc.
  EXPECT_EQ(GetAppType(kAppId1), apps::AppType::kArc);
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, GetAppPermission) {
  InstallApp(kAppId1);

  VideoConferenceAppServiceClient::VideoConferencePermissions permission =
      GetAppPermission(kAppId1);
  EXPECT_FALSE(permission.has_camera_permission);
  EXPECT_FALSE(permission.has_microphone_permission);

  UpdateAppPermision(kAppId1, /*has_camera_permission=*/false,
                     /*has_microphone_permission=*/true);
  permission = GetAppPermission(kAppId1);
  EXPECT_FALSE(permission.has_camera_permission);
  EXPECT_TRUE(permission.has_microphone_permission);

  UpdateAppPermision(kAppId1, /*has_camera_permission=*/true,
                     /*has_microphone_permission=*/true);
  permission = GetAppPermission(kAppId1);
  EXPECT_TRUE(permission.has_camera_permission);
  EXPECT_TRUE(permission.has_microphone_permission);

  UpdateAppPermision(kAppId1, /*has_camera_permission=*/true,
                     /*has_microphone_permission=*/false);
  permission = GetAppPermission(kAppId1);
  EXPECT_TRUE(permission.has_camera_permission);
  EXPECT_FALSE(permission.has_microphone_permission);

  UpdateAppPermision(kAppId1, /*has_camera_permission=*/false,
                     /*has_microphone_permission=*/false);
  permission = GetAppPermission(kAppId1);
  EXPECT_FALSE(permission.has_camera_permission);
  EXPECT_FALSE(permission.has_microphone_permission);
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, GetMediaApps) {
  // Add {kAppId1, state1} pair to the client_.
  const base::UnguessableToken token1 = base::UnguessableToken::Create();
  const VideoConferenceAppServiceClient::AppState state1{
      token1, base::Time::Now(), true, true};
  AddAppState(kAppId1, state1);

  // Add {kAppId2, state2} pair to the client_.
  const base::UnguessableToken token2 = base::UnguessableToken::Create();
  const VideoConferenceAppServiceClient::AppState state2{
      token2, base::Time::Now(), true, false};
  AddAppState(kAppId2, state2);

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info =
      GetMediaApps();

  // GetMediaApps will not return anything because unrecognized apps will be
  // skipped.
  EXPECT_TRUE(media_app_info.empty());

  InstallApp(kAppId1);

  // GetMediaApps should return kAppId1 since it is installed.
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);

  crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
      crosapi::mojom::VideoConferenceMediaAppInfo::New(
          /*id=*/token1,
          /*last_activity_time=*/state1.last_activity_time,
          /*is_capturing_camera=*/state1.is_capturing_camera,
          /*is_capturing_microphone=*/state1.is_capturing_microphone,
          /*is_capturing_screen=*/false,
          /*title=*/base::UTF8ToUTF16(std::string(kAppName1)),
          /*url=*/std::nullopt,
          /*app_type=*/crosapi::mojom::VideoConferenceAppType::kArcApp);

  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, ReturnToApp) {
  // Add two instance for kAppId1.
  FakeAppInstance instance1(instance_registry_, kAppId1);
  instance1.Start();
  FakeAppInstance instance2(instance_registry_, kAppId1);
  instance2.Start();

  aura::Window* window1 = instance1.window();
  aura::Window* window2 = instance2.window();
  window1->Hide();
  window2->Hide();

  const base::UnguessableToken token1 = base::UnguessableToken::Create();
  bool reactivated_app = false;

  // Return to token1 should not do anything since the token1 is not in the
  // client_->id_to_app_state_.
  client_->ReturnToApp(
      token1, base::BindLambdaForTesting([&reactivated_app](bool result) {
        reactivated_app = result;
      }));

  EXPECT_FALSE(reactivated_app);
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());

  // Add pair {token1, state1} to client_->id_to_app_state_.
  const VideoConferenceAppServiceClient::AppState state1{
      token1, base::Time::Now(), true, true};
  AddAppState(kAppId1, state1);

  // Return to token1 should show all instances associated with kAppId1.
  client_->ReturnToApp(
      token1, base::BindLambdaForTesting([&reactivated_app](bool result) {
        reactivated_app = result;
      }));

  EXPECT_TRUE(reactivated_app);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, MediaCapturing) {
  // Install two apps so that they will can be tracked inside GetMediaApps.
  InstallApp(kAppId1);
  InstallApp(kAppId2);
  FakeAppInstance instance1(instance_registry_, kAppId1);
  instance1.Start();
  FakeAppInstance instance2(instance_registry_, kAppId2);
  instance2.Start();

  // no-camera, no-mic should not start a tracking of the app.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/false);
  EXPECT_TRUE(GetMediaApps().empty());

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

  // has-camera, no-mic should start the tracking of the app.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/false);
  media_app_info = GetMediaApps();
  crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
      crosapi::mojom::VideoConferenceMediaAppInfo::New(
          /*id=*/media_app_info[0]->id,
          /*last_activity_time=*/media_app_info[0]->last_activity_time,
          /*is_capturing_camera=*/true,
          /*is_capturing_microphone=*/false,
          /*is_capturing_screen=*/false,
          /*title=*/media_app_info[0]->title, /*url=*/std::nullopt,
          /*app_type=*/crosapi::mojom::VideoConferenceAppType::kArcApp);
  ASSERT_EQ(media_app_info.size(), 1u);
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // has-camera, has-mic should change the value of GetMediaApps.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/true);
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  expected_media_app_info->is_capturing_microphone = true;
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // no-camera, has-mic should change the value of GetMediaApps.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/true);
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  expected_media_app_info->is_capturing_camera = false;
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // no-camera, no-mic should change the value of GetMediaApps; but not removing
  // the tracking app.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/false);
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  expected_media_app_info->is_capturing_microphone = false;
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, LastActivityTime) {
  // Start an instance of kAppId1.
  InstallApp(kAppId1);
  FakeAppInstance instance1(instance_registry_, kAppId1);
  instance1.Start();

  // has-camera, has-mic should start tracking of the kAppId1.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/true);

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

  media_app_info = GetMediaApps();
  crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
      crosapi::mojom::VideoConferenceMediaAppInfo::New(
          /*id=*/media_app_info[0]->id,
          /*last_activity_time=*/media_app_info[0]->last_activity_time,
          /*is_capturing_camera=*/true,
          /*is_capturing_microphone=*/true,
          /*is_capturing_screen=*/false,
          /*title=*/media_app_info[0]->title, /*url=*/std::nullopt,
          /*app_type=*/crosapi::mojom::VideoConferenceAppType::kArcApp);
  ASSERT_EQ(media_app_info.size(), 1u);
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // Hide should not update last activity time.
  instance1.Hide();
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // Show should update last activity time.
  instance1.Show();
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  EXPECT_GT(media_app_info[0]->last_activity_time,
            expected_media_app_info->last_activity_time);
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, CloseApp) {
  // Start two instance of kAppId1.
  InstallApp(kAppId1);
  FakeAppInstance instance1(instance_registry_, kAppId1);
  instance1.Start();
  FakeAppInstance instance2(instance_registry_, kAppId1);
  instance2.Start();

  // No media app should be recorded till now.
  EXPECT_TRUE(GetMediaApps().empty());

  // has-camera, has-mic should start a tracking of the app.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/true);

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

  media_app_info = GetMediaApps();
  crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
      crosapi::mojom::VideoConferenceMediaAppInfo::New(
          /*id=*/media_app_info[0]->id,
          /*last_activity_time=*/media_app_info[0]->last_activity_time,
          /*is_capturing_camera=*/true,
          /*is_capturing_microphone=*/true,
          /*is_capturing_screen=*/false,
          /*title=*/media_app_info[0]->title, /*url=*/std::nullopt,
          /*app_type=*/crosapi::mojom::VideoConferenceAppType::kArcApp);
  ASSERT_EQ(media_app_info.size(), 1u);
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // Closing instance1 should not remove tracking of kAppId1.
  instance1.Close();
  // Wait for the VideoConferenceAppServiceClient::MaybeRemoveApp to be called
  // in the PostTask.
  base::RunLoop().RunUntilIdle();
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // Closing instance2 should remove trackingg of kAppId1.
  instance2.Close();
  // Wait for the VideoConferenceAppServiceClient::MaybeRemoveApp to be called
  // in the PostTask.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetMediaApps().empty());

  // This should not add the app tracking back because there is no running
  // instance.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetMediaApps().empty());

  // This should not add the app tracking back because there is no running
  // instance.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetMediaApps().empty());
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest,
                       HandleMediaUsageUpdate) {
  // Install two apps with permissions.
  InstallApp(kAppId1);
  InstallApp(kAppId2);
  UpdateAppPermision(kAppId1, /*has_camera_permission=*/true,
                     /*has_microphone_permission=*/false);
  UpdateAppPermision(kAppId2, /*has_camera_permission=*/false,
                     /*has_microphone_permission=*/true);

  // Start two running instance.
  FakeAppInstance instance1(instance_registry_, kAppId1);
  instance1.Start();
  FakeAppInstance instance2(instance_registry_, kAppId2);
  instance2.Start();

  VideoConferenceMediaState state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_FALSE(state.has_media_app);
  EXPECT_FALSE(state.has_camera_permission);
  EXPECT_FALSE(state.has_microphone_permission);
  EXPECT_FALSE(state.is_capturing_camera);
  EXPECT_FALSE(state.is_capturing_microphone);
  EXPECT_FALSE(state.is_capturing_screen);

  // Accessing camera should start a tracking of the kAppId1.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/false);

  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_TRUE(state.has_media_app);
  EXPECT_TRUE(state.has_camera_permission);
  EXPECT_TRUE(state.is_capturing_camera);

  // Accessing microphone should start a tracking of the kAppId2.
  SetAppCapabilityAccess(kAppId2, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/true);

  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_TRUE(state.has_media_app);
  EXPECT_TRUE(state.has_microphone_permission);
  EXPECT_TRUE(state.is_capturing_microphone);

  // This should stop the accessing, but not the permission.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/false);

  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_TRUE(state.has_camera_permission);
  EXPECT_FALSE(state.is_capturing_camera);

  // Closing instance1 should remove tracking of kAppId1.
  instance1.Close();
  // Wait for the VideoConferenceAppServiceClient::MaybeRemoveApp to be called
  // in the PostTask.
  base::RunLoop().RunUntilIdle();

  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_FALSE(state.has_camera_permission);
  EXPECT_FALSE(state.is_capturing_camera);

  SetAppCapabilityAccess(kAppId2, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/false);

  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_TRUE(state.has_microphone_permission);
  EXPECT_FALSE(state.is_capturing_microphone);

  // Closing instance2 should remove trackingg of kAppId2.
  instance2.Close();
  // Wait for the VideoConferenceAppServiceClient::MaybeRemoveApp to be called
  // in the PostTask.
  base::RunLoop().RunUntilIdle();

  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_FALSE(state.has_media_app);
  EXPECT_FALSE(state.has_camera_permission);
  EXPECT_FALSE(state.has_microphone_permission);
  EXPECT_FALSE(state.is_capturing_camera);
  EXPECT_FALSE(state.is_capturing_microphone);
  EXPECT_FALSE(state.is_capturing_screen);
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest,
                       OnlyCertainAppsAreTracked) {
  for (const auto type :
       {apps::AppType::kUnknown, apps::AppType::kBuiltIn,
        apps::AppType::kCrostini, apps::AppType::kChromeApp,
        apps::AppType::kWeb, apps::AppType::kPluginVm,
        apps::AppType::kStandaloneBrowser, apps::AppType::kRemote,
        apps::AppType::kBorealis, apps::AppType::kSystemWeb,
        apps::AppType::kStandaloneBrowserChromeApp, apps::AppType::kExtension,
        apps::AppType::kStandaloneBrowserExtension,
        apps::AppType::kBruschetta}) {
    // Create a fake id.
    const std::string app_id = base::NumberToString(static_cast<int>(type));
    // Install the app with given type.
    InstallApp(app_id, type);
    // Start the app.
    FakeAppInstance instance(instance_registry_, app_id);
    instance.Start();

    // has-camera, has-mic should not start tracking of the app only because
    // that we are not tracking the AppType listed above.
    SetAppCapabilityAccess(app_id, /*is_capturing_camera=*/true,
                           /*is_capturing_microphone=*/true);

    EXPECT_TRUE(GetMediaApps().empty());
  }
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest,
                       HandleDeviceUsedWhileDisabled) {
  // Notify disabling state of camera and microphone from
  // video_conference_manager_ash.
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->SetSystemMediaDeviceStatus(
          crosapi::mojom::VideoConferenceMediaDevice::kCamera,
          /*disabled=*/true);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->SetSystemMediaDeviceStatus(
          crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
          /*disabled=*/true);

  FakeVideoConferenceTrayController* fake_try_controller =
      static_cast<FakeVideoConferenceTrayController*>(
          VideoConferenceTrayController::Get());

  InstallApp(kAppId1);

  // Accessing camera will trigger NotifyDeviceUsedWhileDisabled.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/false);
  ASSERT_EQ(fake_try_controller->device_used_while_disabled_records().size(),
            1u);
  EXPECT_THAT(fake_try_controller->device_used_while_disabled_records().back(),
              testing::Pair(crosapi::mojom::VideoConferenceMediaDevice::kCamera,
                            base::UTF8ToUTF16(std::string(kAppName1))));

  // Accessing microphone will trigger NotifyDeviceUsedWhileDisabled.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/true);
  ASSERT_EQ(fake_try_controller->device_used_while_disabled_records().size(),
            2u);
  EXPECT_THAT(
      fake_try_controller->device_used_while_disabled_records().back(),
      testing::Pair(crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
                    base::UTF8ToUTF16(std::string(kAppName1))));

  // Stopping microphone access should not trigger
  // NotifyDeviceUsedWhileDisabled.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/false);
  ASSERT_EQ(fake_try_controller->device_used_while_disabled_records().size(),
            2u);

  // Stopping camera access should not trigger NotifyDeviceUsedWhileDisabled.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/false);
  ASSERT_EQ(fake_try_controller->device_used_while_disabled_records().size(),
            2u);

  // Notify enabling state of camera and microphone from
  // video_conference_manager_ash.
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->SetSystemMediaDeviceStatus(
          crosapi::mojom::VideoConferenceMediaDevice::kCamera,
          /*disabled=*/false);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->SetSystemMediaDeviceStatus(
          crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
          /*disabled=*/false);

  // Accessing camera should not trigger NotifyDeviceUsedWhileDisabled because
  // camera is not disabled.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/false);
  ASSERT_EQ(fake_try_controller->device_used_while_disabled_records().size(),
            2u);

  // Accessing microphone should not trigger NotifyDeviceUsedWhileDisabled
  // because microphone is not disabled.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/true);
  ASSERT_EQ(fake_try_controller->device_used_while_disabled_records().size(),
            2u);
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest,
                       SomeAppsAreNotTracked) {
  // Install all apps that should be skipped.
  for (const std::string& app_id : ::video_conference::kSkipAppIds) {
    InstallApp(app_id);
    // Accessing mic and camera should trigger tracking except the app_id is
    // skipped.
    SetAppCapabilityAccess(app_id, /*is_capturing_camera=*/true,
                           /*is_capturing_microphone=*/true);
  }

  EXPECT_TRUE(GetMediaApps().empty());
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, UkmTest) {
  // Install two apps with permissions.
  InstallApp(kAppId1);
  InstallApp(kAppId2);
  UpdateAppPermision(kAppId1, /*has_camera_permission=*/true,
                     /*has_microphone_permission=*/false);
  UpdateAppPermision(kAppId2, /*has_camera_permission=*/false,
                     /*has_microphone_permission=*/true);

  // Start two running instance.
  FakeAppInstance instance1(instance_registry_, kAppId1);
  instance1.Start();
  FakeAppInstance instance2(instance_registry_, kAppId2);
  instance2.Start();

  // Accessing camera should start a tracking of the kAppId1.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/false);
  // Stopping camera access.
  SetAppCapabilityAccess(kAppId1, /*is_capturing_camera=*/false,
                         /*is_capturing_microphone=*/false);

  // Closing instance1 should remove tracking of kAppId1, thus triggers ukm
  // logging.
  instance1.Close();
  // Wait for the VideoConferenceAppServiceClient::MaybeRemoveApp to be called
  // in the PostTask.
  base::RunLoop().RunUntilIdle();

  auto* vc_entry0 =
      test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName)[0].get();
  test_ukm_recorder_->ExpectEntryMetric(vc_entry0,
                                        UkmEntry::kDidCaptureCameraName, true);
  test_ukm_recorder_->ExpectEntryMetric(
      vc_entry0, UkmEntry::kDidCaptureMicrophoneName, false);
  test_ukm_recorder_->ExpectEntryMetric(vc_entry0,
                                        UkmEntry::kDidCaptureScreenName, false);
  test_ukm_recorder_->ExpectEntryMetric(
      vc_entry0, UkmEntry::kMicrophoneCaptureDurationName, 0);
  test_ukm_recorder_->ExpectEntryMetric(
      vc_entry0, UkmEntry::kScreenCaptureDurationName, 0);

  SetAppCapabilityAccess(kAppId2, /*is_capturing_camera=*/true,
                         /*is_capturing_microphone=*/true);

  // Closing instance2 should remove tracking of kAppId2, thus triggers ukm
  // logging.
  instance2.Close();
  // Wait for the VideoConferenceAppServiceClient::MaybeRemoveApp to be called
  // in the PostTask.
  base::RunLoop().RunUntilIdle();

  auto* vc_entry1 =
      test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName)[1].get();
  test_ukm_recorder_->ExpectEntryMetric(vc_entry1,
                                        UkmEntry::kDidCaptureCameraName, true);
  test_ukm_recorder_->ExpectEntryMetric(
      vc_entry1, UkmEntry::kDidCaptureMicrophoneName, true);
  test_ukm_recorder_->ExpectEntryMetric(vc_entry1,
                                        UkmEntry::kDidCaptureScreenName, false);
  test_ukm_recorder_->ExpectEntryMetric(
      vc_entry1, UkmEntry::kScreenCaptureDurationName, 0);
}

}  // namespace ash
