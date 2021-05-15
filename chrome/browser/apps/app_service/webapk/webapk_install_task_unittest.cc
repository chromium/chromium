// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_install_task.h"

#include <memory>

#include "base/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/webapk.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/fake_webapk_instance.h"
#include "components/webapk/webapk.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestAppActionUrl[] = "https://www.example.com/share";
constexpr char kTestAppIcon[] = "https://www.example.com/icon.png";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kTestShareTextParam[] = "share_text";
const std::u16string kTestAppTitle = u"Test App";

constexpr char kServerPath[] = "/webapk";

constexpr char kToken[] = "opaque token";

std::unique_ptr<net::test_server::HttpResponse> BuildValidWebApkResponse(
    std::string package_name) {
  auto webapk_response = std::make_unique<webapk::WebApkResponse>();
  webapk_response->set_package_name(std::move(package_name));
  webapk_response->set_version("1");
  webapk_response->set_token(kToken);

  std::string response_content;
  webapk_response->SerializeToString(&response_content);

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(response_content);

  return response;
}

std::unique_ptr<net::test_server::HttpResponse> BuildFailedResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_BAD_REQUEST);
  return response;
}

std::unique_ptr<WebApplicationInfo> BuildDefaultWebAppInfo() {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->start_url = GURL(kTestAppUrl);
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = kTestAppTitle;
  app_info->manifest_url = GURL(kTestManifestUrl);
  WebApplicationIconInfo icon;
  icon.square_size_px = 64;
  icon.purpose = IconPurpose::ANY;
  icon.url = GURL(kTestAppIcon);
  app_info->icon_infos.push_back(icon);

  apps::ShareTarget target;
  target.action = GURL(kTestAppActionUrl);
  target.method = apps::ShareTarget::Method::kPost;
  target.enctype = apps::ShareTarget::Enctype::kMultipartFormData;
  target.params.text = kTestShareTextParam;
  app_info->share_target = target;

  return app_info;
}

