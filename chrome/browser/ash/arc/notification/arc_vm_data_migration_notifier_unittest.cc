// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_vm_data_migration_notifier.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kProfileName[] = "user@gmail.com";
constexpr char kGaiaId[] = "1234567890";

constexpr char kNotificationId[] = "arc_vm_data_migration_notification";

class ArcVmDataMigrationNotifierTest : public ash::AshTestBase {
 public:
  ArcVmDataMigrationNotifierTest()
      : ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {
    base::CommandLine::ForCurrentProcess()->InitFromArgv(
        {"", "--arc-availability=officially-supported", "--enable-arcvm"});
  }

  ~ArcVmDataMigrationNotifierTest() override = default;

  ArcVmDataMigrationNotifierTest(const ArcVmDataMigrationNotifierTest&) =
      delete;
  ArcVmDataMigrationNotifierTest& operator=(
      const ArcVmDataMigrationNotifierTest&) = delete;

  void SetUp() override {
    ash::AshTestBase::SetUp();
    ash::ConciergeClient::InitializeFake();
    ArcSessionManager::SetUiEnabledForTesting(false);
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile(kProfileName);
    const AccountId account_id = AccountId::FromUserEmailGaiaId(
        testing_profile_->GetProfileUserName(), kGaiaId);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    DCHECK(ash::ProfileHelper::IsPrimaryProfile(testing_profile_));

    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(testing_profile_);
    arc_vm_data_migration_notifier_ =
        std::make_unique<ArcVmDataMigrationNotifier>(testing_profile_);

    arc_session_manager_->SetProfile(testing_profile_);
    arc_session_manager_->Initialize();
  }

  void TearDown() override {
    arc_session_manager_->Shutdown();
    notification_tester_.reset();
    profile_manager_->DeleteTestingProfile(kProfileName);
    testing_profile_ = nullptr;
    profile_manager_.reset();
    fake_user_manager_.Reset();
    arc_vm_data_migration_notifier_.reset();
    arc_session_manager_.reset();
    ash::ConciergeClient::Shutdown();
    ash::AshTestBase::TearDown();
  }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  NotificationDisplayServiceTester* notification_tester() {
    return notification_tester_.get();
  }

  TestingProfile* profile() { return testing_profile_; }

 private:
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcVmDataMigrationNotifier> arc_vm_data_migration_notifier_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> testing_profile_ =
      nullptr;  // Owned by |profile_manager_|.
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
};

// Tests that no notification is shown when the migration is disabled.
TEST_F(ArcVmDataMigrationNotifierTest, MigrationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableArcVmDataMigration);

  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(notification_tester()->GetNotification(kNotificationId));
  // TODO(b/258278176): Use GetArcVmDataMigrationStatus() and stop using
  // Yoda-style comparisons. The same goes for other test cases.
  EXPECT_EQ(
      ArcVmDataMigrationStatus::kUnnotified,
      static_cast<ArcVmDataMigrationStatus>(
          profile()->GetPrefs()->GetInteger(prefs::kArcVmDataMigrationStatus)));
}

// Tests that no notification is shown for managed users even when the migration
// is enabled via the feature and when the policy is not set.
TEST_F(ArcVmDataMigrationNotifierTest, AccountManagedDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(GetArcVmDataMigrationStatus(profile()->GetPrefs()),
            ArcVmDataMigrationStatus::kUnnotified);
}

// Tests that no notification is shown for managed users when the migration is
// enabled via the featuer and when the policy is set to not prompt.
TEST_F(ArcVmDataMigrationNotifierTest, AccountManagedDoNotPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  profile()->GetPrefs()->SetInteger(
      prefs::kArcVmDataMigrationStrategy,
      static_cast<int>(ArcVmDataMigrationStrategy::kDoNotPrompt));

  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(GetArcVmDataMigrationStatus(profile()->GetPrefs()),
            ArcVmDataMigrationStatus::kUnnotified);
}

// Tests that no notification is shown for managed users when the migration is
// enabled via the featuer and when the policy is set to not prompt and already
// started.
TEST_F(ArcVmDataMigrationNotifierTest, AccountManagedPromptAndStarted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  auto* prefs = profile()->GetPrefs();
  SetArcVmDataMigrationStatus(prefs, ArcVmDataMigrationStatus::kStarted);
  profile()->GetPrefs()->SetInteger(
      prefs::kArcVmDataMigrationStrategy,
      static_cast<int>(ArcVmDataMigrationStrategy::kPrompt));

  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(GetArcVmDataMigrationStatus(profile()->GetPrefs()),
            ArcVmDataMigrationStatus::kStarted);
}

// Tests that no notification is shown for managed users when the migration is
// enabled via the featuer and when the policy is set to not prompti and
// already finished.
TEST_F(ArcVmDataMigrationNotifierTest, AccountManagedPromptAndFinished) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  auto* prefs = profile()->GetPrefs();
  SetArcVmDataMigrationStatus(prefs, ArcVmDataMigrationStatus::kFinished);
  profile()->GetPrefs()->SetInteger(
      prefs::kArcVmDataMigrationStrategy,
      static_cast<int>(ArcVmDataMigrationStrategy::kPrompt));

  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(GetArcVmDataMigrationStatus(profile()->GetPrefs()),
            ArcVmDataMigrationStatus::kFinished);
}

