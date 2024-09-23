// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_service_wrapper.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/app_permissions.mojom.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/child_accounts/apps/app_test_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using web_app::GenerateAppId;
using web_app::WebAppProvider;

namespace ash {
namespace app_time {

namespace {

constexpr char kArcPackage1[] = "com.example.app1";
constexpr char kArcApp1[] = "ArcApp1";
constexpr char kArcPackage2[] = "com.example.app2";
constexpr char kArcApp2[] = "ArcApp2";

constexpr char kExtensionAppUrl[] = "https://example.com/";
constexpr char kExtensionNameChrome[] = "Chrome";
constexpr char kExtensionNameA[] = "ExtensionA";

constexpr char kWebAppUrl1[] = "https://webappone.com/";
constexpr char kWebAppName1[] = "WebApp1";
constexpr char kWebAppUrl2[] = "https://webapptwo.com/";
constexpr char kWebAppName2[] = "WebApp2";

}  // namespace

class AppServiceWrapperTest : public ::testing::Test {
 public:
  class MockListener : public AppServiceWrapper::EventListener {
   public:
    MockListener() = default;
    MockListener(const MockListener&) = delete;
    MockListener& operator=(const MockListener&) = delete;
    ~MockListener() override = default;

    MOCK_METHOD1(OnAppInstalled, void(const AppId& app_id));
    MOCK_METHOD1(OnAppUninstalled, void(const AppId& app_id));
    MOCK_METHOD1(OnAppAvailable, void(const AppId& app_id));
    MOCK_METHOD1(OnAppBlocked, void(const AppId& app_id));
  };

 protected:
  AppServiceWrapperTest() = default;
  AppServiceWrapperTest(const AppServiceWrapperTest&) = delete;
  AppServiceWrapperTest& operator=(const AppServiceWrapperTest&) = delete;
  ~AppServiceWrapperTest() override = default;

  AppServiceWrapper& tested_wrapper() { return tested_wrapper_; }
  MockListener& test_listener() { return test_listener_; }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableDefaultApps);

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_)));
    extension_service_ = extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_->Init();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(&profile_);

    app_service_test_.SetUp(&profile_);
    arc_test_.SetUp(&profile_);
    task_environment_.RunUntilIdle();

    tested_wrapper_.AddObserver(&test_listener_);

    // Install Chrome.
    scoped_refptr<extensions::Extension> chrome = CreateExtension(
        app_constants::kChromeAppId, kExtensionNameChrome, kExtensionAppUrl);
    extension_service_->AddComponentExtension(chrome.get());
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    tested_wrapper_.RemoveObserver(&test_listener_);
    arc_test_.TearDown();

    testing::Test::TearDown();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SimulateAppInstalled(const AppId& app_id,
                            const std::string& app_name,
                            std::optional<std::string> url = std::nullopt) {
    if (app_id.app_type() == apps::AppType::kArc) {
      const std::string& package_name = app_id.app_id();
      arc_test_.AddPackage(CreateArcAppPackage(package_name)->Clone());
      std::vector<arc::mojom::AppInfoPtr> apps;
      apps.emplace_back(CreateArcAppInfo(package_name, app_name));
      arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, apps);
      task_environment_.RunUntilIdle();
      return;
    }

    if (app_id.app_type() == apps::AppType::kChromeApp) {
      scoped_refptr<extensions::Extension> ext =
          CreateExtension(app_id.app_id(), app_name, url.value());
      extension_service_->AddExtension(ext.get());
      task_environment_.RunUntilIdle();
      return;
    }

    if (app_id.app_type() == apps::AppType::kWeb) {
      DCHECK(url.has_value());
      const webapps::AppId installed_app_id = web_app::test::InstallDummyWebApp(
          &profile_, app_name, GURL(url.value()),
          webapps::WebappInstallSource::EXTERNAL_DEFAULT);
      EXPECT_EQ(installed_app_id, app_id.app_id());
      task_environment_.RunUntilIdle();
      return;
    }
  }

  void SimulateAppUninstalled(const AppId& app_id) {
    if (app_id.app_type() == apps::AppType::kArc) {
      const std::string& package_name = app_id.app_id();
      arc_test_.app_instance()->UninstallPackage(package_name);
      task_environment_.RunUntilIdle();
      return;
    }

    if (app_id.app_type() == apps::AppType::kWeb) {
      base::RunLoop run_loop;
      WebAppProvider::GetForTest(&profile_)
          ->scheduler()
          .RemoveInstallManagementMaybeUninstall(
              app_id.app_id(), web_app::WebAppManagement::kDefault,
              webapps::WebappUninstallSource::kExternalPreinstalled,
              base::BindLambdaForTesting(
                  [&](webapps::UninstallResultCode code) {
                    EXPECT_EQ(code, webapps::UninstallResultCode::kAppRemoved);
                    run_loop.Quit();
                  }));
      run_loop.Run();
      task_environment_.RunUntilIdle();
      return;
    }

    if (app_id.app_type() == apps::AppType::kChromeApp ||
        app_id.app_type() == apps::AppType::kWeb) {
      extension_service_->UnloadExtension(
          app_id.app_id(), extensions::UnloadedExtensionReason::UNINSTALL);
      task_environment_.RunUntilIdle();
      return;
    }
  }

  void SimulateAppDisabled(const AppId& app_id,
                           const std::string& app_name,
                           bool disabled) {
    if (app_id.app_type() == apps::AppType::kArc) {
      const std::string& package_name = app_id.app_id();
      std::vector<arc::mojom::AppInfoPtr> apps;
      apps.emplace_back(CreateArcAppInfo(package_name, app_name))->suspended =
          disabled;
      arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, apps);
      task_environment_.RunUntilIdle();
      return;
    }

    if (app_id.app_type() == apps::AppType::kWeb) {
      WebAppProvider::GetForTest(&profile_)->scheduler().SetAppIsDisabled(
          app_id.app_id(), disabled, base::DoNothing());
      task_environment_.RunUntilIdle();
      return;
    }

    if (app_id.app_type() == apps::AppType::kChromeApp ||
        app_id.app_type() == apps::AppType::kWeb) {
      if (disabled) {
        extension_service_->DisableExtension(
            app_id.app_id(),
            extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);
      } else {
        extension_service_->EnableExtension(app_id.app_id());
      }
      task_environment_.RunUntilIdle();
      return;
    }
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;

  raw_ptr<extensions::ExtensionService> extension_service_ = nullptr;

  AppServiceWrapper tested_wrapper_{&profile_};
  MockListener test_listener_;
};

