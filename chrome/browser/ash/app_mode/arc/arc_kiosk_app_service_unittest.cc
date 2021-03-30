// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_service.h"

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_stop_reason.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/exo/shell_surface_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

namespace ash {

namespace {

const char kAppPackageName[] = "com.app";
const char kAppClassName[] = "Main";
const char kAppEmail[] = "email@app.com";
const char kAppName[] = "Name";
const char kAppWindowAppId[] = "org.chromium.arc.0";

}  // namespace

class FakeController : public KioskAppLauncher::Delegate {
 public:
  explicit FakeController(ArcKioskAppService* service) : service_(service) {
    service_->SetDelegate(this);
  }

  ~FakeController() override { service_->SetDelegate(nullptr); }

  void Reset() {
    window_created_ = false;
    app_prepared_ = false;
  }

  // KioskAppLauncher::Delegate:
  bool IsNetworkReady() const override { return true; }
  bool IsShowingNetworkConfigScreen() const override { return false; }
  bool ShouldSkipAppInstallation() const override { return false; }

  void OnAppWindowCreated() override {
    window_created_ = true;
    if (waiter_)
      waiter_->Quit();
  }

  void OnAppPrepared() override {
    app_prepared_ = true;
    if (waiter_)
      waiter_->Quit();
  }

  void WaitUntilWindowCreated() {
    if (window_created_)
      return;
    waiter_ = std::make_unique<base::RunLoop>();
    waiter_->Run();
  }

  void WaitForAppToBePrepared() {
    if (app_prepared_)
      return;
    waiter_ = std::make_unique<base::RunLoop>();
    waiter_->Run();
  }

  void InitializeNetwork() override {}

 private:
  std::unique_ptr<base::RunLoop> waiter_;
  ArcKioskAppService* service_;

  bool window_created_ = false;
  bool app_prepared_ = false;
};

class ArcKioskAppServiceTest : public testing::Test {
 public:
  ArcKioskAppServiceTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    display::Screen::SetScreenInstance(&test_screen_);

    profile_ = std::make_unique<TestingProfile>();
    profile_->set_profile_name(kAppEmail);
    arc_app_test_.set_persist_service_manager(true);
    arc_app_test_.SetUp(profile_.get());
    arc_policy_bridge_ =
        arc::ArcPolicyBridge::GetForBrowserContextForTesting(profile_.get());
    app_manager_ = std::make_unique<ArcKioskAppManager>();
    // Initialize ArcKioskAppService to listen to ArcKioskAppManager updates.
    ArcKioskAppService::Get(profile());

    app_manager_->AddAutoLaunchAppForTest(
        ArcAppListPrefs::GetAppId(kAppPackageName, kAppClassName),
        policy::ArcKioskAppBasicInfo(kAppPackageName, kAppClassName,
                                     std::string(), kAppName),
        AccountId::FromUserEmail(kAppEmail));
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    profile_.reset();
    display::Screen::SetScreenInstance(nullptr);
  }

  TestingProfile* profile() { return profile_.get(); }

  ArcKioskAppService* service() { return ArcKioskAppService::Get(profile()); }

  arc::mojom::ArcPackageInfoPtr package() {
    auto package = arc::mojom::ArcPackageInfo::New();
    package->package_name = kAppPackageName;
    package->package_version = 1;
    package->last_backup_android_id = 1;
    package->last_backup_time = 1;
    package->sync = false;
    package->system = false;
    package->permissions = base::flat_map<::arc::mojom::AppPermission, bool>();
    return package;
  }

  arc::mojom::AppInfo app_info() {
    arc::mojom::AppInfo app;
    app.package_name = kAppPackageName;
    app.name = kAppName;
    app.activity = kAppClassName;
    app.sticky = true;
    return app;
  }

  void SendComplianceReport() {
    arc_policy_bridge_->ReportCompliance(
        "{}", base::BindOnce([](const std::string&) {}));
  }

  void ExpectAppLaunch(FakeController& controller) {
    // Wait before app is launched.
    controller.WaitForAppToBePrepared();

    launch_requests_++;
    EXPECT_EQ(launch_requests_, app_instance()->launch_requests().size());
    EXPECT_TRUE(app_instance()->launch_requests().back()->IsForApp(app_info()));
    controller.Reset();
    app_instance()->SendTaskCreated(0, app_info(), std::string());
  }

  ArcAppTest& arc_app_test() { return arc_app_test_; }

  arc::FakeAppInstance* app_instance() { return arc_app_test_.app_instance(); }

 private:
  // Number of times app tried to be launched.
  size_t launch_requests_ = 0;

  content::BrowserTaskEnvironment task_environment;
  ArcAppTest arc_app_test_;
  ScopedTestingLocalState testing_local_state_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcKioskAppManager> app_manager_;

  arc::ArcPolicyBridge* arc_policy_bridge_;

  display::test::TestScreen test_screen_;
};

TEST_F(ArcKioskAppServiceTest, LaunchConditions) {
  FakeController controller(service());

  // Load default apps from arc app test.
  app_instance()->SendRefreshPackageList({});
  // App gets installed.
  arc_app_test().AddPackage(package()->Clone());
  // Make app suspended.
  arc::mojom::AppInfo app = app_info();
  app.suspended = true;
  app_instance()->SendPackageAppListRefreshed(kAppPackageName, {app});

  // Send a report which indicates that there were no installation issues.
  SendComplianceReport();

  // App should not be launched since it is suspended.
  EXPECT_EQ(nullptr, service()->GetLauncherForTesting());

  app_instance()->SendRefreshAppList({app_info()});
  // App is installed, compliance report received, app should be launched.
  ExpectAppLaunch(controller);

  // If ARC process stops due to crash or shutdown, app should be terminated.
  service()->OnArcSessionStopped(arc::ArcStopReason::CRASH);
  EXPECT_EQ(nullptr, service()->GetLauncherForTesting());

  // Send a notification which will remind ArcKioskAppService that it should
  // launch the app again.
  SendComplianceReport();
  ExpectAppLaunch(controller);

  // If ARC process stops due to crash or shutdown, app should be terminated.
  service()->OnArcSessionRestarting();
  EXPECT_EQ(nullptr, service()->GetLauncherForTesting());

  // Send a notification which will remind ArcKioskAppService that it should
  // launch the app again.
  SendComplianceReport();
  ExpectAppLaunch(controller);

  // Start maintenance session.
  service()->OnMaintenanceSessionCreated();
  app_instance()->SendTaskDestroyed(0);
  EXPECT_EQ(nullptr, service()->GetLauncherForTesting());

  // The app should be restarted after maintenance is finished.
  service()->OnMaintenanceSessionFinished();
  ExpectAppLaunch(controller);
}

TEST_F(ArcKioskAppServiceTest, AppLaunches) {
  FakeController controller(service());

  // App gets installed.
  arc_app_test().AddPackage(package()->Clone());
  // Make app launchable.
  app_instance()->SendPackageAppListRefreshed(kAppPackageName, {app_info()});

  SendComplianceReport();

  ExpectAppLaunch(controller);

  // Create window of other app which should not be handled.
  auto other_window = std::make_unique<aura::Window>(nullptr);
  other_window->Init(ui::LAYER_SOLID_COLOR);
  other_window.reset();

  auto app_window = std::make_unique<aura::Window>(nullptr);
  app_window->Init(ui::LAYER_SOLID_COLOR);
  exo::SetShellApplicationId(app_window.get(), kAppWindowAppId);

  controller.WaitUntilWindowCreated();
}

}  // namespace ash
