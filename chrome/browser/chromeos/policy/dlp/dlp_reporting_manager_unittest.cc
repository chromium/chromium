// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/test_storage_module.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::policy::DMToken;
using ::reporting::test::TestStorageModule;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;

namespace policy {

class DlpReportingManagerTest : public testing::Test {
 protected:
  DlpReportingManagerTest()
      : storage_module_(
            base::MakeRefCounted<reporting::test::TestStorageModule>()) {}

  void SetUpReporting() {
    // Provide a mock cloud policy client.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken(dm_token_.value());
    test_reporting_ =
        std::make_unique<reporting::ReportingClient::TestEnvironment>(
            client_.get());
  }

  void SetUpDlpReportingManager() {
    manager_ = new DlpReportingManager();
    DlpReportingManager::SetDlpReportingManagerForTesting(manager_);
  }

  void SetUpDlpReportingQueue() {
    reporting::StatusOr<std::unique_ptr<reporting::ReportQueueConfiguration>>
        config_result = reporting::ReportQueueConfiguration::Create(
            dm_token_.value(), reporting::Destination::DLP_EVENTS,
            base::BindRepeating(
                []() { return reporting::Status::StatusOK(); }));
    ASSERT_TRUE(config_result.ok());

    reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
        report_queue_result = reporting::ReportQueueImpl::Create(
            std::move(config_result.ValueOrDie()), storage_module_);
    ASSERT_TRUE(report_queue_result.ok());

    manager_->report_queue_ = std::move(report_queue_result.ValueOrDie());
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    testing::Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();

    scoped_feature_list_.InitAndEnableFeature(
        reporting::ReportingClient::kEncryptedReportingPipeline);
    SetUpReporting();
    SetUpDlpReportingManager();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    storage_module_.get()->Release();
    testing::Test::TearDown();
  }

  reporting::test::TestStorageModule* test_storage_module() const {
    reporting::test::TestStorageModule* test_storage_module =
        google::protobuf::down_cast<reporting::test::TestStorageModule*>(
            storage_module_.get());
    DCHECK(test_storage_module);
    return test_storage_module;
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

 protected:
  DlpReportingManager* manager_;
  // BrowserTaskEnvironment needs to be destroyed before TestEnvironment
  // and ScopedFeatureList, so that tasks on other threads don't run after
  // they are destroyed.
  content::BrowserTaskEnvironment task_environment_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<reporting::StorageModuleInterface> storage_module_;
  std::unique_ptr<reporting::ReportingClient::TestEnvironment> test_reporting_;
  const DMToken dm_token_ = DMToken::CreateValidTokenForTesting("TOKEN");
  std::unique_ptr<MockCloudPolicyClient> client_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DlpReportingManagerTest, IsPrintingRestricted) {
  EXPECT_CALL(*test_storage_module(), AddRecord).Times(1);
  SetUpDlpReportingQueue();
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  manager_->ReportPrintingEvent(web_contents.get(),
                                DlpRulesManager::Level::kBlock);
  task_environment_.RunUntilIdle();

  DlpPolicyEvent event_result;
  EXPECT_TRUE(
      event_result.ParseFromString(test_storage_module()->record().data()));
  EXPECT_EQ(event_result.restriction(), DlpPolicyEvent_Restriction_PRINTING);
  EXPECT_EQ(event_result.mode(), DlpPolicyEvent_Mode_BLOCK);
  EXPECT_EQ(event_result.source().url(), web_contents.get()->GetURL().spec());
  EXPECT_EQ(event_result.destination().component(),
            DlpPolicyEventDestination_Component_UNDEFINED_COMPONENT);
  EXPECT_GT(event_result.timestamp(), 0);
  EXPECT_FALSE(event_result.restriction_enforced());
}
}  // namespace policy