// Tests GetInstalledApps() method.
TEST_F(AppServiceWrapperTest, GetInstalledApps) {
  // Chrome is the only 'preinstalled' app.
  const AppId chrome =
      AppId(apps::AppType::kChromeApp, app_constants::kChromeAppId);
  std::vector<AppId> installed_apps = tested_wrapper().GetInstalledApps();
  EXPECT_EQ(1u, installed_apps.size());
  EXPECT_TRUE(base::Contains(installed_apps, chrome));

  // Add ARC app.
  const AppId app1(apps::AppType::kArc, kArcPackage1);
  EXPECT_CALL(test_listener(), OnAppInstalled(app1)).Times(1);
  SimulateAppInstalled(app1, kArcApp1);

  // Add extension app. It will be ignored, because PATL does not support
  // extensions (with exception of Chrome) now.
  const AppId app2(
      apps::AppType::kChromeApp,
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kExtensionAppUrl)));

  EXPECT_CALL(test_listener(), OnAppInstalled(app2)).Times(1);
  SimulateAppInstalled(app2, kExtensionNameA, kExtensionAppUrl);

  // Add web app.
  const AppId app3(
      apps::AppType::kWeb,
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kWebAppUrl1)));
  EXPECT_CALL(test_listener(), OnAppInstalled(app3)).Times(1);
  SimulateAppInstalled(app3, kWebAppName1, kWebAppUrl1);

  // Expect, chrome, ARC app, hosted extension app and web app to be included.
  const std::vector<AppId> expected_apps = {chrome, app1, app2, app3};
  installed_apps = tested_wrapper().GetInstalledApps();
  ASSERT_EQ(4u, installed_apps.size());
  for (const auto& app : expected_apps) {
    EXPECT_TRUE(base::Contains(installed_apps, app));
  }
}

TEST_F(AppServiceWrapperTest, GetAppName) {
  const AppId chrome(apps::AppType::kChromeApp, app_constants::kChromeAppId);
  EXPECT_EQ(kExtensionNameChrome, tested_wrapper().GetAppName(chrome));

  const AppId app1(apps::AppType::kArc, kArcPackage1);
  EXPECT_CALL(test_listener(), OnAppInstalled(app1)).Times(1);
  SimulateAppInstalled(app1, kArcApp1);

  const AppId app2(
      apps::AppType::kChromeApp,
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kExtensionAppUrl)));

  EXPECT_CALL(test_listener(), OnAppInstalled(app2)).Times(1);
  SimulateAppInstalled(app2, kExtensionNameA, kExtensionAppUrl);

  const AppId app3(
      apps::AppType::kWeb,
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kWebAppUrl1)));
  EXPECT_CALL(test_listener(), OnAppInstalled(app3)).Times(1);
  SimulateAppInstalled(app3, kWebAppName1, kWebAppUrl1);

  EXPECT_EQ(kArcApp1, tested_wrapper().GetAppName(app1));
  EXPECT_EQ(kExtensionNameA, tested_wrapper().GetAppName(app2));
  EXPECT_EQ(kWebAppName1, tested_wrapper().GetAppName(app3));
}

