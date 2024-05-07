// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_install_task.h"

#include <memory>

#include "ash/components/arc/mojom/webapk.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_webapk_instance.h"
#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/webapk/webapk_metrics.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/apps/app_service/webapk/webapk_test_server.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/webapk/webapk.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestAppActionUrl[] = "https://www.example.com/share";
constexpr char kTestAppIcon[] = "https://www.example.com/icon.png";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kTestShareTextParam[] = "share_text";
const std::u16string kTestAppTitle = u"Test App";

std::unique_ptr<web_app::WebAppInstallInfo> BuildDefaultWebAppInfo() {
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL(kTestAppUrl));
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = kTestAppTitle;
  app_info->manifest_url = GURL(kTestManifestUrl);
  apps::IconInfo icon;
  icon.square_size_px = 64;
  icon.purpose = apps::IconInfo::Purpose::kAny;
  icon.url = GURL(kTestAppIcon);
  app_info->manifest_icons.push_back(icon);

  apps::ShareTarget target;
  target.action = GURL(kTestAppActionUrl);
  target.method = apps::ShareTarget::Method::kPost;
  target.enctype = apps::ShareTarget::Enctype::kMultipartFormData;
  target.params.text = kTestShareTextParam;
  app_info->share_target = target;

  return app_info;
}

arc::mojom::WebApkInfoPtr BuildDefaultWebApkInfo(
    const std::string& package_name,
    const std::string& icon_hash) {
  auto webapk_info = arc::mojom::WebApkInfo::New();
  webapk_info->package_name = package_name;
  webapk_info->apk_version = "1";
  webapk_info->shell_apk_version = "1";
  webapk_info->manifest_url = kTestManifestUrl;
  webapk_info->name = "Test App";
  webapk_info->start_url = kTestAppUrl;
  webapk_info->scope = kTestAppUrl;
  webapk_info->icon_hash = icon_hash;
  auto target_info = arc::mojom::WebShareTargetInfo::New();
  target_info->action = kTestAppActionUrl;
  target_info->method = "POST";
  target_info->enctype = "multipart/form-data";
  target_info->param_text = kTestShareTextParam;
  webapk_info->share_info = std::move(target_info);
  return webapk_info;
}

std::optional<arc::ArcFeatures> GetArcFeaturesWithAbiList(
    std::string abi_list) {
  arc::ArcFeatures arc_features;
  arc_features.build_props.abi_list = abi_list;
  return arc_features;
}

}  // namespace

class WebApkInstallTaskTest : public testing::Test {
  using WebApkResponseBuilder =
      base::RepeatingCallback<std::unique_ptr<net::test_server::HttpResponse>(
          void)>;

 public:
  WebApkInstallTaskTest()
      : task_environment_(content::BrowserTaskEnvironment::MainThreadType::IO) {
  }
  WebApkInstallTaskTest(const WebApkInstallTaskTest&) = delete;
  WebApkInstallTaskTest& operator=(const WebApkInstallTaskTest&) = delete;

  void SetUp() override {
    testing::Test::SetUp();
    app_service_test_.SetUp(&profile_);

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    // Disable WebApkManager by policy. This allows us to unit test
    // WebApkInstallTask without interference from the WebApkManager started by
    // ArcApps.
    profile()->GetPrefs()->SetBoolean(
        apps::webapk_prefs::kGeneratedWebApksEnabled, false);

    arc_test_.SetUp(&profile_);
    auto* arc_bridge_service =
        arc_test_.arc_service_manager()->arc_bridge_service();
    fake_webapk_instance_ = std::make_unique<arc::FakeWebApkInstance>();
    arc_bridge_service->webapk()->SetInstance(fake_webapk_instance_.get());

    net::test_server::RegisterDefaultHandlers(&test_server_);
    webapk_test_server_ = std::make_unique<apps::WebApkTestServer>();
    ASSERT_TRUE(webapk_test_server_->SetUpAndStartServer(&test_server_));

    arc_features_getter_ =
        base::BindRepeating(&GetArcFeaturesWithAbiList, "x86_64");
    arc::ArcFeaturesParser::SetArcFeaturesGetterForTesting(
        &arc_features_getter_);
  }

  void TearDown() override { arc_test_.TearDown(); }

