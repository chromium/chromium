// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_service.h"

#include "ash/test/ash_test_helper.h"
#include "ash/test/test_window_builder.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/session/arc_stop_reason.h"
#include "chromeos/ash/experiences/arc/test/fake_app_instance.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using base::test::TestFuture;

namespace {

const char kAppPackageName[] = "com.app";
const char kAppClassName[] = "Main";
const char kAppEmail[] = "email@app.com";
const char kAppName[] = "Name";
const char kAppWindowAppId[] = "org.chromium.arc.0";

}  // namespace

class FakeController : public KioskAppLauncher::NetworkDelegate,
                       public KioskAppLauncher::Observer {
 public:
  explicit FakeController(KioskArcvmAppService* service) : service_(service) {
    kiosk_app_launcher_observation_.Observe(service_);
  }

  ~FakeController() override = default;

  // KioskAppLauncher::Delegate:
  bool IsNetworkReady() const override { return true; }

  void OnAppWindowCreated(const std::optional<std::string>& app_name) override {
    window_created_signal_.SetValue();
  }

  void OnAppPrepared() override { app_prepared_signal_.SetValue(); }

  void WaitUntilWindowCreated() {
    EXPECT_TRUE(window_created_signal_.Wait());
    window_created_signal_.Clear();
  }

  void WaitForAppToBePrepared() {
    EXPECT_TRUE(app_prepared_signal_.Wait());
    app_prepared_signal_.Clear();
  }

  void InitializeNetwork() override {}

 private:
  // TODO(crbug.com/418638940): Refactor to use base::WaitableEvent if possible.
  TestFuture<void> window_created_signal_;
  TestFuture<void> app_prepared_signal_;

  base::ScopedObservation<KioskAppLauncher, KioskAppLauncher::Observer>
      kiosk_app_launcher_observation_{this};

  raw_ptr<KioskArcvmAppService> service_;
};

class KioskArcvmAppServiceTest : public testing::Test {
 public:
  KioskArcvmAppServiceTest() = default;

  void SetUp() override {
    arc_app_test_.set_persist_service_manager(true);
    arc_app_test_.SetUserEmail(kAppEmail);
    arc_app_test_.PreProfileSetUp();

    // TODO(crbug.com/418638940): Refactor to use ChromeAshTestBase.
    ash_test_helper_ = std::make_unique<AshTestHelper>();
    ash_test_helper_->SetUp();

    wm_helper_ = std::make_unique<exo::WMHelper>();

    // KioskArcvmAppManager depends on CrosSettings.
    ASSERT_TRUE(ash::CrosSettings::IsInitialized());
    app_manager_ = std::make_unique<KioskArcvmAppManager>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        &kiosk_cryptohome_remover_);

    profile_ = std::make_unique<TestingProfile>();
    profile_->set_profile_name(kAppEmail);

    arc_app_test_.PostProfileSetUp(profile_.get());

    app_info_ = arc::mojom::AppInfo::New(kAppName, kAppPackageName,
                                         kAppClassName, /*sticky=*/true);
    arc_policy_bridge_ =
        arc::ArcPolicyBridge::GetForBrowserContextForTesting(profile_.get());

    // Initialize KioskArcvmAppService to listen to KioskArcvmAppManager
    // updates.
    KioskArcvmAppService::Get(profile());

