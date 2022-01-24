// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_added_removed/user_added_removed_reporter.h"

#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"

namespace reporting {

class TestHelper : public UserEventReporterHelper {
 public:
  TestHelper(std::unique_ptr<::reporting::ReportQueue,
                             base::OnTaskRunnerDeleter> report_queue,
             base::WeakPtr<testing::StrictMock<::reporting::MockReportQueue>>
                 mock_queue,
             bool should_report_event,
             bool should_report_user,
             bool is_user_new)
      : UserEventReporterHelper(std::move(report_queue)),
        mock_queue_(mock_queue),
        should_report_event_(should_report_event),
        should_report_user_(should_report_user),
        is_user_new_(is_user_new) {}

  bool ShouldReportUser(const std::string& user_email) const override {
    return should_report_user_;
  }

  bool ReportingEnabled(const std::string& policy_path) const override {
    return should_report_event_;
  }

  bool IsCurrentUserNew() const override { return is_user_new_; }

  void ReportEvent(const google::protobuf::MessageLite* record,
                   Priority priority) override {
    event_reported_ = true;
    mock_queue_->Enqueue(record, priority,
                         base::BindOnce([](::reporting::Status status) {}));
  }

  base::WeakPtr<testing::StrictMock<::reporting::MockReportQueue>> mock_queue_;

  bool should_report_event_;

  bool should_report_user_;

  bool is_user_new_;

  bool event_reported_;
};

class UserAddedRemovedReporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager_ = user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    mock_queue_ = new testing::StrictMock<::reporting::MockReportQueue>();
    weak_mock_queue_factory_ = std::make_unique<base::WeakPtrFactory<
        testing::StrictMock<::reporting::MockReportQueue>>>(mock_queue_);
  }

  void TearDown() override {
    chromeos::PowerManagerClient::Shutdown();
    delete mock_queue_;
  }

  std::unique_ptr<TestingProfile> CreateRegularProfile(
      base::StringPiece user_email) {
    const AccountId account_id =
        AccountId::FromUserEmail(std::string(user_email));
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    auto profile = profile_builder.Build();
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id, true);
    return profile;
  }

  std::unique_ptr<TestingProfile> CreateGuestProfile() {
    TestingProfile::Builder profile_builder;
    auto profile = profile_builder.Build();
    user_manager::User* user = user_manager_->AddGuestUser();
    user_manager_->LoginUser(user->GetAccountId(), true);
    return profile;
  }

  std::unique_ptr<TestingProfile> CreateKioskProfile(
      base::StringPiece user_email) {
    const AccountId account_id =
        AccountId::FromUserEmail(std::string(user_email));
    TestingProfile::Builder profile_builder;
    auto profile = profile_builder.Build();
    user_manager_->AddKioskAppUser(account_id);
    user_manager_->LoginUser(account_id, true);
    return profile;
  }

  testing::StrictMock<::reporting::MockReportQueue>* mock_queue_;

  std::unique_ptr<
      base::WeakPtrFactory<testing::StrictMock<::reporting::MockReportQueue>>>
      weak_mock_queue_factory_;

 private:
  ash::FakeChromeUserManager* user_manager_;

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(UserAddedRemovedReporterTest, TestAffiliatedUserAdded) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });
  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ true, /* user_new */ true));

  auto profile = CreateRegularProfile(user_email);
  reporter.OnLogin(profile.get());

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.has_user_added_event());
  EXPECT_TRUE(record.has_affiliated_user());
  ASSERT_EQ(record.affiliated_user().user_email(), user_email);
}

TEST_F(UserAddedRemovedReporterTest, TestUnaffiliatedUserAdded) {
  static constexpr char user_email[] = "unaffiliated@managed.org";
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ false, /* user_new */ true));

  auto profile = CreateRegularProfile(user_email);
  reporter.OnLogin(profile.get());

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.has_user_added_event());
  ASSERT_FALSE(record.has_affiliated_user());
}

