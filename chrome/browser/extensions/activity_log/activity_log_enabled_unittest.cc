// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

const char kExtensionID[] = "eplckmlabaanikjjcgnigddmagoglhmp";

class ActivityLogEnabledTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetActivityLogTaskRunnerForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    SetActivityLogTaskRunnerForTesting(nullptr);
  }
};

TEST_F(ActivityLogEnabledTest, NoSwitch) {
  std::unique_ptr<TestingProfile> profile(CreateTestingProfile());
  EXPECT_FALSE(
      profile->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));

  ActivityLog* activity_log = ActivityLog::GetInstance(profile.get());

  EXPECT_EQ(0,
    profile->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log->IsWatchdogAppActive());
}

TEST_F(ActivityLogEnabledTest, CommandLineSwitch) {
  std::unique_ptr<TestingProfile> profile1(CreateTestingProfile());
  std::unique_ptr<TestingProfile> profile2(CreateTestingProfile());

  ActivityLog* activity_log1;
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableExtensionActivityLogging);
    activity_log1 = ActivityLog::GetInstance(profile1.get());
  }
  ActivityLog* activity_log2 = ActivityLog::GetInstance(profile2.get());

  EXPECT_EQ(0,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile2->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
}

TEST_F(ActivityLogEnabledTest, PrefSwitch) {
  std::unique_ptr<TestingProfile> profile1(CreateTestingProfile());
  std::unique_ptr<TestingProfile> profile2(CreateTestingProfile());
  std::unique_ptr<TestingProfile> profile3(CreateTestingProfile());

  EXPECT_EQ(0,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile2->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile3->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));

  profile1->GetPrefs()->SetInteger(prefs::kWatchdogExtensionActive, 1);
  profile3->GetPrefs()->SetInteger(prefs::kWatchdogExtensionActive, 2);
  ActivityLog* activity_log1 = ActivityLog::GetInstance(profile1.get());
  ActivityLog* activity_log2 = ActivityLog::GetInstance(profile2.get());
  ActivityLog* activity_log3 = ActivityLog::GetInstance(profile3.get());

  EXPECT_EQ(1,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile2->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(2,
      profile3->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->is_active());
  EXPECT_FALSE(activity_log2->is_active());
  EXPECT_TRUE(activity_log3->is_active());
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());
  EXPECT_TRUE(activity_log3->IsDatabaseEnabled());
}

TEST_F(ActivityLogEnabledTest, WatchdogSwitch) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::unique_ptr<TestingProfile> profile1(CreateTestingProfile());
  std::unique_ptr<TestingProfile> profile2(CreateTestingProfile());
  // Extension service is destroyed by the profile.
  ExtensionService* extension_service1 =
    static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile1.get()))->CreateExtensionService(
            &command_line, base::FilePath(), false);
  static_cast<TestExtensionSystem*>(
      ExtensionSystem::Get(profile1.get()))->SetReady();

  ActivityLog* activity_log1 = ActivityLog::GetInstance(profile1.get());
  ActivityLog* activity_log2 = ActivityLog::GetInstance(profile2.get());

  // Allow Activity Log to install extension tracker.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile2->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "Watchdog Extension ")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2))
          .SetID(kExtensionID)
          .Build();
  extension_service1->AddExtension(extension.get());

  EXPECT_EQ(1,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile2->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());

  extension_service1->DisableExtension(kExtensionID,
                                       disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(0,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile2->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());

  extension_service1->EnableExtension(kExtensionID);

  EXPECT_EQ(1,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile2->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());

  // Wait for UninstallExtension to complete to avoid race condition with data
  // cleanup at TearDown.
  base::RunLoop loop;
  extension_service1->UninstallExtension(
      kExtensionID, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr,
      loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(0,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_EQ(0,
      profile2->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());

  scoped_refptr<const Extension> extension2 =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "Watchdog Extension ")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2))
          .SetID("fpofdchlamddhnajleknffcbmnjfahpg")
          .Build();
  extension_service1->AddExtension(extension.get());
  extension_service1->AddExtension(extension2.get());
  EXPECT_EQ(2,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  extension_service1->DisableExtension(kExtensionID,
                                       disable_reason::DISABLE_USER_ACTION);
  extension_service1->DisableExtension("fpofdchlamddhnajleknffcbmnjfahpg",
                                       disable_reason::DISABLE_USER_ACTION);
  EXPECT_EQ(0,
      profile1->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log1->IsDatabaseEnabled());
}

TEST_F(ActivityLogEnabledTest, AppAndCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExtensionActivityLogging);

  std::unique_ptr<TestingProfile> profile(CreateTestingProfile());
  // Extension service is destroyed by the profile.
  base::CommandLine no_program_command_line(base::CommandLine::NO_PROGRAM);
  ExtensionService* extension_service =
    static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile.get()))->CreateExtensionService(
            &no_program_command_line, base::FilePath(), false);
  static_cast<TestExtensionSystem*>(
      ExtensionSystem::Get(profile.get()))->SetReady();

  ActivityLog* activity_log = ActivityLog::GetInstance(profile.get());
  // Allow Activity Log to install extension tracker.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(activity_log->IsDatabaseEnabled());
  EXPECT_EQ(0,
      profile->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log->IsWatchdogAppActive());

  // Enable the extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "Watchdog Extension ")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2))
          .SetID(kExtensionID)
          .Build();
  extension_service->AddExtension(extension.get());

  EXPECT_TRUE(activity_log->IsDatabaseEnabled());
  EXPECT_EQ(1,
      profile->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log->IsWatchdogAppActive());

  // Wait for UninstallExtension to complete to avoid race condition with data
  // cleanup at TearDown.
  base::RunLoop loop;
  extension_service->UninstallExtension(
      kExtensionID, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr,
      loop.QuitClosure());
  loop.Run();

  EXPECT_TRUE(activity_log->IsDatabaseEnabled());
  EXPECT_EQ(0,
      profile->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log->IsWatchdogAppActive());
}

// Tests that if the cached count in the profile preferences is incorrect, the
// activity log will correct itself.
TEST_F(ActivityLogEnabledTest, IncorrectPrefsRecovery) {
  std::unique_ptr<TestingProfile> profile(CreateTestingProfile());
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ExtensionService* extension_service =
    static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile.get()))->CreateExtensionService(
            &command_line, base::FilePath(), false);

  // Set the preferences to indicate a cached count of 10.
  profile->GetPrefs()->SetInteger(prefs::kWatchdogExtensionActive, 10);
  ActivityLog* activity_log = ActivityLog::GetInstance(profile.get());

  static_cast<TestExtensionSystem*>(
      ExtensionSystem::Get(profile.get()))->SetReady();
  base::RunLoop().RunUntilIdle();

  // Even though the cached count was 10, the activity log should correctly
  // realize that there were no real consumers, and should be inactive and
  // correct the prefs.
  EXPECT_FALSE(activity_log->is_active());
  EXPECT_EQ(
      0, profile->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));

  // Testing adding an extension maintains pref and active correctness.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "Watchdog Extension ")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2))
          .SetID(kExtensionID)
          .Build();
  extension_service->AddExtension(extension.get());

  EXPECT_EQ(
      1, profile->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log->is_active());
}

}  // namespace extensions
