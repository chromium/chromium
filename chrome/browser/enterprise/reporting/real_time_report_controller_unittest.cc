// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_report_controller.h"

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#include "chrome/browser/enterprise/reporting/real_time_report_controller_desktop.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/reporting/real_time_uploader.h"
#include "components/enterprise/common/proto/extensions_workflow_events.pb.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

namespace enterprise_reporting {

namespace {

class MockRealTimeReportGenerator : public RealTimeReportGenerator {
 public:
  explicit MockRealTimeReportGenerator(
      ReportingDelegateFactory* delegate_factory)
      : RealTimeReportGenerator(delegate_factory) {}

  MOCK_METHOD2(Generate,
               std::vector<std::unique_ptr<google::protobuf::MessageLite>>(
                   ReportType type,
                   const RealTimeReportGenerator::Data& data));
};

class MockRealTimeUploader : public RealTimeUploader {
 public:
  MockRealTimeUploader() : RealTimeUploader(reporting::Priority::FAST_BATCH) {}

  MOCK_METHOD2(Upload,
               void(std::unique_ptr<google::protobuf::MessageLite> report,
                    EnqueueCallback callback));
};

}  // namespace

class RealTimeReportControllerTest : public ::testing::Test {
 public:
  RealTimeReportControllerTest() = default;
  ~RealTimeReportControllerTest() override = default;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  ReportingDelegateFactoryDesktop delegate_factory_;
};

TEST_F(RealTimeReportControllerTest, ExtensionRequest) {
  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports;
  reports.push_back(std::make_unique<ExtensionsWorkflowEvent>());
  reports.push_back(std::make_unique<ExtensionsWorkflowEvent>());

  Profile* profile = profile_manager_.CreateTestingProfile("profile");

  auto report_generator =
      std::make_unique<MockRealTimeReportGenerator>(&delegate_factory_);
  auto report_uploader = std::make_unique<MockRealTimeUploader>();

  RealTimeReportController report_controller{&delegate_factory_};

  EXPECT_CALL(
      *report_generator.get(),
      Generate(RealTimeReportGenerator::ReportType::kExtensionRequest, _))
      .WillOnce(DoAll(
          WithArgs<1>(
              Invoke([profile](const MockRealTimeReportGenerator::Data& data) {
                EXPECT_EQ(profile,
                          static_cast<const ExtensionRequestReportGenerator::
                                          ExtensionRequestData&>(data)
                              .profile);
              })),
          Return(ByMove(std::move(reports)))));
  EXPECT_CALL(*report_uploader, Upload(_, _)).Times(2);

  report_controller.SetExtensionRequestUploaderForTesting(
      std::move(report_uploader));
  report_controller.SetReportGeneratorForTesting(std::move(report_generator));
  report_controller.OnDMTokenUpdated(
      policy::DMToken::CreateValidToken("dm-token"));

  static_cast<RealTimeReportControllerDesktop*>(
      report_controller.GetDelegateForTesting())
      ->TriggerExtensionRequest(profile);
}

}  // namespace enterprise_reporting