  bool InstallWebApk(std::string app_id) {
    apps::WebApkInstallTask install_task(profile(), app_id);
    base::RunLoop run_loop;
    base::test::TestFuture<bool> future;
    install_task.Start(future.GetCallback());
    bool install_success = future.Get();
    return install_success;
  }

  bool UpdateWebApk(const std::string& app_id) {
    // This is normally set by WebApkManager when an update is queued.
    apps::webapk_prefs::SetUpdateNeededForApp(profile(), app_id,
                                              /* update_needed= */ true);
    return InstallWebApk(app_id);
  }

  TestingProfile* profile() { return &profile_; }

  apps::AppServiceTest* app_service_test() { return &app_service_test_; }

  arc::FakeWebApkInstance* fake_webapk_instance() {
    return fake_webapk_instance_.get();
  }

  webapk::WebApk* last_webapk_request() {
    return webapk_test_server_->last_webapk_request();
  }

  apps::WebApkTestServer* webapk_test_server() {
    return webapk_test_server_.get();
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;

  net::EmbeddedTestServer test_server_;

  std::unique_ptr<arc::FakeWebApkInstance> fake_webapk_instance_;
  std::unique_ptr<apps::WebApkTestServer> webapk_test_server_;
  base::RepeatingCallback<std::optional<arc::ArcFeatures>()>
      arc_features_getter_;
};

TEST_F(WebApkInstallTaskTest, SuccessfulInstall) {
  auto arc_features_getter =
      base::BindRepeating(&GetArcFeaturesWithAbiList, "arm64-v8a,armeabi-v7a");
  arc::ArcFeaturesParser::SetArcFeaturesGetterForTesting(&arc_features_getter);

  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");
  base::HistogramTester histograms;

  EXPECT_TRUE(InstallWebApk(app_id));

  ASSERT_EQ(last_webapk_request()->manifest_url(), kTestManifestUrl);
  ASSERT_EQ(last_webapk_request()->android_abi(), "arm64-v8a");
  const webapk::WebAppManifest& manifest = last_webapk_request()->manifest();
  EXPECT_EQ(manifest.short_name(), "Test App");
  EXPECT_EQ(manifest.start_url(), kTestAppUrl);
  EXPECT_EQ(manifest.icons(0).src(), kTestAppIcon);

  ASSERT_EQ(fake_webapk_instance()->handled_packages().size(), 1u);
  ASSERT_EQ(fake_webapk_instance()->handled_packages().count(
                "org.chromium.webapk.some_package"),
            1u);

  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(profile()),
              testing::ElementsAre(app_id));
  ASSERT_EQ(*apps::webapk_prefs::GetWebApkPackageName(profile(), app_id),
            "org.chromium.webapk.some_package");
  histograms.ExpectBucketCount(apps::kWebApkInstallResultHistogram,
                               apps::WebApkInstallStatus::kSuccess, 1);
}

TEST_F(WebApkInstallTaskTest, ShareTarget) {
  auto web_app_info = BuildDefaultWebAppInfo();

  apps::ShareTarget share_target;
  share_target.action = GURL("https://www.example.com/new");
  share_target.method = apps::ShareTarget::Method::kPost;
  share_target.enctype = apps::ShareTarget::Enctype::kFormUrlEncoded;
  share_target.params.text = "share_text";
  share_target.params.url = "share_url";
  apps::ShareTarget::Files files1;
  files1.name = "images";
  files1.accept = {"image/*"};
  apps::ShareTarget::Files files2;
  files2.name = "videos";
  files2.accept = {"video/mp4", "video/quicktime"};
  share_target.params.files = {files1, files2};
  web_app_info->share_target = share_target;

  auto app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));

  const webapk::WebAppManifest& manifest = last_webapk_request()->manifest();
  EXPECT_EQ(manifest.share_targets(0).action(), "https://www.example.com/new");
  EXPECT_EQ(manifest.share_targets(0).params().text(), "share_text");
  EXPECT_EQ(manifest.share_targets(0).params().url(), "share_url");
  EXPECT_FALSE(manifest.share_targets(0).params().has_title());
  EXPECT_EQ(manifest.share_targets(0).params().files(0).name(), "images");
  EXPECT_EQ(manifest.share_targets(0).params().files(0).accept_size(), 1);
  EXPECT_EQ(manifest.share_targets(0).params().files(0).accept(0), "image/*");
  EXPECT_EQ(manifest.share_targets(0).params().files(1).accept_size(), 2);
}