// Tests installs and uninstalls of Arc apps.
TEST_F(AppServiceWrapperTest, ArcAppInstallation) {
  // Only Chrome installed.
  EXPECT_EQ(1u, tested_wrapper().GetInstalledApps().size());

  // Install first ARC app.
  const AppId app1(apps::AppType::kArc, kArcPackage1);
  EXPECT_CALL(test_listener(), OnAppInstalled(app1)).Times(1);
  SimulateAppInstalled(app1, kArcApp1);

  std::vector<AppId> installed_apps = tested_wrapper().GetInstalledApps();
  EXPECT_EQ(2u, installed_apps.size());
  EXPECT_TRUE(base::Contains(installed_apps, app1));

  // Install second ARC app.
  const AppId app2(apps::AppType::kArc, kArcPackage2);
  EXPECT_CALL(test_listener(), OnAppInstalled(app2)).Times(1);
  SimulateAppInstalled(app2, kArcApp2);

  installed_apps = tested_wrapper().GetInstalledApps();
  EXPECT_EQ(3u, installed_apps.size());
  EXPECT_TRUE(base::Contains(installed_apps, app2));

  // Uninstall first ARC app.
  EXPECT_CALL(test_listener(), OnAppUninstalled(app1)).Times(1);
  SimulateAppUninstalled(app1);

  installed_apps = tested_wrapper().GetInstalledApps();
  ASSERT_EQ(2u, installed_apps.size());
  EXPECT_TRUE(base::Contains(installed_apps, app2));
}

// Tests installs and uninstalls of web apps.
TEST_F(AppServiceWrapperTest, WebAppInstallation) {
  // Only Chrome installed.
  EXPECT_EQ(1u, tested_wrapper().GetInstalledApps().size());

  // Install first web app.
  const AppId app1(
      apps::AppType::kWeb,
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kWebAppUrl1)));
  EXPECT_CALL(test_listener(), OnAppInstalled(app1)).Times(1);
  SimulateAppInstalled(app1, kWebAppName1, kWebAppUrl1);

  std::vector<AppId> installed_apps = tested_wrapper().GetInstalledApps();
  EXPECT_EQ(2u, installed_apps.size());
  EXPECT_TRUE(base::Contains(installed_apps, app1));

  // Install second web app.
  const AppId app2(
      apps::AppType::kWeb,
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kWebAppUrl2)));
  EXPECT_CALL(test_listener(), OnAppInstalled(app2)).Times(1);
  SimulateAppInstalled(app2, kWebAppName2, kWebAppUrl2);

  installed_apps = tested_wrapper().GetInstalledApps();
  EXPECT_EQ(3u, installed_apps.size());
  EXPECT_TRUE(base::Contains(installed_apps, app2));

  // Uninstall first web app.
  EXPECT_CALL(test_listener(), OnAppUninstalled(app1)).Times(1);
  SimulateAppUninstalled(app1);

  installed_apps = tested_wrapper().GetInstalledApps();
  ASSERT_EQ(2u, installed_apps.size());
  EXPECT_TRUE(base::Contains(installed_apps, app2));
}

TEST_F(AppServiceWrapperTest, ArcAppDisabled) {
  // Install ARC app.
  const AppId app(apps::AppType::kArc, kArcPackage1);
  EXPECT_CALL(test_listener(), OnAppInstalled(app)).Times(1);
  SimulateAppInstalled(app, kArcApp1);

  // Make app disabled.
  EXPECT_CALL(test_listener(), OnAppBlocked(app)).Times(1);
  SimulateAppDisabled(app, kArcApp1, true);

  // Re-enable app.
  EXPECT_CALL(test_listener(), OnAppAvailable(app)).Times(1);
  SimulateAppDisabled(app, kArcApp1, false);
}

TEST_F(AppServiceWrapperTest, WebAppDisabled) {
  // Install web app.
  const AppId app(
      apps::AppType::kWeb,
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kWebAppUrl1)));
  EXPECT_CALL(test_listener(), OnAppInstalled(app)).Times(1);
  SimulateAppInstalled(app, kWebAppName1, kWebAppUrl1);

  // Make app disabled.
  EXPECT_CALL(test_listener(), OnAppBlocked(app)).Times(1);
  SimulateAppDisabled(app, kWebAppName1, true /*disabled*/);

  // Re-enable app.
  EXPECT_CALL(test_listener(), OnAppAvailable(app)).Times(1);
  SimulateAppDisabled(app, kWebAppName1, false /*disabled*/);
}

// PATL v1 does not support 'extensions' other than Chrome.
TEST_F(AppServiceWrapperTest, IgnoreOtherExtensions) {
  const AppId chrome(apps::AppType::kChromeApp, app_constants::kChromeAppId);
  std::vector<AppId> installed_apps = tested_wrapper().GetInstalledApps();
  EXPECT_TRUE(base::Contains(installed_apps, chrome));

  const AppId app1(
      apps::AppType::kChromeApp,
      GenerateAppId(/*manifest_id=*/std::nullopt, GURL(kExtensionAppUrl)));
  EXPECT_CALL(test_listener(), OnAppInstalled(app1)).Times(1);
  SimulateAppInstalled(app1, kExtensionNameA, kExtensionAppUrl);

  installed_apps = tested_wrapper().GetInstalledApps();
  EXPECT_EQ(2u, installed_apps.size());
  EXPECT_TRUE(base::Contains(installed_apps, chrome));

  // TODO(yilkal): simulate install for non hosted extension apps (such as
  // platform extensions apps, normal extensions, theme extensions for this
  // test)
}

// TODO(agawronska): Add tests for ARC apps activity once crrev.com/c/1906614 is
// landed.

}  // namespace app_time
}  // namespace ash
