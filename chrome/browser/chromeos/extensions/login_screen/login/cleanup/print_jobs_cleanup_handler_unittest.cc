// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/print_jobs_cleanup_handler.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_impl.h"
#include "chrome/browser/ash/printing/history/test_print_job_database.h"
#include "chrome/browser/ash/printing/print_management/printing_manager.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using testing::_;
using testing::WithArg;

namespace {

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test-user@example.com",
                                            GaiaId::Literal("1234567890"));

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

  ~MockPrintingManager() override = default;

  MOCK_METHOD(void,
              DeleteAllPrintJobs,
              (DeleteAllPrintJobsCallback),
              (override));
};

}  // namespace

class PrintJobsCleanupHandlerUnittest : public testing::Test {
 protected:
  void SetUp() override {
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kTestAccountId));
    user_session_test_environment_->LogIn(kTestAccountId);
    testing_profile_ = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            kTestAccountId)));
    ASSERT_TRUE(testing_profile_);

    // Set up `MockPrintingManager`.
    print_job_manager_ =
        std::make_unique<ash::TestCupsPrintJobManager>(testing_profile_);
    auto print_job_database = std::make_unique<ash::TestPrintJobDatabase>();
    print_job_history_service_ =
        std::make_unique<ash::PrintJobHistoryServiceImpl>(
            std::move(print_job_database), print_job_manager_.get(),
            &test_prefs_);
    test_prefs_.registry()->RegisterBooleanPref(
        ash::prefs::kDeletePrintJobHistoryAllowed, true);
    test_prefs_.registry()->RegisterIntegerPref(
        ash::prefs::kPrintJobHistoryExpirationPeriod, 1);
    EXPECT_TRUE(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);

    ash::printing::print_management::PrintingManagerFactory::GetInstance()
        ->SetTestingFactory(
            testing_profile_,
            base::BindRepeating(
                &PrintJobsCleanupHandlerUnittest::MockPrintingManagerFactory,
                base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> MockPrintingManagerFactory(
      content::BrowserContext* context) {
    return std::make_unique<MockPrintingManager>(
        print_job_history_service_.get(), history_service_.get(),
        print_job_manager_.get(), &test_prefs_);
  }

  void TearDown() override {
    testing_profile_ = nullptr;
    user_session_test_environment_.reset();
    testing::Test::TearDown();
  }

  void SetUpDeleteAllPrintJobsMock(bool success) {
    auto* mock = static_cast<MockPrintingManager*>(
        ash::printing::print_management::PrintingManagerFactory::GetForProfile(
            testing_profile_));
    EXPECT_CALL(*mock, DeleteAllPrintJobs(_))
        .WillOnce(WithArg<0>(
            [success](ash::printing::print_management::PrintingManager::
                          DeleteAllPrintJobsCallback callback) {
              std::move(callback).Run(success);
            }));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple test_prefs_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<TestingProfile> testing_profile_;
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