TEST_F(WebApkInstallTaskTest, NoIconInManifest) {
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL(kTestAppUrl));
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = kTestAppTitle;
  app_info->manifest_url = GURL(kTestManifestUrl);
  auto app_id = web_app::test::InstallWebApp(profile(), std::move(app_info));
  base::HistogramTester histograms;

  ASSERT_FALSE(InstallWebApk(app_id));
  ASSERT_EQ(apps::webapk_prefs::GetWebApkAppIds(profile()).size(), 0u);
  histograms.ExpectBucketCount(apps::kWebApkInstallResultHistogram,
                               apps::WebApkInstallStatus::kAppInvalid, 1);
}

TEST_F(WebApkInstallTaskTest, FailedServerCall) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  webapk_test_server()->RespondWithError();
  base::HistogramTester histograms;

  ASSERT_FALSE(InstallWebApk(app_id));

  ASSERT_EQ(fake_webapk_instance()->handled_packages().size(), 0u);
  ASSERT_EQ(apps::webapk_prefs::GetWebApkAppIds(profile()).size(), 0u);
  histograms.ExpectBucketCount(apps::kWebApkInstallResultHistogram,
                               apps::WebApkInstallStatus::kNetworkError, 1);
}

TEST_F(WebApkInstallTaskTest, FailedArcInstall) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");
  fake_webapk_instance()->set_install_result(
      arc::mojom::WebApkInstallResult::kErrorResolveNetworkError);
  base::HistogramTester histograms;

  ASSERT_FALSE(InstallWebApk(app_id));
  ASSERT_EQ(fake_webapk_instance()->handled_packages().count(
                "org.chromium.webapk.some_package"),
            1u);
  ASSERT_EQ(apps::webapk_prefs::GetWebApkAppIds(profile()).size(), 0u);
  histograms.ExpectBucketCount(apps::kWebApkInstallResultHistogram,
                               apps::WebApkInstallStatus::kGooglePlayError, 1);
}

TEST_F(WebApkInstallTaskTest, MinterTimeout) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebApkServerUrl, test_server()->GetURL("/slow?1000").spec());
  base::HistogramTester histograms;

  apps::WebApkInstallTask install_task(profile(), app_id);
  install_task.SetTimeoutForTesting(base::Milliseconds(100));
  base::test::TestFuture<bool> future;
  install_task.Start(future.GetCallback());
  bool install_success = future.Get();

  ASSERT_FALSE(install_success);
  histograms.ExpectBucketCount(apps::kWebApkInstallResultHistogram,
                               apps::WebApkInstallStatus::kNetworkTimeout, 1);
}

TEST_F(WebApkInstallTaskTest, NoManifestUrl) {
  auto info = BuildDefaultWebAppInfo();
  info->manifest_url = GURL();
  auto app_id = web_app::test::InstallWebApp(profile(), std::move(info));
  base::HistogramTester histograms;

  ASSERT_FALSE(InstallWebApk(app_id));
  histograms.ExpectBucketCount(apps::kWebApkInstallResultHistogram,
                               apps::WebApkInstallStatus::kAppInvalid, 1);
}

TEST_F(WebApkInstallTaskTest, SuccessfulUpdateShortName) {
  // Install an initial app.
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));

  fake_webapk_instance()->set_web_apk_info(BuildDefaultWebApkInfo(
      "org.chromium.webapk.some_package",
      last_webapk_request()->manifest().icons(0).hash()));

  // Install the same app with |short_name| changed. This should trigger an
  // update.
  auto web_app_info = BuildDefaultWebAppInfo();
  web_app_info->title = u"Testy test App";
  web_app::test::InstallWebApp(profile(), std::move(web_app_info),
                               /*overwrite_existing_manifest_fields=*/true);
  EXPECT_TRUE(UpdateWebApk(app_id));

  // Check that the update worked.
  ASSERT_THAT(last_webapk_request()->update_reasons(),
              ::testing::ElementsAre(webapk::WebApk::SHORT_NAME_DIFFERS));
  ASSERT_EQ(last_webapk_request()->package_name(),
            "org.chromium.webapk.some_package");
  ASSERT_EQ(last_webapk_request()->version(), "1");

  webapk::WebAppManifest manifest = last_webapk_request()->manifest();
  EXPECT_EQ(manifest.short_name(), "Testy test App");

  // Check we still only have 1 version of |app_id| installed.
  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(profile()),
              testing::ElementsAre(app_id));
}

