// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"

#include <memory>
#include <vector>

#include "ash/test/ash_test_helper.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_mode/test_kiosk_extension_builder.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/api/app_runtime.h"

using base::test::TestFuture;
using extensions::Manifest;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using LaunchResult = chromeos::ChromeKioskAppLauncher::LaunchResult;
using chromeos::ChromeKioskAppLauncher;

namespace ash {

namespace {

constexpr char kTestPrimaryAppId[] = "abcdefghabcdefghabcdefghabcdefgh";
constexpr char kSecondaryAppId[] = "aaaabbbbaaaabbbbaaaabbbbaaaabbbb";
constexpr char kExtraSecondaryAppId[] = "aaaaccccaaaaccccaaaaccccaaaacccc";

class AppLaunchTracker : public extensions::TestEventRouter::EventObserver {
 public:
  explicit AppLaunchTracker(extensions::TestEventRouter* event_router) {
    observation_.Observe(event_router);
  }
  AppLaunchTracker(const AppLaunchTracker&) = delete;
  AppLaunchTracker& operator=(const AppLaunchTracker&) = delete;

  std::vector<std::string> launched_apps() const { return launched_apps_; }

  // TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override {
    ADD_FAILURE() << "Unexpected broadcast " << event.event_name;
  }

  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const extensions::Event& event) override {
    ASSERT_EQ(event.event_name,
              extensions::api::app_runtime::OnLaunched::kEventName);
    ASSERT_EQ(1u, event.event_args.size());

    const base::Value::Dict& launch_data = event.event_args[0].GetDict();
    std::optional<bool> is_kiosk_session =
        launch_data.FindBool("isKioskSession");
    ASSERT_TRUE(is_kiosk_session);
    EXPECT_TRUE(*is_kiosk_session);

    launched_apps_.push_back(extension_id);
  }

 private:
  const std::string app_id_;
  base::ScopedObservation<extensions::TestEventRouter,
                          extensions::TestEventRouter::EventObserver>
      observation_{this};
  std::vector<std::string> launched_apps_;
};

void InitAppWindow(extensions::AppWindow* app_window, const gfx::Rect& bounds) {
  // Create a TestAppWindowContents for the ShellAppDelegate to initialize the
  // ShellExtensionWebContentsObserver with.
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(app_window->browser_context())));
  auto app_window_contents =
      std::make_unique<extensions::TestAppWindowContents>(
          std::move(web_contents));

  // Initialize the web contents and AppWindow.
  app_window->app_delegate()->InitWebContents(
      app_window_contents->GetWebContents());

  content::RenderFrameHost* main_frame =
      app_window_contents->GetWebContents()->GetPrimaryMainFrame();
  DCHECK(main_frame);

  extensions::AppWindow::CreateParams params;
  params.content_spec.bounds = bounds;
  app_window->Init(GURL(), std::move(app_window_contents), main_frame, params);
}

extensions::AppWindow* CreateAppWindow(Profile* profile,
                                       const extensions::Extension* extension,
                                       gfx::Rect bounds = {}) {
  extensions::AppWindow* app_window = new extensions::AppWindow(
      profile, std::make_unique<ChromeAppDelegate>(profile, true), extension);
  InitAppWindow(app_window, bounds);
  return app_window;
}

}  // namespace

class ChromeKioskAppLauncherTest : public extensions::ExtensionServiceTestBase,
                                   extensions::TestEventRouter::EventObserver {
 public:
  ChromeKioskAppLauncherTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                content::BrowserTaskEnvironment::REAL_IO_THREAD)) {}

  // testing::Test:
  void SetUp() override {
    ash_test_helper_.SetUp(ash::AshTestHelper::InitParams());

    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kForceAppMode);
    command_line_.GetProcessCommandLine()->AppendSwitch(switches::kAppId);

    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    apps::WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(profile()));

    extensions::TestEventRouter* event_router =
        extensions::CreateAndUseTestEventRouter(browser_context());
    app_launch_tracker_ = std::make_unique<AppLaunchTracker>(event_router);
  }

  void TearDown() override {
    app_launch_tracker_.reset();

    extensions::ExtensionServiceTestBase::TearDown();
    ash_test_helper_.TearDown();
  }

 protected:
  void CreateLauncher(bool is_network_ready) {
    launcher_ = std::make_unique<ChromeKioskAppLauncher>(
        profile(), kTestPrimaryAppId, is_network_ready);
  }

  void SimulateAppWindowLaunch(const extensions::Extension* extension) {
    CreateAppWindow(profile(), extension);
  }

  ash::AshTestHelper ash_test_helper_;
  std::unique_ptr<ChromeKioskAppLauncher> launcher_;
  std::unique_ptr<AppLaunchTracker> app_launch_tracker_;

 private:
  base::test::ScopedCommandLine command_line_;
};