absl::optional<arc::ArcFeatures> GetArcFeaturesWithAbiList(
    const std::string& abi_list) {
  arc::ArcFeatures arc_features;
  arc_features.build_props["ro.product.cpu.abilist"] = abi_list;
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

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_)));
    extension_service_ = extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_->Init();

    app_service_test_.SetUp(&profile_);

    auto* const provider = web_app::TestWebAppProvider::Get(&profile_);
    provider->SetRunSubsystemStartupTasks(true);
    provider->Start();

    arc_test_.SetUp(&profile_);
    auto* arc_bridge_service =
        arc_test_.arc_service_manager()->arc_bridge_service();
    fake_webapk_instance_ = std::make_unique<arc::FakeWebApkInstance>();
    arc_bridge_service->webapk()->SetInstance(fake_webapk_instance_.get());

    app_service_test_.FlushMojoCalls();

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &WebApkInstallTaskTest::HandleWebApkRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    GURL server_url = test_server_.GetURL(kServerPath);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWebApkServerUrl, server_url.spec());

    arc_features_getter_ =
        base::BindRepeating(&GetArcFeaturesWithAbiList, "x86_64");
    arc::ArcFeaturesParser::SetArcFeaturesGetterForTesting(
        &arc_features_getter_);
  }

  void SetWebApkResponse(WebApkResponseBuilder builder) {
    webapk_response_builder_ = builder;
  }

  bool InstallWebApk(std::string app_id) {
    bool install_success;
    apps::WebApkInstallTask install_task(profile(), app_id);
    base::RunLoop run_loop;
    install_task.Start(base::BindLambdaForTesting([&](bool success) {
      install_success = success;
      run_loop.Quit();
    }));
    run_loop.Run();
    return install_success;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleWebApkRequest(
      const net::test_server::HttpRequest& request) {
    last_webapk_request_ = std::make_unique<webapk::WebApk>();
    last_webapk_request_->ParseFromString(request.content);
    return webapk_response_builder_.Run();
  }

  TestingProfile* profile() { return &profile_; }

  apps::AppServiceTest* app_service_test() { return &app_service_test_; }

  arc::FakeWebApkInstance* fake_webapk_instance() {
    return fake_webapk_instance_.get();
  }

  webapk::WebApk* last_webapk_request() { return last_webapk_request_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;
  extensions::ExtensionService* extension_service_ = nullptr;

  net::EmbeddedTestServer test_server_;

  std::unique_ptr<arc::FakeWebApkInstance> fake_webapk_instance_;
  WebApkResponseBuilder webapk_response_builder_;
  std::unique_ptr<webapk::WebApk> last_webapk_request_;
  base::RepeatingCallback<absl::optional<arc::ArcFeatures>()>
      arc_features_getter_;
};

TEST_F(WebApkInstallTaskTest, SuccessfulInstall) {
  auto arc_features_getter =
      base::BindRepeating(&GetArcFeaturesWithAbiList, "arm64-v8a,armeabi-v7a");
  arc::ArcFeaturesParser::SetArcFeaturesGetterForTesting(&arc_features_getter);

  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  SetWebApkResponse(base::BindRepeating(&BuildValidWebApkResponse,
                                        "org.chromium.webapk.some_package"));

  EXPECT_TRUE(InstallWebApk(app_id));

  ASSERT_EQ(last_webapk_request()->manifest_url(), kTestManifestUrl);
  ASSERT_EQ(last_webapk_request()->android_abi(), "arm64-v8a");
  const webapk::WebAppManifest& manifest = last_webapk_request()->manifest();
  EXPECT_EQ(manifest.short_name(), "Test App");
  EXPECT_EQ(manifest.start_url(), kTestAppUrl);
  EXPECT_EQ(manifest.icons(0).src(), kTestAppIcon);

  ASSERT_EQ(fake_webapk_instance()->handled_packages().size(), 1);
  ASSERT_EQ(fake_webapk_instance()->handled_packages()[0],
            "org.chromium.webapk.some_package");

  base::flat_set<std::string> installed_webapks =
      apps::webapk_prefs::GetWebApkAppIds(profile());
  ASSERT_EQ(installed_webapks.size(), 1);
  ASSERT_TRUE(installed_webapks.contains(app_id));
  ASSERT_EQ(*apps::webapk_prefs::GetWebApkPackageName(profile(), app_id),
            "org.chromium.webapk.some_package");
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

  SetWebApkResponse(base::BindRepeating(&BuildValidWebApkResponse,
                                        "org.chromium.webapk.some_package"));

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
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->start_url = GURL(kTestAppUrl);
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = kTestAppTitle;
  app_info->manifest_url = GURL(kTestManifestUrl);
  auto app_id = web_app::test::InstallWebApp(profile(), std::move(app_info));

  ASSERT_FALSE(InstallWebApk(app_id));
  ASSERT_EQ(apps::webapk_prefs::GetWebApkAppIds(profile()).size(), 0);
}

TEST_F(WebApkInstallTaskTest, FailedServerCall) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  SetWebApkResponse(base::BindRepeating(&BuildFailedResponse));

  ASSERT_FALSE(InstallWebApk(app_id));

  ASSERT_EQ(fake_webapk_instance()->handled_packages().size(), 0);
  ASSERT_EQ(apps::webapk_prefs::GetWebApkAppIds(profile()).size(), 0);
}

TEST_F(WebApkInstallTaskTest, FailedArcInstall) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  SetWebApkResponse(base::BindRepeating(&BuildValidWebApkResponse,
                                        "org.chromium.webapk.some_package"));
  fake_webapk_instance()->set_install_result(
      arc::mojom::WebApkInstallResult::kErrorResolveNetworkError);

  ASSERT_FALSE(InstallWebApk(app_id));
  ASSERT_EQ(fake_webapk_instance()->handled_packages()[0],
            "org.chromium.webapk.some_package");
  ASSERT_EQ(apps::webapk_prefs::GetWebApkAppIds(profile()).size(), 0);
}
