// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps_publisher_host.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace apps {

class MockAppPublisher : public crosapi::mojom::AppPublisher {
 public:
  MockAppPublisher() { run_loop_ = std::make_unique<base::RunLoop>(); }
  ~MockAppPublisher() override = default;

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  const std::vector<apps::mojom::AppPtr>& get_deltas() const {
    return app_deltas_;
  }

  const std::vector<apps::mojom::CapabilityAccessPtr>&
  get_capability_access_deltas() const {
    return capability_access_deltas_;
  }

 private:
  // crosapi::mojom::AppPublisher:
  void OnApps(std::vector<apps::mojom::AppPtr> deltas) override {
    app_deltas_.insert(app_deltas_.end(),
                       std::make_move_iterator(deltas.begin()),
                       std::make_move_iterator(deltas.end()));
    run_loop_->Quit();
  }

  void RegisterAppController(
      mojo::PendingRemote<crosapi::mojom::AppController> controller) override {
    NOTIMPLEMENTED();
  }

  void OnCapabilityAccesses(
      std::vector<apps::mojom::CapabilityAccessPtr> deltas) override {
    capability_access_deltas_.insert(capability_access_deltas_.end(),
                                     std::make_move_iterator(deltas.begin()),
                                     std::make_move_iterator(deltas.end()));
    run_loop_->Quit();
  }

  std::vector<apps::mojom::AppPtr> app_deltas_;
  std::vector<apps::mojom::CapabilityAccessPtr> capability_access_deltas_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class WebAppsPublisherHostBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  WebAppsPublisherHostBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kAppServiceAdaptiveIcon);
  }
  ~WebAppsPublisherHostBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, PublishApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  web_app::InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));
  web_app::InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/web_share_target/charts.html"));

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);

  web_app::AppId app_id = web_app::InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  mock_app_publisher.Wait();

  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 3U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->readiness,
            apps::mojom::Readiness::kReady);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);

  {
    base::RunLoop run_loop;
    web_app::UninstallWebAppWithCallback(
        profile(), app_id,
        base::BindLambdaForTesting([&run_loop](bool uninstalled) {
          EXPECT_TRUE(uninstalled);
          run_loop.Quit();
        }));
    run_loop.Run();
    mock_app_publisher.Wait();
    EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
    EXPECT_EQ(mock_app_publisher.get_deltas().back()->readiness,
              apps::mojom::Readiness::kUninstalledByUser);
  }
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, LaunchTime) {
  ASSERT_TRUE(embedded_test_server()->Start());
  web_app::AppId app_id = web_app::InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  ASSERT_TRUE(
      mock_app_publisher.get_deltas().back()->last_launch_time.has_value());
  const base::Time last_launch_time =
      *mock_app_publisher.get_deltas().back()->last_launch_time;

  LaunchWebAppBrowser(app_id);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  ASSERT_TRUE(
      mock_app_publisher.get_deltas().back()->last_launch_time.has_value());
  EXPECT_GT(*mock_app_publisher.get_deltas().back()->last_launch_time,
            last_launch_time);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, ManifestUpdate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/simple.html");

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();

  web_app::AppId app_id;
  {
    const std::u16string original_description = u"Original Web App";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = original_description;
    web_app_info->description = original_description;
    app_id = InstallWebApp(std::move(web_app_info));

    mock_app_publisher.Wait();
    EXPECT_EQ(*mock_app_publisher.get_deltas().back()->description,
              base::UTF16ToUTF8(original_description));
  }

  {
    const std::u16string updated_description = u"Updated Web App";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = updated_description;
    web_app_info->description = updated_description;

    base::RunLoop run_loop;
    provider().install_manager().UpdateWebAppFromInfo(
        app_id, std::move(web_app_info),
        /*redownload_app_icons=*/false,
        base::BindLambdaForTesting(
            [&run_loop](const web_app::AppId& app_id,
                        web_app::InstallResultCode code) { run_loop.Quit(); }));

    run_loop.Run();
    mock_app_publisher.Wait();
    EXPECT_EQ(*mock_app_publisher.get_deltas().back()->description,
              base::UTF16ToUTF8(updated_description));
  }
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, LocallyInstalledState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/simple.html");

  web_app::AppId app_id;
  {
    const std::u16string description = u"Web App";
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = description;
    web_app_info->description = description;
    app_id = InstallWebApp(std::move(web_app_info));

    provider()
        .registry_controller()
        .AsWebAppSyncBridge()
        ->SetAppIsLocallyInstalled(app_id,
                                   /*is_locally_installed=*/false);
  }

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            static_cast<IconEffects>(IconEffects::kRoundCorners |
                                     IconEffects::kBlocked |
                                     IconEffects::kCrOsStandardMask));

  provider()
      .registry_controller()
      .AsWebAppSyncBridge()
      ->SetAppIsLocallyInstalled(app_id,
                                 /*is_locally_installed=*/true);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask);
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, ContentSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  web_app::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), app_url);

  // Install an additional app from a different host.
  {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = GURL("https://example.com:8080/");
    web_app_info->scope = web_app_info->start_url;
    web_app_info->title = u"Unrelated Web App";
    InstallWebApp(std::move(web_app_info));
  }

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);

  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings_map->SetContentSettingDefaultScope(
      app_url, app_url, ContentSettingsType::MEDIASTREAM_CAMERA,
      CONTENT_SETTING_ALLOW);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 3U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_type,
            apps::mojom::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);

  const std::vector<apps::mojom::PermissionPtr>& permissions =
      mock_app_publisher.get_deltas().back()->permissions;
  auto camera_permission = std::find_if(
      permissions.begin(), permissions.end(),
      [](const apps::mojom::PermissionPtr& permission) {
        return permission->permission_id ==
               static_cast<uint32_t>(ContentSettingsType::MEDIASTREAM_CAMERA);
      });
  ASSERT_TRUE(camera_permission != permissions.end());
  EXPECT_EQ((*camera_permission)->value_type,
            apps::mojom::PermissionValueType::kTriState);
  EXPECT_EQ((*camera_permission)->value,
            static_cast<uint32_t>(apps::mojom::TriState::kAllow));
}