// Tests that notification is shown for managed users when the migration is
// enabled via the feature and when the policy is set to prompt.
TEST_F(ArcVmDataMigrationNotifierTest, AccountManagedPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  profile()->GetPrefs()->SetInteger(
      prefs::kArcVmDataMigrationStrategy,
      static_cast<int>(ArcVmDataMigrationStrategy::kPrompt));

  arc_session_manager()->StartArcForTesting();
  EXPECT_TRUE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(GetArcVmDataMigrationStatus(profile()->GetPrefs()),
            ArcVmDataMigrationStatus::kNotified);
}

// Tests that a notification is shown when the migration is enabled but not
// started nor finished yet.
TEST_F(ArcVmDataMigrationNotifierTest, MigrationEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);

  arc_session_manager()->StartArcForTesting();
  EXPECT_TRUE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(
      ArcVmDataMigrationStatus::kNotified,
      static_cast<ArcVmDataMigrationStatus>(
          profile()->GetPrefs()->GetInteger(prefs::kArcVmDataMigrationStatus)));
}

// Tests that a notification is shown when the user has been notified of the
// migration but not started the migration yet.
TEST_F(ArcVmDataMigrationNotifierTest, MigrationNotified) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  profile()->GetPrefs()->SetInteger(
      prefs::kArcVmDataMigrationStatus,
      static_cast<int>(ArcVmDataMigrationStatus::kNotified));

  arc_session_manager()->StartArcForTesting();
  EXPECT_TRUE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(
      ArcVmDataMigrationStatus::kNotified,
      static_cast<ArcVmDataMigrationStatus>(
          profile()->GetPrefs()->GetInteger(prefs::kArcVmDataMigrationStatus)));
}

// Tests that a notification is shown when the user has confirmed the migration
// but ARC session is started without entering the migration screen.
TEST_F(ArcVmDataMigrationNotifierTest, MigrationConfirmed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  profile()->GetPrefs()->SetInteger(
      prefs::kArcVmDataMigrationStatus,
      static_cast<int>(ArcVmDataMigrationStatus::kConfirmed));

  arc_session_manager()->StartArcForTesting();
  EXPECT_TRUE(notification_tester()->GetNotification(kNotificationId));
  // The migration status is set back to kNotified.
  EXPECT_EQ(
      ArcVmDataMigrationStatus::kNotified,
      static_cast<ArcVmDataMigrationStatus>(
          profile()->GetPrefs()->GetInteger(prefs::kArcVmDataMigrationStatus)));
}

// Tests that no notification is shown once the migration has started but the
// maximum number of auto-resumes has not been reached yet.
TEST_F(ArcVmDataMigrationNotifierTest, MigrationStarted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  auto* prefs = profile()->GetPrefs();
  SetArcVmDataMigrationStatus(prefs, ArcVmDataMigrationStatus::kStarted);
  prefs->SetInteger(prefs::kArcVmDataMigrationAutoResumeCount, 0);

  arc_session_manager()->RequestEnable();
  EXPECT_FALSE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(GetArcVmDataMigrationStatus(prefs),
            ArcVmDataMigrationStatus::kStarted);
}

// Tests that a notification is shown when the migration has started and the
// maximum number of auto-resumes has been reached.
TEST_F(ArcVmDataMigrationNotifierTest, MaxNumberOfAutoResumesReached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  auto* prefs = profile()->GetPrefs();
  SetArcVmDataMigrationStatus(prefs, ArcVmDataMigrationStatus::kStarted);
  prefs->SetInteger(prefs::kArcVmDataMigrationAutoResumeCount,
                    kArcVmDataMigrationMaxAutoResumeCount + 1);

  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(GetArcVmDataMigrationStatus(prefs),
            ArcVmDataMigrationStatus::kStarted);
}

// Tests that no notification is shown once the migration has finished.
TEST_F(ArcVmDataMigrationNotifierTest, MigrationFinished) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableArcVmDataMigration);
  profile()->GetPrefs()->SetInteger(
      prefs::kArcVmDataMigrationStatus,
      static_cast<int>(ArcVmDataMigrationStatus::kFinished));

  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(notification_tester()->GetNotification(kNotificationId));
  EXPECT_EQ(
      ArcVmDataMigrationStatus::kFinished,
      static_cast<ArcVmDataMigrationStatus>(
          profile()->GetPrefs()->GetInteger(prefs::kArcVmDataMigrationStatus)));
}

// Tests that no notification is shown even when the migration is enabled if
// virtio-blk /data is forcibly enabled via kEnableVirtioBlkForData.
TEST_F(ArcVmDataMigrationNotifierTest, VirtioBlkDataForced) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kEnableArcVmDataMigration, kEnableVirtioBlkForData}, {});

  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(notification_tester()->GetNotification(kNotificationId));
}

}  // namespace

}  // namespace arc