TEST_F(WebApkInstallTaskTest, SuccessfulUpdateScope) {
  // Install an initial app.
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));

  fake_webapk_instance()->set_web_apk_info(BuildDefaultWebApkInfo(
      "org.chromium.webapk.some_package",
      last_webapk_request()->manifest().icons(0).hash()));

  // Install the same app with |scope| changed. This should trigger an
  // update.
  auto web_app_info = BuildDefaultWebAppInfo();
  web_app_info->scope = GURL("https://www.differentexample.com/");
  web_app::test::InstallWebApp(profile(), std::move(web_app_info),
                               /*overwrite_existing_manifest_fields=*/true);
  EXPECT_TRUE(UpdateWebApk(app_id));

  // Check that the update worked.
  ASSERT_THAT(last_webapk_request()->update_reasons(),
              ::testing::ElementsAre(webapk::WebApk::SCOPE_DIFFERS));

  webapk::WebAppManifest manifest = last_webapk_request()->manifest();
  EXPECT_EQ(last_webapk_request()->manifest().scopes_size(), 1);
  EXPECT_EQ(last_webapk_request()->manifest().scopes(0),
            "https://www.differentexample.com/");

  // Check we still only have 1 version of |app_id| installed.
  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(profile()),
              testing::ElementsAre(app_id));
}

TEST_F(WebApkInstallTaskTest, SuccessfulUpdateIconHash) {
  // Install an initial app.
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));

  // Change icon hash.
  fake_webapk_instance()->set_web_apk_info(BuildDefaultWebApkInfo(
      "org.chromium.webapk.some_package", "fakeiconhash123456789"));

  auto web_app_info = BuildDefaultWebAppInfo();
  web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  EXPECT_TRUE(UpdateWebApk(app_id));

  // Check that the update worked.
  ASSERT_THAT(
      last_webapk_request()->update_reasons(),
      ::testing::ElementsAre(webapk::WebApk::PRIMARY_ICON_HASH_DIFFERS));
  ASSERT_TRUE(last_webapk_request()->app_identity_update_supported());

  // Check we still only have 1 version of |app_id| installed.
  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(profile()),
              testing::ElementsAre(app_id));
}

TEST_F(WebApkInstallTaskTest, SuccessfulUpdateShareTarget) {
  // Install an initial app.
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));
  fake_webapk_instance()->set_web_apk_info(BuildDefaultWebApkInfo(
      "org.chromium.webapk.some_package",
      last_webapk_request()->manifest().icons(0).hash()));

  // Install the same app with |share_target| changed. This should trigger an
  // update.
  auto web_app_info = BuildDefaultWebAppInfo();
  web_app_info->share_target->action =
      GURL("https://www.differentexample.com/");
  web_app::test::InstallWebApp(profile(), std::move(web_app_info),
                               /*overwrite_existing_manifest_fields=*/true);
  EXPECT_TRUE(UpdateWebApk(app_id));

  // Check that the update worked.
  ASSERT_THAT(last_webapk_request()->update_reasons(),
              ::testing::ElementsAre(webapk::WebApk::WEB_SHARE_TARGET_DIFFERS));

  webapk::WebAppManifest manifest = last_webapk_request()->manifest();
  EXPECT_EQ(manifest.share_targets(0).action(),
            "https://www.differentexample.com/");

  // Check we still only have 1 version of |app_id| installed.
  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(profile()),
              testing::ElementsAre(app_id));
}

