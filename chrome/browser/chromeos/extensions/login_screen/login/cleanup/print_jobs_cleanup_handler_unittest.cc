// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/print_jobs_cleanup_handler.h"

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_impl.h"
#include "chrome/browser/ash/printing/history/test_print_job_database.h"
#include "chrome/browser/ash/printing/print_management/printing_manager.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace {

class MockPrintingManager
    : public ash::printing::print_management::PrintingManager {
 public:
  MockPrintingManager(ash::PrintJobHistoryService* print_job_history_service,
                      history::HistoryService* history_service,
                      ash::CupsPrintJobManager* cups_print_job_manager,
                      PrefService* pref_service)
      : ash::printing::print_management::PrintingManager(
            print_job_history_service,
            history_service,
            cups_print_job_manager,
            pref_service) {}

  MockPrintingManager(const MockPrintingManager&) = delete;
  MockPrintingManager& operator=(const MockPrintingManager&) = delete;

  ~MockPrintingManager() override {}

  MOCK_METHOD(void,
              DeleteAllPrintJobs,
              (DeleteAllPrintJobsCallback),
              (override));
};

}  // namespace

class PrintJobsCleanupHandlerUnittest : public testing::Test {
 protected:
  PrintJobsCleanupHandlerUnittest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    // Add a user.
    testing_profile_ = testing_profile_manager_.CreateTestingProfile(
        account_id.GetUserEmail());

    // Log in to set active profile.
    std::unique_ptr<FakeChromeUserManager> fake_user_manager =
        std::make_unique<FakeChromeUserManager>();
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);

    // Set up `MockPrintingManager`.
    print_job_manager_ =
        std::make_unique<ash::TestCupsPrintJobManager>(testing_profile_);
    auto print_job_database = std::make_unique<ash::TestPrintJobDatabase>();
    print_job_history_service_ =
        std::make_unique<ash::PrintJobHistoryServiceImpl>(
            std::move(print_job_database), print_job_manager_.get(),
            &test_prefs_);
    test_prefs_.registry()->RegisterBooleanPref(
        prefs::kDeletePrintJobHistoryAllowed, true);
    test_prefs_.registry()->RegisterIntegerPref(
        prefs::kPrintJobHistoryExpirationPeriod, 1);
    EXPECT_TRUE(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);

    ash::printing::print_management::PrintingManagerFactory::GetInstance()
        ->SetTestingFactory(
            testing_profile_,
            base::BindRepeating(
                &PrintJobsCleanupHandlerUnittest::MockPrintingManagerFactory,
                base::Unretained(this)));

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  std::unique_ptr<KeyedService> MockPrintingManagerFactory(
      content::BrowserContext* context) {
    return std::make_unique<MockPrintingManager>(
        print_job_history_service_.get(), history_service_.get(),
        print_job_manager_.get(), &test_prefs_);
  }

  void TearDown() override {
    scoped_user_manager_.reset();
    testing_profile_manager_.DeleteTestingProfile(account_id.GetUserEmail());
    testing::Test::TearDown();
  }

  void SetUpDeleteAllPrintJobsMock(bool success) {
    auto* mock = static_cast<MockPrintingManager*>(
        ash::printing::print_management::PrintingManagerFactory::GetForProfile(
            testing_profile_));
    EXPECT_CALL(*mock, DeleteAllPrintJobs(_))
        .WillOnce(WithArg<0>(
            Invoke([success](ash::printing::print_management::PrintingManager::
                                 DeleteAllPrintJobsCallback callback) {
              std::move(callback).Run(success);
            })));
  }

  content::BrowserTaskEnvironment task_environment_;
  AccountId account_id = AccountId::FromUserEmail("test-user@example.com");
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  TestingPrefServiceSimple test_prefs_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> testing_profile_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<ash::TestCupsPrintJobManager> print_job_manager_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<ash::PrintJobHistoryService> print_job_history_service_;
};

TEST_F(PrintJobsCleanupHandlerUnittest, Cleanup) {
  SetUpDeleteAllPrintJobsMock(/* success =*/true);

  PrintJobsCleanupHandler handler;

  base::RunLoop run_loop;

  CleanupHandler::CleanupHandlerCallback callback = base::BindLambdaForTesting(
      [&](const std::optional<std::string>& error_message) {
        ASSERT_FALSE(error_message);
        run_loop.QuitClosure().Run();
      });

  handler.Cleanup(std::move(callback));
  run_loop.Run();
}

TEST_F(PrintJobsCleanupHandlerUnittest, CleanupWithError) {
  SetUpDeleteAllPrintJobsMock(/* success =*/false);

  PrintJobsCleanupHandler handler;

  base::RunLoop run_loop;

  CleanupHandler::CleanupHandlerCallback callback = base::BindLambdaForTesting(
      [&](const std::optional<std::string>& error_message) {
        ASSERT_EQ(error_message, "Failed to delete all print jobs");
        run_loop.QuitClosure().Run();
      });

  handler.Cleanup(std::move(callback));
  run_loop.Run();
}

}  // namespace chromeos
