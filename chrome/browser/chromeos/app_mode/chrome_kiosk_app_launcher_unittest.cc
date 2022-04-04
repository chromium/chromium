// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/test_kiosk_extension_builder.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/api/app_runtime.h"

using base::test::TestFuture;
using extensions::Manifest;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using LaunchResult = ash::ChromeKioskAppLauncher::LaunchResult;

namespace ash {

namespace {

constexpr char kTestPrimaryAppId[] = "abcdefghabcdefghabcdefghabcdefgh";
constexpr char kSecondaryAppId[] = "aaaabbbbaaaabbbbaaaabbbbaaaabbbb";
constexpr char kExtraSecondaryAppId[] = "aaaaccccaaaaccccaaaaccccaaaacccc";

class AppLaunchTracker : public extensions::TestEventRouter::EventObserver {
 public:
  AppLaunchTracker(extensions::TestEventRouter* event_router) {
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
    ASSERT_TRUE(event.event_args);
    ASSERT_EQ(1u, event.event_args->GetListDeprecated().size());

    const base::Value& launch_data = event.event_args->GetListDeprecated()[0];
    const base::Value* is_kiosk_session =
        launch_data.FindKeyOfType("isKioskSession", base::Value::Type::BOOLEAN);
    ASSERT_TRUE(is_kiosk_session);
    EXPECT_TRUE(is_kiosk_session->GetBool());

    launched_apps_.push_back(extension_id);
  }

 private:
  const std::string app_id_;
  base::ScopedObservation<extensions::TestEventRouter,
                          extensions::TestEventRouter::EventObserver,
                          &extensions::TestEventRouter::AddEventObserver,
                          &extensions::TestEventRouter::RemoveEventObserver>
      observation_{this};
  std::vector<std::string> launched_apps_;
};
}  // namespace

class ChromeKioskAppLauncherTest : public extensions::ExtensionServiceTestBase,
                                   extensions::TestEventRouter::EventObserver {
 public:
  // testing::Test:
  void SetUp() override {
    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kForceAppMode);
    command_line_.GetProcessCommandLine()->AppendSwitch(switches::kAppId);

    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    extensions::TestEventRouter* event_router =
        extensions::CreateAndUseTestEventRouter(browser_context());
    app_launch_tracker_ = std::make_unique<AppLaunchTracker>(event_router);
  }

  void TearDown() override {
    app_launch_tracker_.reset();

    extensions::ExtensionServiceTestBase::TearDown();
  }

 protected:
  void CreateLauncher(bool is_network_ready) {
    launcher_ = std::make_unique<ChromeKioskAppLauncher>(
        profile(), kTestPrimaryAppId, is_network_ready);
  }

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

  ASSERT_THAT(future.Get(), Eq(LaunchResult::kSuccess));

  EXPECT_THAT(app_launch_tracker_->launched_apps(),
              ElementsAre(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kTestPrimaryAppId));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kSecondaryAppId));
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kExtraSecondaryAppId));
}

}  // namespace ash
