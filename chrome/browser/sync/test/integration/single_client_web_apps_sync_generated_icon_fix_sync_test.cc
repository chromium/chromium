// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
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
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {
using Param = std::tuple<bool /*wait_8_days*/,
                         bool /*sync_broken_icons*/,
                         bool /*trusted_icons_enabled*/>;
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
    });
  }

  SingleClientWebAppsSyncGeneratedIconFixSyncTest()
      : WebAppsSyncTestBase(SINGLE_CLIENT) {
    if (trusted_icons_enabled()) {
      feature_list_.InitAndEnableFeature(features::kWebAppUsePrimaryIcon);
    } else {
      feature_list_.InitAndDisableFeature(features::kWebAppUsePrimaryIcon);
    }
  }

  ~SingleClientWebAppsSyncGeneratedIconFixSyncTest() override = default;

  bool wait_8_days() const { return std::get<0>(GetParam()); }
  bool sync_broken_icons() const { return std::get<1>(GetParam()); }
  bool trusted_icons_enabled() const { return std::get<2>(GetParam()); }

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
  }

 protected:
  std::atomic<bool> serve_pngs_ = true;

 private:
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
  net::test_server::EmbeddedTestServerHandle embedded_test_server_handle_;
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
  sync_pb::EntitySpecifics specifics;
  sync_pb::WebAppSpecifics& web_app_specifics = *specifics.mutable_web_app();
  web_app_specifics.set_start_url(start_url.spec());
  web_app_specifics.set_user_display_mode_default(
      sync_pb::WebAppSpecifics::STANDALONE);
  web_app_specifics.set_name("Basic web app");
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/app_id,
          /*client_tag=*/app_id, specifics,
          /*creation_time=*/0, /*last_modified_time=*/0));

  // Await sync install.
  EXPECT_EQ(install_observer.Wait(), app_id);

  // Icons should be generated always now that sync follows the fallback
  // installation path once the trusted icons architecture lands.
  EXPECT_EQ(
      provider(0).registrar_unsafe().GetAppById(app_id)->is_generated_icon(),
      trusted_icons_enabled() || sync_broken_icons());

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

  // Check icons fixed in time window, provided trusted icons architecture is
  // not enabled. With trusted icons enabled, sync installs always install from
  // fallback and have generated icons, so the fix is always applied as part of
  // the manifest update process.
  bool expect_fix_applied =
      (trusted_icons_enabled() || (!wait_8_days() && sync_broken_icons()));

  // The only time generated icons are still expected are:
  // 1. The fix is not applied (like if it's more than 8 days which is the
  // threshold for the GeneratedIconFixManager).
  // 2. If there is a generated icon in the first case, which can either be from
  // the trusted icon infrastructure installing from empty icons in this case,
  // or if broken icons were synced.
  bool expect_generated_icons =
      wait_8_days() && (trusted_icons_enabled() || sync_broken_icons());
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
        /*wait_8_days=*/testing::Bool(),
        /*sync_broken_icons=*/testing::Bool(),
        /*trusted_icons_enabled=*/testing::Bool()),
    SingleClientWebAppsSyncGeneratedIconFixSyncTest::ParamToString);

}  // namespace web_app