IN_PROC_BROWSER_TEST_F(WebAppsPublisherHostBrowserTest, MediaRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  web_app::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), app_url);
  Browser* browser = LaunchWebAppBrowserAndWait(app_id);
  content::RenderFrameHost* render_frame_host =
      browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  const int render_process_id = render_frame_host->GetProcess()->GetID();
  const int render_frame_id = render_frame_host->GetRoutingID();

  MockAppPublisher mock_app_publisher;
  WebAppsPublisherHost web_apps_publisher_host(profile());
  web_apps_publisher_host.SetPublisherForTesting(&mock_app_publisher);
  web_apps_publisher_host.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([render_process_id, render_frame_id,
                                             app_url]() {
        MediaCaptureDevicesDispatcher::GetInstance()
            ->OnMediaRequestStateChanged(
                render_process_id, render_frame_id,
                /*page_request_id=*/0, app_url,
                blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                content::MEDIA_REQUEST_STATE_DONE);
      }));
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().size(), 1U);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->app_id,
            app_id);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->camera,
            apps::mojom::OptionalBool::kUnknown);
  EXPECT_EQ(
      mock_app_publisher.get_capability_access_deltas().back()->microphone,
      apps::mojom::OptionalBool::kTrue);

  browser->tab_strip_model()->CloseAllTabs();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().size(), 2U);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->app_id,
            app_id);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->camera,
            apps::mojom::OptionalBool::kUnknown);
  EXPECT_EQ(
      mock_app_publisher.get_capability_access_deltas().back()->microphone,
      apps::mojom::OptionalBool::kFalse);
}

}  // namespace apps
