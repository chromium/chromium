// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/web_apps/web_apps_sync_test_base.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {
using Param = std::tuple<bool /*wait_8_days*/,
                         bool /*sync_broken_icons*/,
                         bool /*trusted_icons_enabled*/,
                         bool /*predictable_app_updates_enabled*/,
                         SyncTest::SetupSyncMode>;
}  // namespace

class SingleClientWebAppsSyncGeneratedIconFixSyncTest
    : public WebAppsSyncTestBase,
      public testing::WithParamInterface<Param> {
 public:
  static std::string ParamToString(testing::TestParamInfo<Param> param) {
    return base::StrCat({
        std::get<0>(param.param) ? "Wait8Days" : "NoWait",
        "_",
        std::get<1>(param.param) ? "SyncBrokenIcons" : "SyncNormalIcons",
        "_",
        std::get<2>(param.param) ? "TrustedIconsEnabled"
                                 : "TrustedIconsDisabled",
        "_",
        std::get<3>(param.param) ? "PredictableAppUpdatesEnabled"
                                 : "PredictableAppUpdatesDisabled",
        "_",
        testing::PrintToString(std::get<4>(param.param)),
    });
  }

  SingleClientWebAppsSyncGeneratedIconFixSyncTest()
      : WebAppsSyncTestBase(SINGLE_CLIENT) {
    clock_ = std::make_unique<base::SimpleTestClock>();
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }

    if (trusted_icons_enabled()) {
      enabled_features.push_back(features::kWebAppUsePrimaryIcon);
    } else {
      disabled_features.push_back(features::kWebAppUsePrimaryIcon);
    }

    if (predictable_app_updates_enabled()) {
      enabled_features.push_back(features::kWebAppPredictableAppUpdating);
    } else {
      disabled_features.push_back(features::kWebAppPredictableAppUpdating);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~SingleClientWebAppsSyncGeneratedIconFixSyncTest() override = default;

  bool wait_8_days() const { return std::get<0>(GetParam()); }
  bool sync_broken_icons() const { return std::get<1>(GetParam()); }
  bool trusted_icons_enabled() const { return std::get<2>(GetParam()); }
  bool predictable_app_updates_enabled() const {
    return std::get<3>(GetParam());
  }

  WebAppProvider& provider(int index) {
    return *WebAppProvider::GetForTest(GetProfile(index));
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();
    ASSERT_TRUE(SetupSync());
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [this](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (!serve_pngs_.load() &&
              base::EndsWith(request.GetURL().spec(), ".png")) {
            auto http_response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            http_response->set_code(net::HTTP_NOT_FOUND);
            return std::move(http_response);
          }
          return nullptr;
        }));
    embedded_test_server_handle_ =
        embedded_test_server()->StartAndReturnHandle();

    // Since this is a single client test and there is only one provider to work
    // with, set the clock just once.
    provider(0).SetClockForTesting(clock_.get());
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallAllWebApps(GetProfile(/*index=*/0));
    SyncTest::TearDownOnMainThread();
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return std::get<4>(GetParam());
  }

  // Triggers a manifest update by launching the app or loading the update_url
  // in a new browser tab as per the manifest update flow being tested.
  void TriggerManifestUpdateAndAwaitCompletion(const webapps::AppId& app_id,
                                               GURL update_url) {
    clock_->SetNow(base::Time::Now());
    base::test::TestFuture<void> future;
    // TODO(http://crbug.com/452053908): This is no longer needed after we move
    // waiting for page load to the update command.
    provider(0).manifest_update_manager().SetLoadFinishedCallbackForTesting(
        future.GetCallback());
    if (predictable_app_updates_enabled() && trusted_icons_enabled()) {
      Browser* app_browser =
          LaunchWebAppBrowserAndWait(GetProfile(/*index=*/0), app_id);
      CHECK(app_browser);
    } else {
      CHECK(AddTabAtIndexToBrowser(GetBrowser(0), 0, update_url,
                                   ui::PAGE_TRANSITION_AUTO_TOPLEVEL));
    }
    EXPECT_TRUE(future.Wait());
    provider(0).command_manager().AwaitAllCommandsCompleteForTesting();
  }

  bool WasAppUpdated(const webapps::AppId& app_id) {
    return !provider(0)
                .registrar_unsafe()
                .GetAppById(app_id)
                ->manifest_update_time()
                .is_null();
  }

 protected:
  std::atomic<bool> serve_pngs_ = true;

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  net::test_server::EmbeddedTestServerHandle embedded_test_server_handle_;
  std::unique_ptr<base::SimpleTestClock> clock_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SingleClientWebAppsSyncGeneratedIconFixSyncTest,
                       GeneratedIconsSilentlyUpdate) {
  // Listen for sync install in client.
  WebAppTestInstallObserver install_observer(GetProfile(0));
  install_observer.BeginListening();

  if (sync_broken_icons()) {
    // Cause icon downloading to fail.
    serve_pngs_.store(false);
  }

  // Insert web app into sync profile.
  // Fields copied from chrome/test/data/web_apps/basic.json.
  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  GURL image_url1 = embedded_test_server()->GetURL("/web_apps/basic-48.png");
  GURL image_url2 = embedded_test_server()->GetURL("/web_apps/basic-192.png");

  sync_pb::EntitySpecifics specifics;
  sync_pb::WebAppSpecifics& web_app_specifics = *specifics.mutable_web_app();
  web_app_specifics.set_start_url(start_url.spec());
  web_app_specifics.set_user_display_mode_default(
      sync_pb::WebAppSpecifics::STANDALONE);
  web_app_specifics.set_name("Basic web app");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);

  sync_pb::WebAppIconInfo icon_info1;
  icon_info1.set_size_in_px(48);
  icon_info1.set_url(image_url1.spec());
  icon_info1.set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);

  sync_pb::WebAppIconInfo icon_info2;
  icon_info2.set_size_in_px(192);
  icon_info2.set_url(image_url2.spec());
  icon_info2.set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
  web_app_specifics.mutable_icon_infos()->Add(std::move(icon_info1));
  web_app_specifics.mutable_icon_infos()->Add(std::move(icon_info2));

  sync_pb::WebAppIconInfo trusted_icon_info;
  trusted_icon_info.set_size_in_px(192);
  trusted_icon_info.set_url(image_url2.spec());
  trusted_icon_info.set_purpose(sync_pb::WebAppIconInfo_Purpose_ANY);
  web_app_specifics.mutable_trusted_icons()->Add(std::move(trusted_icon_info));

  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/app_id,
          /*client_tag=*/app_id, specifics,
          /*creation_time=*/0, /*last_modified_time=*/0));

  // Await sync install.
  EXPECT_EQ(install_observer.Wait(), app_id);
  provider(/*index=*/0).command_manager().AwaitAllCommandsCompleteForTesting();

  // Verify that the app did not have a manifest update triggered yet.
  EXPECT_FALSE(WasAppUpdated(app_id));

  // Icons should be generated always now that sync follows the fallback
  // installation path once the trusted icons architecture lands.
  EXPECT_EQ(provider(/*index=*/0)
                .registrar_unsafe()
                .GetAppById(app_id)
                ->is_generated_icon(),
            sync_broken_icons());

  // Ensure installed locally to enable manifest updating.
  {
    base::RunLoop run_loop;
    provider(/*index=*/0)
        .scheduler()
        .InstallAppLocally(app_id, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Make sure the web app fields that might "mistakenly" trigger a silent
  // update are set correctly so that it does not happen. This includes:
  // 1. DisplayMode. The sync installed app comes installed with a display mode
  // of kBrowser, but basic.json has a display mode of kStandalone.
  // 2. Manifest url. The sync installed app does not come with a manifest url
  // but the new app does.
  {
    ScopedRegistryUpdate scoped_update =
        provider(/*index=*/0).sync_bridge_unsafe().BeginUpdate();
    WebApp* app = scoped_update->UpdateApp(app_id);
    ASSERT_NE(nullptr, app);
    app->SetDisplayMode(DisplayMode::kStandalone);
    app->SetManifestUrl(embedded_test_server()->GetURL("/web_apps/basic.json"));
  }

  // Re-enable icons if disabled.
  serve_pngs_.store(true);

  if (wait_8_days()) {
    // Advance time beyond the fix time window.
    generated_icon_fix_util::SetNowForTesting(base::Time::Now() +
                                              base::Days(8));
  }

  // Trigger manifest update, and verify that the icons were updated.
  TriggerManifestUpdateAndAwaitCompletion(app_id, start_url);

  // Check icons fixed in time window, provided trusted icons architecture is
  // not enabled. With trusted icons enabled, sync installs always install from
  // fallback and have generated icons, so the fix is always applied as part of
  // the manifest update process.
  bool expect_fix_applied = !wait_8_days() && sync_broken_icons();

  // The only time generated icons are still expected are:
  // 1. The fix is not applied (like if it's more than 8 days which is the
  // threshold for the GeneratedIconFixManager).
  // 2. If there is a generated icon in the first case, which can either be from
  // the trusted icon infrastructure installing from empty icons in this case,
  // or if broken icons were synced.
  bool expect_generated_icons = wait_8_days() && sync_broken_icons();

  EXPECT_EQ(WasAppUpdated(app_id), expect_fix_applied);
  EXPECT_EQ(provider(/*index=*/0)
                .registrar_unsafe()
                .GetAppById(app_id)
                ->is_generated_icon(),
            expect_generated_icons);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SingleClientWebAppsSyncGeneratedIconFixSyncTest,
    testing::Combine(/*wait_8_days=*/testing::Bool(),
                     /*sync_broken_icons=*/testing::Bool(),
                     /*trusted_icons_enabled=*/testing::Bool(),
                     /*predictable_app_updates_enabled=*/testing::Bool(),
                     GetSyncTestModes()),
    SingleClientWebAppsSyncGeneratedIconFixSyncTest::ParamToString);

}  // namespace web_app
