// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_added_removed/user_added_removed_reporter.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/add_remove_user_event.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

class TestHelper : public UserEventReporterHelper {
 public:
  TestHelper(std::unique_ptr<::reporting::ReportQueue,
                             base::OnTaskRunnerDeleter> report_queue,
             base::WeakPtr<::reporting::MockReportQueueStrict> mock_queue,
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

  void ReportEvent(
      std::unique_ptr<const google::protobuf::MessageLite> record,
      Priority priority,
      ReportQueue::EnqueueCallback enqueue_cb = base::DoNothing()) override {
    event_reported_ = true;
    mock_queue_->Enqueue(std::move(record), priority, std::move(enqueue_cb));
  }

  base::WeakPtr<::reporting::MockReportQueueStrict> mock_queue_;

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

    mock_queue_ = new ::reporting::MockReportQueueStrict();
    weak_mock_queue_factory_ = std::make_unique<
        base::WeakPtrFactory<::reporting::MockReportQueueStrict>>(mock_queue_);
  }

  void TearDown() override {
    chromeos::PowerManagerClient::Shutdown();
    delete mock_queue_;
  }

  std::unique_ptr<TestingProfile> LoginRegularProfile(
      std::string_view user_email,
      policy::ManagedSessionService* managed_session_service) {
    const AccountId account_id =
        AccountId::FromUserEmail(std::string(user_email));
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    auto profile = profile_builder.Build();
    auto* const user = user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id, true);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile.get());
    managed_session_service->OnUserProfileLoaded(account_id);
    return profile;
  }

  std::unique_ptr<TestingProfile> LoginGuestProfile(
      policy::ManagedSessionService* managed_session_service) {
    TestingProfile::Builder profile_builder;
    auto profile = profile_builder.Build();
    user_manager::User* user = user_manager_->AddGuestUser();
    user_manager_->LoginUser(user->GetAccountId(), true);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile.get());
    managed_session_service->OnUserProfileLoaded(user->GetAccountId());
    return profile;
  }

  std::unique_ptr<TestingProfile> LoginKioskProfile(
      std::string_view user_email,
      policy::ManagedSessionService* managed_session_service) {
    const AccountId account_id =
        AccountId::FromUserEmail(std::string(user_email));
    TestingProfile::Builder profile_builder;
    auto profile = profile_builder.Build();
    auto* const user = user_manager_->AddKioskAppUser(account_id);
    user_manager_->LoginUser(account_id, true);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile.get());
    managed_session_service->OnUserProfileLoaded(account_id);
    return profile;
  }

  raw_ptr<::reporting::MockReportQueueStrict, DanglingUntriaged> mock_queue_;

  std::unique_ptr<base::WeakPtrFactory<::reporting::MockReportQueueStrict>>
      weak_mock_queue_factory_;

 private:
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(UserAddedRemovedReporterTest, TestAffiliatedUserAdded) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/true, /*user_new=*/true);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  LoginRegularProfile(user_email, managed_session_service.get());

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
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/false, /*user_new=*/true);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  LoginRegularProfile(user_email, managed_session_service.get());

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
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();
  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/false,
                                   /*report_user=*/true, /*user_new=*/true);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  auto profile = LoginRegularProfile(user_email, managed_session_service.get());
  managed_session_service->OnUserToBeRemoved(account_id);
  managed_session_service->OnUserRemoved(
      account_id, user_manager::UserRemovalReason::GAIA_REMOVED);
}

TEST_F(UserAddedRemovedReporterTest, TestExistingUserLogin) {
  static constexpr char user_email[] = "unaffiliated@managed.org";
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();
  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/true, /*user_new=*/false);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  LoginRegularProfile(user_email, managed_session_service.get());
}

TEST_F(UserAddedRemovedReporterTest, TestGuestSessionLogsIn) {
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();
  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/true, /*user_new=*/true);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  LoginGuestProfile(managed_session_service.get());
}

TEST_F(UserAddedRemovedReporterTest, TestKioskUserLogsIn) {
  static constexpr char user_email[] = "kiosk@managed.org";
  const AccountId account_id =
      AccountId::FromUserEmail(std::string(user_email));
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();
  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/true, /*user_new=*/true);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  LoginKioskProfile(user_email, managed_session_service.get());
}

TEST_F(UserAddedRemovedReporterTest, TestAffiliatedUserRemoval) {
  static constexpr char user_email[] = "affiliated@managed.org";
  const AccountId account_id =
      AccountId::FromUserEmail(std::string(user_email));
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/true, /*user_new=*/false);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  auto profile = LoginRegularProfile(user_email, managed_session_service.get());
  managed_session_service->OnUserToBeRemoved(account_id);
  managed_session_service->OnUserRemoved(
      account_id, user_manager::UserRemovalReason::GAIA_REMOVED);

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
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/false, /*user_new=*/false);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  auto profile = LoginRegularProfile(user_email, managed_session_service.get());
  managed_session_service->OnUserToBeRemoved(account_id);
  managed_session_service->OnUserRemoved(
      account_id, user_manager::UserRemovalReason::GAIA_REMOVED);

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
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();
  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/true, /*user_new=*/true);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());

  auto profile = LoginKioskProfile(user_email, managed_session_service.get());
  managed_session_service->OnUserToBeRemoved(account_id);
  managed_session_service->OnUserRemoved(
      account_id, user_manager::UserRemovalReason::GAIA_REMOVED);
}

TEST_F(UserAddedRemovedReporterTest, TestRemoteRemoval) {
  static constexpr char user_email[] = "user@managed.org";

  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue,
                                   /*report_event=*/true,
                                   /*report_user=*/false, /*user_new=*/true);
  auto managed_session_service =
      std::make_unique<policy::ManagedSessionService>();

  auto reporter = UserAddedRemovedReporter::CreateForTesting(
      std::move(test_helper), /*users_to_be_removed=*/{},
      managed_session_service.get());
  reporter->ProcessRemovedUser(
      user_email, user_manager::UserRemovalReason::REMOTE_ADMIN_INITIATED);

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp_sec());
  EXPECT_TRUE(record.has_user_removed_event());
  EXPECT_TRUE(record.has_affiliated_user());
  EXPECT_EQ(record.affiliated_user().user_email(), user_email);
  EXPECT_THAT(record.user_removed_event().reason(),
              ::testing::Eq(UserRemovalReason::REMOTE_ADMIN_INITIATED));
}
}  // namespace reporting
