// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_critical_action_logger.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::SaveArg;

class MockCriticalActionService
    : public critical_actions::CriticalActionService {
 public:
  MockCriticalActionService(
      const base::FilePath& db_path,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
      : critical_actions::CriticalActionService(db_path, backend_task_runner) {}
  ~MockCriticalActionService() override = default;

  MOCK_METHOD(void,
              AddCriticalAction,
              (const critical_actions::CriticalActionEntry& entry),
              (override));
  MOCK_METHOD(void,
              AddCriticalActionWithNavigationId,
              (const critical_actions::CriticalActionEntry& entry,
               int64_t navigation_id),
              (override));
  MOCK_METHOD(void, OnNavigationDiscarded, (int64_t navigation_id), (override));
};

}  // namespace

class PasswordManagerCriticalActionLoggerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordManagerCriticalActionLoggerTest() {
    feature_list_.InitAndEnableFeature(
        critical_actions::features::kCriticalActionHistory);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_path =
        temp_dir_.GetPath().AppendASCII("TestCriticalActionsLogger.db");
    auto mock_service = std::make_unique<MockCriticalActionService>(
        db_path,
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
    mock_service_ = mock_service.get();

    critical_actions::CriticalActionFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindOnce(
            [](std::unique_ptr<MockCriticalActionService> service,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> { return std::move(service); },
            std::move(mock_service)));

    // ContentPasswordManagerDriver::GetForRenderFrameHost requires the factory
    // to be registered.
    stub_client_ =
        std::make_unique<password_manager::StubPasswordManagerClient>();
    password_manager::ContentPasswordManagerDriverFactory::CreateForWebContents(
        web_contents(), stub_client_.get());

    logger_ =
        std::make_unique<password_manager::PasswordManagerCriticalActionLogger>(
            web_contents(), profile());

    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        test_url_, web_contents());
    simulator->Start();
    nav_id_ = simulator->GetNavigationHandle()->GetNavigationId();
    simulator->Commit();
  }

  void TearDown() override {
    logger_.reset();
    mock_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
    stub_client_.reset();
  }

  MockCriticalActionService* mock_service() { return mock_service_; }

  password_manager::PasswordManagerCriticalActionLogger* logger() {
    return logger_.get();
  }

  const GURL& test_url() const { return test_url_; }
  int64_t nav_id() const { return nav_id_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  raw_ptr<MockCriticalActionService> mock_service_ = nullptr;
  std::unique_ptr<password_manager::StubPasswordManagerClient> stub_client_;
  std::unique_ptr<password_manager::PasswordManagerCriticalActionLogger>
      logger_;
  GURL test_url_ = GURL("https://example.com");
  int64_t nav_id_ = 0;
};

TEST_F(PasswordManagerCriticalActionLoggerTest,
       LogsCriticalActionForAutofillTrigger) {
  critical_actions::CriticalActionEntry recorded_entry;
  int64_t recorded_nav_id = 0;

  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId)
      .WillOnce(testing::DoAll(SaveArg<0>(&recorded_entry),
                               SaveArg<1>(&recorded_nav_id)));

  logger()->MaybeLogCriticalAction(
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          web_contents()->GetPrimaryMainFrame()),
      test_url(),
      password_manager::PasswordManagerClient::PasswordFillTrigger::
          kPasswordManagerAutofill);

  EXPECT_EQ(recorded_nav_id, nav_id());
  EXPECT_EQ(recorded_entry.action_type,
            critical_actions::ActionType::kFormFill);
  EXPECT_EQ(recorded_entry.action_source,
            critical_actions::ActionSource::kPasswordManager);
  EXPECT_EQ(recorded_entry.url, test_url());

  std::optional<base::Value> metadata =
      base::JSONReader::Read(recorded_entry.metadata, base::JSON_PARSE_RFC);
  ASSERT_TRUE(metadata.has_value());
  ASSERT_TRUE(metadata->is_dict());
  std::string* type_str = metadata->GetDict().FindString("type");
  ASSERT_TRUE(type_str);
  EXPECT_EQ(*type_str, "password_manager_autofill");
}

TEST_F(PasswordManagerCriticalActionLoggerTest,
       LogsCriticalActionForAgentTaskTrigger) {
  critical_actions::CriticalActionEntry recorded_entry;
  int64_t recorded_nav_id = 0;

  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId)
      .WillOnce(testing::DoAll(SaveArg<0>(&recorded_entry),
                               SaveArg<1>(&recorded_nav_id)));

  logger()->MaybeLogCriticalAction(
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          web_contents()->GetPrimaryMainFrame()),
      test_url(),
      password_manager::PasswordManagerClient::PasswordFillTrigger::kAgentTask);

  EXPECT_EQ(recorded_nav_id, nav_id());
  EXPECT_EQ(recorded_entry.action_type,
            critical_actions::ActionType::kFormFill);
  EXPECT_EQ(recorded_entry.action_source,
            critical_actions::ActionSource::kPasswordManager);
  EXPECT_EQ(recorded_entry.url, test_url());

  std::optional<base::Value> metadata =
      base::JSONReader::Read(recorded_entry.metadata, base::JSON_PARSE_RFC);
  ASSERT_TRUE(metadata.has_value());
  ASSERT_TRUE(metadata->is_dict());
  std::string* type_str = metadata->GetDict().FindString("type");
  ASSERT_TRUE(type_str);
  EXPECT_EQ(*type_str, "agent_task");
}

TEST_F(PasswordManagerCriticalActionLoggerTest,
       DoesNotLogCriticalActionIfFeatureDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      critical_actions::features::kCriticalActionHistory);

  EXPECT_CALL(*mock_service(), AddCriticalActionWithNavigationId).Times(0);

  logger()->MaybeLogCriticalAction(
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          web_contents()->GetPrimaryMainFrame()),
      test_url(),
      password_manager::PasswordManagerClient::PasswordFillTrigger::
          kPasswordManagerAutofill);
}

TEST_F(PasswordManagerCriticalActionLoggerTest,
       NotifiesCriticalActionServiceOnUncommittedNavigation) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/other"), web_contents());
  simulator->Start();
  int64_t uncommitted_nav_id =
      simulator->GetNavigationHandle()->GetNavigationId();

  EXPECT_CALL(*mock_service(), OnNavigationDiscarded(uncommitted_nav_id));

  simulator->Fail(net::ERR_ABORTED);

  // Verify that the call happened and clear expectations so that
  // TearDown's DeleteContents() doesn't trigger noise.
  testing::Mock::VerifyAndClearExpectations(mock_service());
}
