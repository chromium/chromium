// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"

#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace policy {

namespace {

constexpr char kExampleSourcePattern1[] = "example1.com";
constexpr char kExampleSourcePattern2[] = "example2.com";

}  // namespace

class DlpFilesUtilsTest
    : public DlpFilesTestBase,
      public ::testing::WithParamInterface<
          std::tuple<data_controls::Component, ::dlp::DlpComponent>> {
 public:
  DlpFilesUtilsTest(const DlpFilesUtilsTest&) = delete;
  DlpFilesUtilsTest& operator=(const DlpFilesUtilsTest&) = delete;

 protected:
  DlpFilesUtilsTest() = default;
  ~DlpFilesUtilsTest() = default;
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesUtilsTest,
    ::testing::Values(
        std::make_tuple(data_controls::Component::kUnknownComponent,
                        ::dlp::DlpComponent::UNKNOWN_COMPONENT),
        std::make_tuple(data_controls::Component::kArc,
                        ::dlp::DlpComponent::ARC),
        std::make_tuple(data_controls::Component::kCrostini,
                        ::dlp::DlpComponent::CROSTINI),
        std::make_tuple(data_controls::Component::kPluginVm,
                        ::dlp::DlpComponent::PLUGIN_VM),
        std::make_tuple(data_controls::Component::kUsb,
                        ::dlp::DlpComponent::USB),
        std::make_tuple(data_controls::Component::kDrive,
                        ::dlp::DlpComponent::GOOGLE_DRIVE),
        std::make_tuple(data_controls::Component::kOneDrive,
                        ::dlp::DlpComponent::MICROSOFT_ONEDRIVE)));

TEST_P(DlpFilesUtilsTest, TestConvert) {
  auto [component, proto] = GetParam();
  EXPECT_EQ(proto, dlp::MapPolicyComponentToProto(component));
}

TEST_F(DlpFilesUtilsTest, IsFilesTransferBlocked_NoneBlocked) {
  const std::vector<std::string> sources = {
      kExampleSourcePattern1, kExampleSourcePattern2, std::string()};

  EXPECT_CALL(
      *rules_manager_,
      IsRestrictedComponent(_, data_controls::Component::kOneDrive, _, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));

  EXPECT_FALSE(dlp::IsFilesTransferBlocked(
      sources, data_controls::Component::kOneDrive));
}

TEST_F(DlpFilesUtilsTest, IsFilesTransferBlocked_SomeBlocked) {
  const std::vector<std::string> sources = {
      kExampleSourcePattern1, kExampleSourcePattern2, std::string()};

  EXPECT_CALL(
      *rules_manager_,
      IsRestrictedComponent(_, data_controls::Component::kOneDrive, _, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  EXPECT_TRUE(dlp::IsFilesTransferBlocked(sources,
                                          data_controls::Component::kOneDrive));
}

}  // namespace policy