TEST_F(WebApkInstallTaskTest, SuccessfulUpdateMultipleChanges) {
  // Install an initial app.
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));

  fake_webapk_instance()->set_web_apk_info(BuildDefaultWebApkInfo(
      "org.chromium.webapk.some_package",
      last_webapk_request()->manifest().icons(0).hash()));

  auto web_app_info = BuildDefaultWebAppInfo();
  web_app_info->title = u"Testy test App";
  web_app_info->share_target->action =
      GURL("https://www.differentexample.com/");
  web_app::test::InstallWebApp(profile(), std::move(web_app_info),
                               /*overwrite_existing_manifest_fields=*/true);
  base::HistogramTester histograms;
  EXPECT_TRUE(UpdateWebApk(app_id));

  ASSERT_THAT(last_webapk_request()->update_reasons(),
              ::testing::UnorderedElementsAre(
                  webapk::WebApk::SHORT_NAME_DIFFERS,
                  webapk::WebApk::WEB_SHARE_TARGET_DIFFERS));

  webapk::WebAppManifest manifest = last_webapk_request()->manifest();
  EXPECT_EQ(manifest.short_name(), "Testy test App");
  EXPECT_EQ(manifest.share_targets(0).action(),
            "https://www.differentexample.com/");

  // Check we still only have 1 version of |app_id| installed.
  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(profile()),
              testing::ElementsAre(app_id));
  ASSERT_THAT(apps::webapk_prefs::GetUpdateNeededAppIds(profile()),
              testing::IsEmpty());
  histograms.ExpectBucketCount(apps::kWebApkUpdateResultHistogram,
                               apps::WebApkInstallStatus::kSuccess, 1);
}

TEST_F(WebApkInstallTaskTest, AbandonedUpdateNoChanges) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");
  EXPECT_TRUE(InstallWebApk(app_id));
  fake_webapk_instance()->set_web_apk_info(BuildDefaultWebApkInfo(
      "org.chromium.webapk.some_package",
      last_webapk_request()->manifest().icons(0).hash()));

  // Install the same app with no changes. This should fail.
  base::HistogramTester histograms;
  EXPECT_FALSE(UpdateWebApk(app_id));
  histograms.ExpectBucketCount(
      apps::kWebApkUpdateResultHistogram,
      apps::WebApkInstallStatus::kUpdateCancelledWebApkUpToDate, 1);
  // Update should no longer be needed.
  ASSERT_THAT(apps::webapk_prefs::GetUpdateNeededAppIds(profile()),
              testing::IsEmpty());
}

TEST_F(WebApkInstallTaskTest, FailedUpdateWebApkInfoInvalid) {
  // Install an initial app.
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));

  // Install the same app without setting web apk info. Install should fail.
  base::HistogramTester histograms;
  EXPECT_FALSE(UpdateWebApk(app_id));
  histograms.ExpectBucketCount(
      apps::kWebApkUpdateResultHistogram,
      apps::WebApkInstallStatus::kUpdateGetWebApkInfoError, 1);
}

TEST_F(WebApkInstallTaskTest, FailedUpdateNetworkError) {
  // Install an initial app.
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));

  fake_webapk_instance()->set_web_apk_info(BuildDefaultWebApkInfo(
      "org.chromium.webapk.some_package",
      last_webapk_request()->manifest().icons(0).hash()));

  // Install the same app with |short_name| changed. This should trigger an
  // update.
  auto web_app_info = BuildDefaultWebAppInfo();
  web_app_info->title = u"Testy test App";
  web_app::test::InstallWebApp(profile(), std::move(web_app_info),
                               /*overwrite_existing_manifest_fields=*/true);

  base::HistogramTester histograms;
  webapk_test_server()->RespondWithError();

  ASSERT_FALSE(UpdateWebApk(app_id));

  histograms.ExpectBucketCount(apps::kWebApkUpdateResultHistogram,
                               apps::WebApkInstallStatus::kNetworkError, 1);
  // Check that the app is still installed and still needs an update.
  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(profile()),
              testing::ElementsAre(app_id));
  ASSERT_THAT(apps::webapk_prefs::GetUpdateNeededAppIds(profile()),
              testing::ElementsAre(app_id));
}

TEST_F(WebApkInstallTaskTest, SingleAbi) {
  auto arc_features_getter =
      base::BindRepeating(&GetArcFeaturesWithAbiList, "armeabi-v7a");
  arc::ArcFeaturesParser::SetArcFeaturesGetterForTesting(&arc_features_getter);

  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  webapk_test_server()->RespondWithSuccess("org.chromium.webapk.some_package");

  EXPECT_TRUE(InstallWebApk(app_id));

  ASSERT_EQ(last_webapk_request()->android_abi(), "armeabi-v7a");
}
