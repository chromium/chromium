// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {
using Param = std::tuple<bool /*flag_enabled*/,
                         bool /*wait_8_days*/,
                         bool /*sync_broken_icons*/>;
}

class SingleClientWebAppsSyncGeneratedIconFixSyncTest
    : public WebAppsSyncTestBase,
      public testing::WithParamInterface<Param> {
 public:
  static std::string ParamToString(testing::TestParamInfo<Param> param) {
    return base::StrCat({
        std::get<0>(param.param) ? "FlagEnabled" : "FlagDisabled",
        "_",
        std::get<1>(param.param) ? "Wait8Days" : "NoWait",
        "_",
        std::get<2>(param.param) ? "SyncBrokenIcons" : "SyncNormalIcons",
    });
  }

  SingleClientWebAppsSyncGeneratedIconFixSyncTest()
      : WebAppsSyncTestBase(SINGLE_CLIENT) {
    if (flag_enabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kWebAppSyncGeneratedIconUpdateFix);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kWebAppSyncGeneratedIconUpdateFix);
    }
  }

  ~SingleClientWebAppsSyncGeneratedIconFixSyncTest() override = default;

  bool flag_enabled() const { return std::get<0>(GetParam()); }
  bool wait_8_days() const { return std::get<1>(GetParam()); }
  bool sync_broken_icons() const { return std::get<2>(GetParam()); }

  WebAppProvider& provider(int index) {
    return *WebAppProvider::GetForTest(GetProfile(index));
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();
    ASSERT_TRUE(SetupClients());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Apps sync is controlled by a dedicated preference on Lacros,
    // corresponding to the Apps toggle in OS Sync settings.
    if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
      GetSyncService(0)->GetUserSettings()->SetAppsSyncEnabledByOs(true);
    }
#endif
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
  }

 protected:
  std::atomic<bool> serve_pngs_ = true;

 private:
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServerHandle embedded_test_server_handle_;
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
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  sync_pb::EntitySpecifics specifics;
  sync_pb::WebAppSpecifics& web_app_specifics = *specifics.mutable_web_app();
  web_app_specifics.set_start_url(start_url.spec());
  web_app_specifics.set_user_display_mode_default(
      sync_pb::WebAppSpecifics::STANDALONE);
  web_app_specifics.set_name("Basic web app");
  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/app_id,
          /*client_tag=*/app_id, specifics,
          /*creation_time=*/0, /*last_modified_time=*/0));

  // Await sync install.
  EXPECT_EQ(install_observer.Wait(), app_id);

  // Icons should be generated if icon downloading was disabled.
  EXPECT_EQ(
      provider(0).registrar_unsafe().GetAppById(app_id)->is_generated_icon(),
      sync_broken_icons());

  // Ensure installed locally to enable manifest updating.
  {
    base::RunLoop run_loop;
    provider(0).scheduler().InstallAppLocally(app_id, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Re-enable icons if disabled.
  serve_pngs_.store(true);

  if (wait_8_days()) {
    // Advance time beyond the fix time window.
    generated_icon_fix_util::SetNowForTesting(base::Time::Now() +
                                              base::Days(8));
  }

  // Trigger manifest update.
  base::test::TestFuture<const GURL&, ManifestUpdateResult> update_future;
  ManifestUpdateManager::SetResultCallbackForTesting(
      update_future.GetCallback());
  ASSERT_TRUE(AddTabAtIndexToBrowser(GetBrowser(0), 0, start_url,
                                     ui::PAGE_TRANSITION_AUTO_TOPLEVEL));
  std::optional<ManifestUpdateResult> update_result = update_future.Get<1>();

  // Check icons fixed if flag enabled and in time window.
  bool expect_fix_applied =
      flag_enabled() && !wait_8_days() && sync_broken_icons();
  bool expect_generated_icons = !expect_fix_applied && sync_broken_icons();
  EXPECT_EQ(update_result, expect_fix_applied
                               ? ManifestUpdateResult::kAppUpdated
                               : ManifestUpdateResult::kAppUpToDate);
  EXPECT_EQ(
      provider(0).registrar_unsafe().GetAppById(app_id)->is_generated_icon(),
      expect_generated_icons);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SingleClientWebAppsSyncGeneratedIconFixSyncTest,
    testing::Combine(
        /*flag_enabled=*/testing::Values(true, false),
        /*wait_8_days=*/testing::Values(true, false),
        /*sync_broken_icons=*/testing::Values(true, false)),
    SingleClientWebAppsSyncGeneratedIconFixSyncTest::ParamToString);

}  // namespace web_app
