// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_manager_client.h"

#include <string>
#include <vector>

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#endif
#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/chromeos/video_conference/video_conference_media_listener.h"
#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_conference {

namespace {
constexpr char kTestURL1[] = "about:blank";
constexpr char kTestURL2[] = "https://localhost";
}  // namespace

// Fake class for testing `VideoConferenceManagerClientImpl`. Overrides
// `NotifyManager()` to not send any updates to the VC manager and provides
// access to the internal `id_to_webcontents` map on the client.
class FakeVideoConferenceManagerClient
    : public VideoConferenceManagerClientImpl {
 public:
  FakeVideoConferenceManagerClient() = default;

  FakeVideoConferenceManagerClient(const FakeVideoConferenceManagerClient&) =
      delete;
  FakeVideoConferenceManagerClient& operator=(
      const FakeVideoConferenceManagerClient&) = delete;

  ~FakeVideoConferenceManagerClient() override = default;

  std::map<base::UnguessableToken, raw_ptr<content::WebContents>>
  id_to_webcontents() {
    return id_to_webcontents_;
  }

  const base::UnguessableToken& client_id() { return client_id_; }

  VideoConferencePermissions GetAggregatedPermissions() {
    return VideoConferenceManagerClientImpl::GetAggregatedPermissions();
  }

  bool camera_system_disabled() {
    return media_listener_->camera_system_disabled_;
  }

  bool microphone_system_disabled() {
    return media_listener_->microphone_system_disabled_;
  }

  crosapi::mojom::VideoConferenceMediaUsageStatusPtr& status() {
    return status_;
  }

 protected:
  void NotifyManager(
      crosapi::mojom::VideoConferenceMediaUsageStatusPtr status) override {}
};

class VideoConferenceManagerClientTest : public InProcessBrowserTest {
 public:
  VideoConferenceManagerClientTest() = default;

  VideoConferenceManagerClientTest(const VideoConferenceManagerClientTest&) =
      delete;
  VideoConferenceManagerClientTest& operator=(
      const VideoConferenceManagerClientTest&) = delete;

  ~VideoConferenceManagerClientTest() override = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kFeatureManagementVideoConference);

    InProcessBrowserTest::SetUp();
  }
#endif

  // Creates and returns a new `WebContents` at the given tab `index`.
  content::WebContents* CreateWebContentsAt(int index) {
    EXPECT_TRUE(
        AddTabAtIndex(index, GURL(kTestURL1), ui::PAGE_TRANSITION_TYPED));
    return browser()->tab_strip_model()->GetWebContentsAt(index);
  }

  // Removed `WebContents` at the given tab `index`.
  void RemoveWebContentsAt(int index) {
    browser()->tab_strip_model()->CloseWebContentsAt(index,
                                                     TabCloseTypes::CLOSE_NONE);
  }

  void UpdateWebContentsTitle(content::WebContents* contents,
                              const std::u16string& title) {
    content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(entry);
    contents->UpdateTitleForEntry(entry, title);
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

// Tests creating VcWebApps and removing them by closing tabs.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerClientTest,
                       TabCreationAndRemoval) {
  FakeVideoConferenceManagerClient client;

  auto* web_contents1 = CreateWebContentsAt(0);
  auto* web_contents2 = CreateWebContentsAt(1);
  auto* web_contents3 = CreateWebContentsAt(2);

  client.CreateVideoConferenceWebApp(web_contents1);
  EXPECT_EQ(client.id_to_webcontents().size(), 1u);

  client.CreateVideoConferenceWebApp(web_contents2);
  EXPECT_EQ(client.id_to_webcontents().size(), 2u);

  client.CreateVideoConferenceWebApp(web_contents3);
  EXPECT_EQ(client.id_to_webcontents().size(), 3u);

  // It's important to close tabs from right-to-left as otherwise the indices
  // change.
  RemoveWebContentsAt(2);
  EXPECT_EQ(client.id_to_webcontents().size(), 2u);

  RemoveWebContentsAt(1);
  EXPECT_EQ(client.id_to_webcontents().size(), 1u);

  RemoveWebContentsAt(0);
  EXPECT_EQ(client.id_to_webcontents().size(), 0u);
}

// Tests that a change in the primary page of the web contents of a VcWebApp
// removes it from the client.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerClientTest,
                       WebContentsPrimaryPageChange) {
  FakeVideoConferenceManagerClient client;
  TabActivitySimulator tab_activity_simulator;

  auto* web_contents = CreateWebContentsAt(0);
  auto* vc_app = client.CreateVideoConferenceWebApp(web_contents);

  EXPECT_EQ(client.id_to_webcontents().size(), 1u);

  // Ensure tab is in focus.
  vc_app->ActivateApp();
  // Navigate to a different URL and trigger a primary page change event.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTestURL2)));
  // There should no longer be a WebContentsUserData associated with this
  // `web_contents`.
  EXPECT_FALSE(
      content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
          web_contents));

  EXPECT_EQ(client.id_to_webcontents().size(), 0u);
}

