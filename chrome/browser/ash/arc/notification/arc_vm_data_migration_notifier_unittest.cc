// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_vm_data_migration_notifier.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_installer.h"
#include "chromeos/ash/experiences/arc/session/arc_session_runner.h"
#include "chromeos/ash/experiences/arc/session/arc_vm_data_migration_status.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

namespace arc {

namespace {

constexpr char kProfileName[] = "user@gmail.com";
constexpr GaiaId::Literal kGaiaId("1234567890");

constexpr char kNotificationId[] = "arc_vm_data_migration_notification";

class ArcVmDataMigrationNotifierTest : public ChromeAshTestBase {
 public:
  ArcVmDataMigrationNotifierTest() {
    base::CommandLine::ForCurrentProcess()->InitFromArgv(
        {"", "--arc-availability=officially-supported", "--enable-arcvm"});
  }
  ~ArcVmDataMigrationNotifierTest() override = default;
  ArcVmDataMigrationNotifierTest(const ArcVmDataMigrationNotifierTest&) =
      delete;
  ArcVmDataMigrationNotifierTest& operator=(
      const ArcVmDataMigrationNotifierTest&) = delete;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    ash::ConciergeClient::InitializeFake();
    ArcSessionManager::SetUiEnabledForTesting(false);
    arc_dlc_installer_ = std::make_unique<ArcDlcInstaller>();
    arc_session_manager_ = CreateTestArcSessionManager(
        std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)),
        arc_dlc_installer_.get());

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kProfileName, kGaiaId);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    ash::ScopedAccountIdAnnotator annotator(profile_manager_->profile_manager(),
                                            account_id);
    testing_profile_ = profile_manager_->CreateTestingProfile(kProfileName);
    DCHECK(user_manager::UserManager::Get()->IsPrimaryUser(
        ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
            testing_profile_)));

    arc_vm_data_migration_notifier_ =
        std::make_unique<ArcVmDataMigrationNotifier>(testing_profile_);

    arc_session_manager_->SetProfile(testing_profile_);
    arc_session_manager_->Initialize();
  }

  void TearDown() override {
    // Destroy profile dependents before the profile.
    arc_vm_data_migration_notifier_.reset();

    arc_session_manager_.reset();
    // Clear the raw_ptr before resetting the profile manager.
    testing_profile_ = nullptr;
    profile_manager_.reset();
    fake_user_manager_.Reset();
    arc_dlc_installer_.reset();

    ChromeAshTestBase::TearDown();
  }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  TestingProfile* profile() { return testing_profile_; }

 private:
  std::unique_ptr<ArcDlcInstaller> arc_dlc_installer_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcVmDataMigrationNotifier> arc_vm_data_migration_notifier_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> testing_profile_ =
      nullptr;  // Owned by |profile_manager_|.
};

// Tests that no notification is shown when the migration is disabled.
TEST_F(ArcVmDataMigrationNotifierTest, MigrationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnableArcVmDataMigration);

  arc_session_manager()->StartArcForTesting();
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          kNotificationId));
  // TODO(b/258278176): Use GetArcVmDataMigrationStatus() and stop using
  // Yoda-style comparisons. The same goes for other test cases.
  EXPECT_EQ(
      ArcVmDataMigrationStatus::kUnnotified,
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
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          kNotificationId));
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
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      kNotificationId));
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
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          kNotificationId));
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
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          kNotificationId));
}

}  // namespace

}  // namespace arc