TEST_F(UserAddedRemovedReporterTest, TestReportingDisabled) {
  static constexpr char user_email[] = "unaffiliated@managed.org";
  const AccountId account_id =
      AccountId::FromUserEmail(std::string(user_email));

  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ false,
      /* report_user */ true, /* user_new */ true));

  auto profile = CreateRegularProfile(user_email);
  reporter.OnLogin(profile.get());
  reporter.OnUserToBeRemoved(account_id);
  reporter.OnUserRemoved(account_id,
                         user_manager::UserRemovalReason::GAIA_REMOVED);

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);
}

TEST_F(UserAddedRemovedReporterTest, TestExistingUserLogin) {
  static constexpr char user_email[] = "unaffiliated@managed.org";
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ true, /* user_new */ false));

  auto profile = CreateRegularProfile(user_email);
  reporter.OnLogin(profile.get());

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);
}

TEST_F(UserAddedRemovedReporterTest, TestGuestSessionLogsIn) {
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ true, /* user_new */ true));

  auto profile = CreateGuestProfile();
  reporter.OnLogin(profile.get());

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);
}

TEST_F(UserAddedRemovedReporterTest, TestKioskUserLogsIn) {
  static constexpr char user_email[] = "kiosk@managed.org";
  const AccountId account_id =
      AccountId::FromUserEmail(std::string(user_email));
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ true, /* user_new */ true));

  auto profile = CreateKioskProfile(user_email);
  reporter.OnLogin(profile.get());

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);
}

TEST_F(UserAddedRemovedReporterTest, TestAffiliatedUserRemoval) {
  static constexpr char user_email[] = "affiliated@managed.org";
  const AccountId account_id =
      AccountId::FromUserEmail(std::string(user_email));
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ true, /* user_new */ true));

  auto profile = CreateRegularProfile(user_email);
  reporter.OnUserToBeRemoved(account_id);
  reporter.OnUserRemoved(account_id,
                         user_manager::UserRemovalReason::GAIA_REMOVED);

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.has_user_removed_event());
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_THAT(record.affiliated_user().user_email(),
              ::testing::StrEq(user_email));
  EXPECT_THAT(record.user_removed_event().reason(),
              ::testing::Eq(UserRemovalReason::GAIA_REMOVED));
}

TEST_F(UserAddedRemovedReporterTest, TestUnaffiliatedUserRemoval) {
  static constexpr char user_email[] = "unaffiliated@managed.org";
  const AccountId account_id =
      AccountId::FromUserEmail(std::string(user_email));
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ false, /* user_new */ true));

  auto profile = CreateRegularProfile(user_email);
  reporter.OnUserToBeRemoved(account_id);
  reporter.OnUserRemoved(account_id,
                         user_manager::UserRemovalReason::GAIA_REMOVED);

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.has_user_removed_event());
  EXPECT_FALSE(record.has_affiliated_user());
  EXPECT_THAT(record.user_removed_event().reason(),
              ::testing::Eq(UserRemovalReason::GAIA_REMOVED));
}

TEST_F(UserAddedRemovedReporterTest, TestKioskUserRemoved) {
  static constexpr char user_email[] = "kiosk@managed.org";
  const AccountId account_id =
      AccountId::FromUserEmail(std::string(user_email));
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ true, /* user_new */ true));
  auto profile = CreateKioskProfile(user_email);
  reporter.OnUserToBeRemoved(account_id);
  reporter.OnUserRemoved(account_id,
                         user_manager::UserRemovalReason::GAIA_REMOVED);

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);
}

TEST_F(UserAddedRemovedReporterTest, TestRemoteRemoval) {
  static constexpr char user_email[] = "user@managed.org";
  ash::ChromeUserManager::Get()->CacheRemovedUser(
      user_email, user_manager::UserRemovalReason::REMOTE_ADMIN_INITIATED);

  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::UserAddedRemovedRecord record_a;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  UserAddedRemovedReporter reporter(std::make_unique<TestHelper>(
      std::move(dummy_queue), mock_queue, /* report_event */ true,
      /* report_user */ false, /* user_new */ true));

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.has_user_removed_event());
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_EQ(record.affiliated_user().user_email(), user_email);
  EXPECT_THAT(record.user_removed_event().reason(),
              ::testing::Eq(UserRemovalReason::REMOTE_ADMIN_INITIATED));
}
}  // namespace reporting
