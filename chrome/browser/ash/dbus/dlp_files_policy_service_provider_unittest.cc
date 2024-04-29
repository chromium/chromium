// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "chrome/browser/ash/dbus/dlp_files_policy_service_provider.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/test/mock_dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlp/dbus-constants.h"

namespace ash {

namespace {

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kExampleUrl2[] = "https://example2.com";
constexpr ino_t kInode = 0;
constexpr char kFilePath[] = "test.txt";

using FileDaemonInfo = policy::DlpFilesController::FileDaemonInfo;

}  // namespace

class DlpFilesPolicyServiceProviderTest
    : public policy::DlpFilesTestBase,
      public ::testing::WithParamInterface<policy::DlpRulesManager::Level> {
 protected:
  DlpFilesPolicyServiceProviderTest()
      : dlp_policy_service_(std::make_unique<DlpFilesPolicyServiceProvider>()) {
  }

  DlpFilesPolicyServiceProviderTest(const DlpFilesPolicyServiceProviderTest&) =
      delete;
  DlpFilesPolicyServiceProviderTest& operator=(
      const DlpFilesPolicyServiceProviderTest&) = delete;

  ~DlpFilesPolicyServiceProviderTest() override {
    dbus_service_test_helper_.TearDown();
  }

  void SetUp() override {
    DlpFilesTestBase::SetUp();

    profile_ = TestingProfile::Builder().Build();

    EXPECT_CALL(*rules_manager_, IsFilesPolicyEnabled)
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*rules_manager_, GetReportingManager())
        .WillRepeatedly(::testing::Return(nullptr));
    files_controller_ = std::make_unique<
        testing::StrictMock<policy::MockDlpFilesControllerAsh>>(*rules_manager_,
                                                                profile_.get());
    EXPECT_CALL(*rules_manager_, GetDlpFilesController())
        .WillRepeatedly(::testing::Return(files_controller_.get()));
  }

  template <class ResponseProtoType>
  std::optional<ResponseProtoType> CallDlpFilesPolicyServiceMethod(
      const char* method_name,
      const google::protobuf::MessageLite& request) {
    dbus::MethodCall method_call(dlp::kDlpFilesPolicyServiceInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    dbus_service_test_helper_.SetUp(
        dlp::kDlpFilesPolicyServiceName,
        dbus::ObjectPath(dlp::kDlpFilesPolicyServicePath),
        dlp::kDlpFilesPolicyServiceInterface, method_name,
        dlp_policy_service_.get());
    std::unique_ptr<dbus::Response> dbus_response =
        dbus_service_test_helper_.CallMethod(&method_call);

    if (!dbus_response)
      return {};

    dbus::MessageReader reader(dbus_response.get());
    ResponseProtoType response;
    if (!reader.PopArrayOfBytesAsProto(&response))
      return {};
    return response;
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<DlpFilesPolicyServiceProvider> dlp_policy_service_;
  ServiceProviderTestHelper dbus_service_test_helper_;

  std::unique_ptr<testing::StrictMock<policy::MockDlpFilesControllerAsh>>
      files_controller_;
};

INSTANTIATE_TEST_SUITE_P(
    DlpFilesPolicyServiceProvider,
    DlpFilesPolicyServiceProviderTest,
    ::testing::Values(policy::DlpRulesManager::Level::kAllow,
                      policy::DlpRulesManager::Level::kBlock));

TEST_P(DlpFilesPolicyServiceProviderTest, IsDlpPolicyMatched) {
  dlp::IsDlpPolicyMatchedRequest request;
  request.mutable_file_metadata()->set_inode(kInode);
  request.mutable_file_metadata()->set_path(kFilePath);
  request.mutable_file_metadata()->set_source_url(kExampleUrl);

  policy::DlpRulesManager::Level level = GetParam();
  bool is_restricted = level == policy::DlpRulesManager::Level::kBlock;

  FileDaemonInfo file_info(
      /*inode=*/kInode,
      /*crtime=*/0,
      /*path=*/base::FilePath(),
      /*source_url=*/kExampleUrl,
      /*referrer_url=*/"");
  EXPECT_CALL(*files_controller_.get(), IsDlpPolicyMatched(file_info))
      .WillOnce(testing::Return(is_restricted));
  auto response =
      CallDlpFilesPolicyServiceMethod<dlp::IsDlpPolicyMatchedResponse>(
          dlp::kDlpFilesPolicyServiceIsDlpPolicyMatchedMethod, request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->has_restricted());
  EXPECT_EQ(response->restricted(), (is_restricted));
}

TEST_P(DlpFilesPolicyServiceProviderTest, IsFilesTransferRestricted) {
  dlp::IsFilesTransferRestrictedRequest request;
  request.set_destination_url(kExampleUrl);
  auto* file = request.add_transferred_files();
  file->set_source_url(kExampleUrl2);
  file->set_inode(kInode);
  file->set_path(kFilePath);
  request.set_file_action(::dlp::FileAction::OPEN);
  request.set_io_task_id(1234);

  policy::DlpRulesManager::Level level = GetParam();
  auto restriction_level = (level == policy::DlpRulesManager::Level::kBlock)
                               ? ::dlp::RestrictionLevel::LEVEL_BLOCK
                               : ::dlp::RestrictionLevel::LEVEL_ALLOW;
  FileDaemonInfo file_info(
      /*inode=*/kInode,
      /*crtime=*/0,
      /*path=*/base::FilePath(kFilePath),
      /*source_url=*/kExampleUrl2,
      /*referrer_url=*/"");
  EXPECT_CALL(
      *files_controller_.get(),
      IsFilesTransferRestricted(
          std::optional<file_manager::io_task::IOTaskId>(1234),
          std::vector<FileDaemonInfo>{file_info},
          policy::DlpFileDestination(GURL(kExampleUrl)),
          policy::dlp::FileAction::kOpen, base::test::IsNotNullCallback()))
      .WillOnce(testing::WithArg<4>(
          [&restriction_level, &file_info](
              policy::DlpFilesControllerAsh::IsFilesTransferRestrictedCallback
                  result_callback) {
            std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>>
                result;
            result.push_back(std::make_pair(file_info, restriction_level));
            std::move(result_callback).Run(std::move(result));
          }));

  auto response =
      CallDlpFilesPolicyServiceMethod<dlp::IsFilesTransferRestrictedResponse>(
          dlp::kDlpFilesPolicyServiceIsFilesTransferRestrictedMethod, request);
  ASSERT_TRUE(response.has_value());
  ASSERT_EQ(response->files_restrictions().size(), 1);
  EXPECT_EQ(response->files_restrictions()[0].restriction_level(),
            restriction_level);
}

TEST_P(DlpFilesPolicyServiceProviderTest, IsFilesTransferRestrictedSystem) {
  dlp::IsFilesTransferRestrictedRequest request;
  request.set_destination_component(::dlp::DlpComponent::SYSTEM);
  auto* file = request.add_transferred_files();
  file->set_source_url(kExampleUrl2);
  file->set_inode(kInode);
  file->set_path(kFilePath);
  request.set_file_action(::dlp::FileAction::COPY);
  request.set_io_task_id(1234);

  auto response =
      CallDlpFilesPolicyServiceMethod<dlp::IsFilesTransferRestrictedResponse>(
          dlp::kDlpFilesPolicyServiceIsFilesTransferRestrictedMethod, request);
  ASSERT_TRUE(response.has_value());
  ASSERT_EQ(response->files_restrictions().size(), 1);
  EXPECT_EQ(dlp::RestrictionLevel::LEVEL_ALLOW,
            response->files_restrictions(0).restriction_level());
}

}  // namespace ash