// Tests `GetMediaApps` returns `VideoConferenceMediaAppInfo`s with expected
// values.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerClientTest, GetMediaApps) {
  FakeVideoConferenceManagerClient client;

  auto* web_contents1 = CreateWebContentsAt(0);
  UpdateWebContentsTitle(web_contents1, u"app1");

  auto* web_contents2 = CreateWebContentsAt(1);
  UpdateWebContentsTitle(web_contents2, u"app2");

  auto* web_contents3 = CreateWebContentsAt(2);
  auto* vc_app1 = client.CreateVideoConferenceWebApp(web_contents1);
  auto* vc_app2 = client.CreateVideoConferenceWebApp(web_contents2);
  auto* vc_app3 = client.CreateVideoConferenceWebApp(web_contents3);

  vc_app1->state().is_capturing_camera = true;

  vc_app1->state().is_capturing_microphone = true;
  vc_app2->state().is_capturing_microphone = true;

  vc_app1->state().is_capturing_screen = true;
  vc_app2->state().is_capturing_screen = true;
  vc_app3->state().is_capturing_screen = true;

  std::map<base::UnguessableToken, VideoConferenceWebApp*> id_to_vc_app = {
      {vc_app1->state().id, vc_app1},
      {vc_app2->state().id, vc_app2},
      {vc_app3->state().id, vc_app3},
  };

  client.GetMediaApps(base::BindLambdaForTesting(
      [&](std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> apps) {
        EXPECT_EQ(apps.size(), 3u);

        for (auto& app : apps) {
          auto* vc_app = id_to_vc_app[app->id];
          EXPECT_EQ(vc_app->state().is_capturing_camera,
                    app->is_capturing_camera);
          EXPECT_EQ(vc_app->state().is_capturing_microphone,
                    app->is_capturing_microphone);
          EXPECT_EQ(vc_app->state().is_capturing_screen,
                    app->is_capturing_screen);
          EXPECT_EQ(vc_app->GetWebContents().GetTitle(), app->title);
        }
      }));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests setting/clearing system statuses for camera and microphone.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerClientTest,
                       SetSystemMediaDeviceStatus) {
  FakeVideoConferenceManagerClient client;

  auto* vc_manager = crosapi::CrosapiManager::Get()
                         ->crosapi_ash()
                         ->video_conference_manager_ash();
  vc_manager->RegisterCppClient(&client, client.client_id());

  ash::FakeVideoConferenceTrayController* controller =
      static_cast<ash::FakeVideoConferenceTrayController*>(
          ash::VideoConferenceTrayController::Get());
  ASSERT_TRUE(controller);
  EXPECT_EQ(controller->device_used_while_disabled_records().size(), 0u);

  EXPECT_FALSE(client.camera_system_disabled());
  EXPECT_FALSE(client.microphone_system_disabled());

  vc_manager->SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera,
      /*disabled=*/true);
  EXPECT_TRUE(client.camera_system_disabled());
  EXPECT_FALSE(client.microphone_system_disabled());

  vc_manager->SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
      /*disabled=*/true);
  EXPECT_TRUE(client.camera_system_disabled());
  EXPECT_TRUE(client.microphone_system_disabled());

  vc_manager->SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
      /*disabled=*/false);
  EXPECT_TRUE(client.camera_system_disabled());
  EXPECT_FALSE(client.microphone_system_disabled());

  vc_manager->SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera,
      /*disabled=*/false);
  EXPECT_FALSE(client.camera_system_disabled());
  EXPECT_FALSE(client.microphone_system_disabled());
}

// Tests client updates relating to adding and removing VC web apps and title
// changes.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerClientTest, ClientUpdate) {
  FakeVideoConferenceManagerClient client;

  auto* vc_manager = crosapi::CrosapiManager::Get()
                         ->crosapi_ash()
                         ->video_conference_manager_ash();
  vc_manager->RegisterCppClient(&client, client.client_id());

  ash::FakeVideoConferenceTrayController* controller =
      static_cast<ash::FakeVideoConferenceTrayController*>(
          ash::VideoConferenceTrayController::Get());
  ASSERT_TRUE(controller);

  // Add a new VC web app on the client.
  EXPECT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  client.CreateVideoConferenceWebApp(web_contents);

  // Confirm the update received by the controller has `added_or_removed_app`
  // set to true.
  EXPECT_EQ(controller->last_client_update()->added_or_removed_app,
            crosapi::mojom::VideoConferenceAppUpdate::kAppAdded);
  EXPECT_FALSE(controller->last_client_update()->title_change_info);

  // Update the title and confirm correct fields were set.
  std::u16string new_title = u"New Title";
  UpdateWebContentsTitle(web_contents, new_title);

  EXPECT_EQ(controller->last_client_update()->added_or_removed_app,
            crosapi::mojom::VideoConferenceAppUpdate::kNone);
  EXPECT_TRUE(controller->last_client_update()->title_change_info);
  EXPECT_EQ(controller->last_client_update()->title_change_info->new_title,
            new_title);

  // Remove the VC web app by closing the corresponding WebContents.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  EXPECT_EQ(controller->last_client_update()->added_or_removed_app,
            crosapi::mojom::VideoConferenceAppUpdate::kAppRemoved);
  EXPECT_FALSE(controller->last_client_update()->title_change_info);
}
#endif

