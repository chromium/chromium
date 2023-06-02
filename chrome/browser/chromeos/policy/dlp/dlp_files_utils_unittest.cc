// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"

#include "chrome/browser/enterprise/data_controls/component.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DlpFilesUtilsTest
    : public testing::Test,
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

}  // namespace policy