TEST_F(ChromeKioskAppLauncherTest, ShouldFailIfPrimaryAppNotInstalled) {
  CreateLauncher(/*is_network_ready=*/true);

  TestFuture<LaunchResult> future;
  launcher_->LaunchApp(future.GetCallback());

  ASSERT_THAT(future.Get(), Eq(LaunchResult::kUnableToLaunch));
  ASSERT_THAT(app_launch_tracker_->launched_apps(), IsEmpty());
}

TEST_F(ChromeKioskAppLauncherTest, ShouldFailIfSecondaryAppNotInstalled) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_version("1.0");
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  CreateLauncher(/*is_network_ready=*/true);

  TestFuture<LaunchResult> future;
  launcher_->LaunchApp(future.GetCallback());

  ASSERT_THAT(future.Get(), Eq(LaunchResult::kUnableToLaunch));
  ASSERT_THAT(app_launch_tracker_->launched_apps(), IsEmpty());
}

TEST_F(ChromeKioskAppLauncherTest,
       ShouldReportNetworkMissingIfAppNotOfflineEnabled) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_version("1.0");
  primary_app_builder.set_offline_enabled(false);
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  CreateLauncher(/*is_network_ready=*/false);

  TestFuture<LaunchResult> future;
  launcher_->LaunchApp(future.GetCallback());

  ASSERT_THAT(future.Get(), Eq(LaunchResult::kNetworkMissing));
  ASSERT_THAT(app_launch_tracker_->launched_apps(), IsEmpty());
}

TEST_F(ChromeKioskAppLauncherTest, ShouldSucceedIfNetworkAvailable) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_version("1.0");
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  CreateLauncher(/*is_network_ready=*/true);

  TestFuture<LaunchResult> future;
  launcher_->LaunchApp(future.GetCallback());

  SimulateAppWindowLaunch(primary_app.get());

  ASSERT_THAT(future.Get(), Eq(LaunchResult::kSuccess));

  EXPECT_THAT(app_launch_tracker_->launched_apps(),
              ElementsAre(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
}

TEST_F(ChromeKioskAppLauncherTest, ShouldSucceedWithSecondaryApp) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_version("1.0");
  primary_app_builder.AddSecondaryExtension(kSecondaryAppId);
  primary_app_builder.AddSecondaryExtensionWithEnabledOnLaunch(
      kExtraSecondaryAppId, false);
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  TestKioskExtensionBuilder secondary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                  kSecondaryAppId);
  secondary_app_builder.set_kiosk_enabled(false);
  scoped_refptr<const extensions::Extension> secondary_app =
      secondary_app_builder.Build();
  service()->AddExtension(secondary_app.get());

  TestKioskExtensionBuilder extra_secondary_app_builder(
      Manifest::TYPE_PLATFORM_APP, kExtraSecondaryAppId);
  extra_secondary_app_builder.set_kiosk_enabled(false);
  scoped_refptr<const extensions::Extension> extra_secondary_app =
      extra_secondary_app_builder.Build();
  service()->AddExtension(extra_secondary_app.get());

  CreateLauncher(/*is_network_ready=*/true);

  TestFuture<LaunchResult> future;
  launcher_->LaunchApp(future.GetCallback());

  SimulateAppWindowLaunch(primary_app.get());

  ASSERT_THAT(future.Get(), Eq(LaunchResult::kSuccess));

  EXPECT_THAT(app_launch_tracker_->launched_apps(),
              ElementsAre(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtraSecondaryAppId));
}

TEST_F(ChromeKioskAppLauncherTest, ShouldSucceedWithAppService) {
  TestKioskExtensionBuilder primary_app_builder(Manifest::TYPE_PLATFORM_APP,
                                                kTestPrimaryAppId);
  primary_app_builder.set_version("1.0");
  scoped_refptr<const extensions::Extension> primary_app =
      primary_app_builder.Build();
  service()->AddExtension(primary_app.get());

  CreateLauncher(/*is_network_ready=*/true);

  TestFuture<LaunchResult> future;
  launcher_->LaunchApp(future.GetCallback());

  SimulateAppWindowLaunch(primary_app.get());

  ASSERT_THAT(future.Get(), Eq(LaunchResult::kSuccess));

  EXPECT_THAT(app_launch_tracker_->launched_apps(),
              ElementsAre(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
}

}  // namespace ash
