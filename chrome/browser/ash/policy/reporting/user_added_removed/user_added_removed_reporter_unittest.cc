// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_added_removed/user_added_removed_reporter.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/add_remove_user_event.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

class TestHelper : public UserEventReporterHelper {
 public:
  // `mock_queue` must be non-null and must outlive this test helper.
  TestHelper(std::unique_ptr<::reporting::ReportQueue,
                             base::OnTaskRunnerDeleter> report_queue,
             ::reporting::ReportQueue* mock_queue,
             bool should_report_event,
             bool should_report_user,
             bool is_user_new)
      : UserEventReporterHelper(std::move(report_queue)),
        mock_queue_(CHECK_DEREF(mock_queue)),
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

  const raw_ref<::reporting::ReportQueue> mock_queue_;

  bool should_report_event_;

  bool should_report_user_;

  bool is_user_new_;

  bool event_reported_;
};

class UserAddedRemovedReporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    user_session_manager_ = std::make_unique<ash::UserSessionManager>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        TestingBrowserProcess::GetGlobal()
            ->GetFeatures()
            ->application_locale_storage(),
        TestingBrowserProcess::GetGlobal()->shared_url_loader_factory(),
        TestingBrowserProcess::GetGlobal()
            ->platform_part()
            ->browser_policy_connector_ash());
    mock_queue_ = std::make_unique<::reporting::MockReportQueueStrict>();
  }

  void TearDown() override {
    mock_queue_.reset();
    user_session_manager_->Shutdown();
    user_session_manager_.reset();
    user_manager_.Reset();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
    chromeos::PowerManagerClient::Shutdown();
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
    auto* const user = user_manager_->AddKioskChromeAppUser(account_id);
    user_manager_->LoginUser(account_id, true);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile.get());
    managed_session_service->OnUserProfileLoaded(account_id);
    return profile;
  }

  std::unique_ptr<::reporting::MockReportQueueStrict> mock_queue_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;

  // NOTE: InstallAttributes is required to construct BrowserPolicyConnectorAsh.
  // CrosSettings is needed because otherwise TestingProfile automatically
  // creates ScopedCrosSettingsTestHelper, which conflicts with
  // ScopedStubInstallAttributes.
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;
  std::unique_ptr<ash::UserSessionManager> user_session_manager_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(UserAddedRemovedReporterTest, TestAffiliatedUserAdded) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr, base::OnTaskRunnerDeleter(
                       base::SequencedTaskRunner::GetCurrentDefault()));

  UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue_.get(), AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue_.get(), AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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
  EXPECT_CALL(*mock_queue_.get(), AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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
  EXPECT_CALL(*mock_queue_.get(), AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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
  EXPECT_CALL(*mock_queue_.get(), AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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
  EXPECT_CALL(*mock_queue_.get(), AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue_.get(), AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue_.get(), AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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
  EXPECT_CALL(*mock_queue_.get(), AddRecord).Times(0);

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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

  ::reporting::UserAddedRemovedRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue_.get(), AddRecord)
      .WillOnce(
          [&record, &priority](std::string_view record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_helper =
      std::make_unique<TestHelper>(std::move(dummy_queue), mock_queue_.get(),
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
