// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_app_service_client.h"

#include <cstdlib>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/test/test_windows.h"

namespace ash {

using AppIdString = std::string;

constexpr char kAppId1[] = "random_app_id_1";
constexpr char kAppName1[] = "random_app_name_1";
constexpr char kAppId2[] = "random_app_id_2";
constexpr char kAppName2[] = "random_app_name_2";

// Creates an app with given id and name.
apps::AppPtr MakeApp(const AppIdString& app_id, const std::string& name) {
  apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kArc, app_id);
  app->name = name;
  app->publisher_id = app_id;
  return app;
}

apps::mojom::OptionalBool MojomOptionalBool(bool value) {
  return value ? apps::mojom::OptionalBool::kTrue
               : apps::mojom::OptionalBool::kFalse;
}

// This fake instance class simulates the creation, destruction, hiding and
// showing an app instance.
class FakeAppInstance {
 public:
  FakeAppInstance(Profile* profile, const AppIdString& app_id) {
    instance_registry_ = &apps::AppServiceProxyFactory::GetForProfile(profile)
                              ->InstanceRegistry();
    window_ = std::unique_ptr<aura::Window>(
        aura::test::CreateTestWindowWithId(/*id=*/++next_window_id_, nullptr));
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
    instance->UpdateState(apps::InstanceState::kVisible, base::Time::Now());
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
  base::raw_ptr<apps::InstanceRegistry> instance_registry_;

  static int next_window_id_;
};

int FakeAppInstance::next_window_id_ = 0;

class VideoConferenceAppServiceClientTest : public LoginManagerTest {
 public:
  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    // Login two accounts.
    login_manager_.AppendRegularUsers(2);
    LoginUser(login_manager_.users()[0].account_id);
    LoginUser(login_manager_.users()[1].account_id);

    capability_cache_.SetAccountId(login_manager_.users()[0].account_id);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        login_manager_.users()[0].account_id, &capability_cache_);

    client_ = std::make_unique<VideoConferenceAppServiceClient>();

    // Switching users will force AppService objects update inside the
    // `client_`.
    user_manager::UserManager::Get()->SwitchActiveUser(
        login_manager_.users()[0].account_id);
    profile_ = ProfileManager::GetActiveUserProfile();
  }

  void TearDownOnMainThread() override {
    client_.reset();
    LoginManagerTest::TearDownOnMainThread();
  }

  // This function creates an app with given id and name, and adds the app into
  // AppRegistryCache of current profile.
  void InstallApp(const std::string& app_id, const std::string& app_name) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
    std::vector<apps::AppPtr> deltas;
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    deltas.push_back(MakeApp(app_id, app_name));
    cache.OnApps(std::move(deltas), apps::AppType::kUnknown,
                 /* should_notify_initialized = */ false);
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

    capability_cache_.OnCapabilityAccesses(std::move(deltas));
  }

  // Adds {id, state} pair to client_->id_to_app_state_.
  void AddAppState(const AppIdString& app_id,
                   const VideoConferenceAppServiceClient::AppState& state) {
    (client_->id_to_app_state_)[app_id] = state;
  }

  std::string GetAppName(const AppIdString& app_id) {
    return client_->GetAppName(app_id);
  }

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> GetMediaApps() {
    std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

    client_->GetMediaApps(base::BindLambdaForTesting(
        [&media_app_info](
            std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>
                result) { media_app_info = std::move(result); }));

    return media_app_info;
  }

 protected:
  int next_window_id_ = 0;
  LoginManagerMixin login_manager_{&mixin_host_};
  Profile* profile_ = nullptr;
  apps::AppCapabilityAccessCache capability_cache_;
  std::unique_ptr<VideoConferenceAppServiceClient> client_;
};

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, GetAppName) {
  // AppName should be empty if it is not installed.
  EXPECT_EQ(GetAppName(kAppId1), std::string());

  InstallApp(kAppId1, kAppName1);

  // AppName should be correct if installed.
  EXPECT_EQ(GetAppName(kAppId1), kAppName1);
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

  InstallApp(kAppId1, kAppName1);

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
          /*url=*/absl::nullopt);

  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, ReturnToApp) {
  // Add two instance for kAppId1.
  FakeAppInstance instance1(profile_, kAppId1);
  instance1.Start();
  FakeAppInstance instance2(profile_, kAppId1);
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
  InstallApp(kAppId1, kAppName1);
  InstallApp(kAppId2, kAppName2);

  // no-camera, no-mic should not start a tracking of the app.
  SetAppCapabilityAccess(kAppId1, false, false);
  EXPECT_TRUE(GetMediaApps().empty());

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

  // has-camera, no-mic should start the tracking of the app.
  SetAppCapabilityAccess(kAppId1, true, false);
  media_app_info = GetMediaApps();
  crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
      crosapi::mojom::VideoConferenceMediaAppInfo::New(
          /*id=*/media_app_info[0]->id,
          /*last_activity_time=*/media_app_info[0]->last_activity_time,
          /*is_capturing_camera=*/true,
          /*is_capturing_microphone=*/false,
          /*is_capturing_screen=*/false,
          /*title=*/media_app_info[0]->title, /*url=*/absl::nullopt);
  ASSERT_EQ(media_app_info.size(), 1u);
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // has-camera, has-mic should change the value of GetMediaApps.
  SetAppCapabilityAccess(kAppId1, true, true);
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  expected_media_app_info->is_capturing_microphone = true;
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // no-camera, has-mic should change the value of GetMediaApps.
  SetAppCapabilityAccess(kAppId1, false, true);
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  expected_media_app_info->is_capturing_camera = false;
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

  // no-camera, no-mic should change the value of GetMediaApps; but not removing
  // the tracking app.
  SetAppCapabilityAccess(kAppId1, false, false);
  media_app_info = GetMediaApps();
  ASSERT_EQ(media_app_info.size(), 1u);
  expected_media_app_info->is_capturing_microphone = false;
  EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAppServiceClientTest, LastActivityTime) {
  // Start an instance of kAppId1.
  InstallApp(kAppId1, kAppName1);
  FakeAppInstance instance1(profile_, kAppId1);
  instance1.Start();

  // has-camera, has-mic should start tracking of the kAppId1.
  SetAppCapabilityAccess(kAppId1, true, true);

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

  media_app_info = GetMediaApps();
  crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
      crosapi::mojom::VideoConferenceMediaAppInfo::New(
          /*id=*/media_app_info[0]->id,
          /*last_activity_time=*/media_app_info[0]->last_activity_time,
          /*is_capturing_camera=*/true,
          /*is_capturing_microphone=*/true,
          /*is_capturing_screen=*/false,
          /*title=*/media_app_info[0]->title, /*url=*/absl::nullopt);
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
  InstallApp(kAppId1, kAppName1);
  FakeAppInstance instance1(profile_, kAppId1);
  instance1.Start();
  FakeAppInstance instance2(profile_, kAppId1);
  instance2.Start();

  // No media app should be recorded till now.
  EXPECT_TRUE(GetMediaApps().empty());

  // has-camera, has-mic should start a tracking of the app.
  SetAppCapabilityAccess(kAppId1, true, true);

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

  media_app_info = GetMediaApps();
  crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
      crosapi::mojom::VideoConferenceMediaAppInfo::New(
          /*id=*/media_app_info[0]->id,
          /*last_activity_time=*/media_app_info[0]->last_activity_time,
          /*is_capturing_camera=*/true,
          /*is_capturing_microphone=*/true,
          /*is_capturing_screen=*/false,
          /*title=*/media_app_info[0]->title, /*url=*/absl::nullopt);
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
}

}  // namespace ash