// Tests aggregated media usage status received on `HandleMediaUsageUpdate`.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerClientTest, MediaUsageUpdate) {
  FakeVideoConferenceManagerClient client;

  EXPECT_FALSE(client.status()->has_media_app);
  EXPECT_FALSE(client.status()->is_capturing_camera);
  EXPECT_FALSE(client.status()->is_capturing_microphone);
  EXPECT_FALSE(client.status()->is_capturing_screen);

  auto* web_contents1 = CreateWebContentsAt(0);
  UpdateWebContentsTitle(web_contents1, u"app1");

  auto* web_contents2 = CreateWebContentsAt(1);
  UpdateWebContentsTitle(web_contents2, u"app2");

  auto* web_contents3 = CreateWebContentsAt(2);
  auto* vc_app1 = client.CreateVideoConferenceWebApp(web_contents1);
  auto* vc_app2 = client.CreateVideoConferenceWebApp(web_contents2);
  auto* vc_app3 = client.CreateVideoConferenceWebApp(web_contents3);

  client.HandleMediaUsageUpdate();
  EXPECT_TRUE(client.status()->has_media_app);
  EXPECT_FALSE(client.status()->is_capturing_camera);
  EXPECT_FALSE(client.status()->is_capturing_microphone);
  EXPECT_FALSE(client.status()->is_capturing_screen);

  vc_app1->state().is_capturing_camera = true;
  client.HandleMediaUsageUpdate();
  EXPECT_TRUE(client.status()->has_media_app);
  EXPECT_TRUE(client.status()->is_capturing_camera);
  EXPECT_FALSE(client.status()->is_capturing_microphone);
  EXPECT_FALSE(client.status()->is_capturing_screen);

  vc_app2->state().is_capturing_microphone = true;
  client.HandleMediaUsageUpdate();
  EXPECT_TRUE(client.status()->has_media_app);
  EXPECT_TRUE(client.status()->is_capturing_camera);
  EXPECT_TRUE(client.status()->is_capturing_microphone);
  EXPECT_FALSE(client.status()->is_capturing_screen);

  vc_app3->state().is_capturing_screen = true;
  client.HandleMediaUsageUpdate();
  EXPECT_TRUE(client.status()->has_media_app);
  EXPECT_TRUE(client.status()->is_capturing_camera);
  EXPECT_TRUE(client.status()->is_capturing_microphone);
  EXPECT_TRUE(client.status()->is_capturing_screen);

  RemoveWebContentsAt(2);
  RemoveWebContentsAt(1);
  RemoveWebContentsAt(0);

  client.HandleMediaUsageUpdate();
  EXPECT_FALSE(client.status()->has_media_app);
  EXPECT_FALSE(client.status()->is_capturing_camera);
  EXPECT_FALSE(client.status()->is_capturing_microphone);
  EXPECT_FALSE(client.status()->is_capturing_screen);
}

// Tests if `ReturnToApp` correctly activates tab of the `VideoConferenceWebApp`
// corresponding to the `id` provided.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerClientTest, ReturnToApp) {
  FakeVideoConferenceManagerClient client;

  auto* web_contents1 = CreateWebContentsAt(0);
  auto* web_contents2 = CreateWebContentsAt(1);

  auto* vc_app1 = client.CreateVideoConferenceWebApp(web_contents1);
  auto* vc_app2 = client.CreateVideoConferenceWebApp(web_contents2);

  client.ReturnToApp(
      vc_app1->state().id, base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
      }));

  client.ReturnToApp(
      vc_app2->state().id, base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
      }));

  client.ReturnToApp(
      vc_app1->state().id, base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
      }));
}

// Tests that for extensions, permissions equate to capturing statuses.
IN_PROC_BROWSER_TEST_F(VideoConferenceManagerClientTest, ExtensionPermissions) {
  FakeVideoConferenceManagerClient client;

  auto* web_contents = CreateWebContentsAt(0);

  auto* vc_app = client.CreateVideoConferenceWebApp(web_contents);

  // Make vc_app an extension.
  vc_app->state().is_extension = true;

  auto permissions = client.GetAggregatedPermissions();
  EXPECT_FALSE(permissions.has_camera_permission);
  EXPECT_FALSE(permissions.has_microphone_permission);

  // Set capturing to true.
  vc_app->state().is_capturing_camera = true;

  permissions = client.GetAggregatedPermissions();
  EXPECT_TRUE(permissions.has_camera_permission);
  EXPECT_FALSE(permissions.has_microphone_permission);

  // Make vc_app a non-extension.
  vc_app->state().is_extension = false;

  permissions = client.GetAggregatedPermissions();
  EXPECT_FALSE(permissions.has_camera_permission);
  EXPECT_FALSE(permissions.has_microphone_permission);
}

}  // namespace video_conference