    app_manager_->AddAutoLaunchAppForTest(
        ArcAppListPrefs::GetAppId(kAppPackageName, kAppClassName),
        policy::ArcvmKioskAppBasicInfo(kAppPackageName, kAppClassName,
                                       std::string(), kAppName),
        AccountId::FromUserEmail(kAppEmail));
  }

  void TearDown() override {
    arc_policy_bridge_ = nullptr;
    arc_app_test_.PreProfileTearDown();
    profile_.reset();
    app_manager_.reset();
    wm_helper_.reset();
    ash_test_helper_->TearDown();
    ash_test_helper_.reset();
    arc_app_test_.PostProfileTearDown();
  }

  TestingProfile* profile() { return profile_.get(); }

  KioskArcvmAppService* service() {
    return KioskArcvmAppService::Get(profile());
  }

  arc::mojom::ArcPackageInfoPtr package() {
    auto package = arc::mojom::ArcPackageInfo::New();
    package->package_name = kAppPackageName;
    package->package_version = 1;
    package->last_backup_android_id = 1;
    package->last_backup_time = 1;
    package->sync = false;
    return package;
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
    EXPECT_TRUE(
        app_instance()->launch_requests().back()->IsForApp(*app_info()));

    app_instance()->SendTaskCreated(0, *app_info(), std::string());
  }

  arc::mojom::AppInfoPtr& app_info() { return app_info_; }

  ArcAppTest& arc_app_test() { return arc_app_test_; }

  arc::FakeAppInstance* app_instance() { return arc_app_test_.app_instance(); }

  void NotifyWindowCreated(aura::Window* window) {
    wm_helper_->NotifyExoWindowCreated(window);
  }

 private:
  // Number of times app tried to be launched.
  size_t launch_requests_ = 0;

  std::unique_ptr<AshTestHelper> ash_test_helper_;

  content::BrowserTaskEnvironment task_environment;
  ArcAppTest arc_app_test_;

  ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  arc::mojom::AppInfoPtr app_info_;

  KioskCryptohomeRemover kiosk_cryptohome_remover_{
      TestingBrowserProcess::GetGlobal()->local_state()};

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<KioskArcvmAppManager> app_manager_;
  std::unique_ptr<exo::WMHelper> wm_helper_;

  raw_ptr<arc::ArcPolicyBridge> arc_policy_bridge_ = nullptr;
};

TEST_F(KioskArcvmAppServiceTest, LaunchConditions) {
  FakeController controller(service());

  // Load default apps from arc app test.
  app_instance()->SendRefreshPackageList({});
  // App gets installed.
  arc_app_test().AddPackage(package()->Clone());
  // Make app suspended.
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(app_info()->Clone())->suspended = true;

  app_instance()->SendPackageAppListRefreshed(kAppPackageName, apps);

  // Send a report which indicates that there were no installation issues.
  SendComplianceReport();

  // App should not be launched since it is suspended.
  EXPECT_EQ(nullptr, service()->GetLauncherForTesting());

  apps.clear();
  apps.emplace_back(app_info()->Clone());
  app_instance()->SendRefreshAppList(apps);
  // App is installed, compliance report received, app should be launched.
  ExpectAppLaunch(controller);

  // If ARC process stops due to crash or shutdown, app should be terminated.
  service()->OnArcSessionStopped(arc::ArcStopReason::CRASH);
  EXPECT_EQ(nullptr, service()->GetLauncherForTesting());

  // Send a notification which will remind KioskArcvmAppService that it should
  // launch the app again.
  SendComplianceReport();
  ExpectAppLaunch(controller);

  // If ARC process stops due to crash or shutdown, app should be terminated.
  service()->OnArcSessionRestarting();
  EXPECT_EQ(nullptr, service()->GetLauncherForTesting());

  // Send a notification which will remind KioskArcvmAppService that it should
  // launch the app again.
  SendComplianceReport();
  ExpectAppLaunch(controller);
}

TEST_F(KioskArcvmAppServiceTest, AppLaunches) {
  FakeController controller(service());

  // App gets installed.
  arc_app_test().AddPackage(package()->Clone());
  // Make app launchable.
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(app_info()->Clone());
  app_instance()->SendPackageAppListRefreshed(kAppPackageName, apps);

  SendComplianceReport();

  ExpectAppLaunch(controller);

  // Create window of other app which should not be handled.
  auto other_window = std::make_unique<aura::Window>(nullptr);
  other_window->Init(ui::LAYER_SOLID_COLOR);
  other_window.reset();

  TestWindowBuilder window_builder;
  std::unique_ptr<aura::Window> app_window = window_builder.Build();
  exo::SetShellApplicationId(app_window.get(), kAppWindowAppId);
  NotifyWindowCreated(app_window.get());

  controller.WaitUntilWindowCreated();
}

}  // namespace ash
