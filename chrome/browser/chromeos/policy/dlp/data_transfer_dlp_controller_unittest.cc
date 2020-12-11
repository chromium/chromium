// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/origin.h"

namespace policy {

namespace {

constexpr char kGoogleUrl[] = "https://www.google.com";
constexpr char kYoutubeUrl[] = "https://www.youtube.com";

class MockDlpRulesManager : public DlpRulesManager {
 public:
  explicit MockDlpRulesManager(PrefService* local_state)
      : DlpRulesManager(local_state) {}
  ~MockDlpRulesManager() override = default;

  MOCK_CONST_METHOD3(IsRestrictedDestination,
                     Level(const GURL& source,
                           const GURL& destination,
                           Restriction restriction));

  MOCK_CONST_METHOD3(IsRestrictedComponent,
                     Level(const GURL& source,
                           const Component& destination,
                           Restriction restriction));

  MOCK_CONST_METHOD3(IsRestrictedAnyOfComponents,
                     Level(const GURL& source,
                           const std::vector<Component>& destinations,
                           Restriction restriction));
};

class MockDlpController : public DataTransferDlpController {
 public:
  MOCK_METHOD2(DoNotifyBlockedPaste,
               void(const ui::DataTransferEndpoint* const data_src,
                    const ui::DataTransferEndpoint* const data_dst));
};

}  // namespace

class DataTransferDlpControllerTest : public testing::Test {
 protected:
  DataTransferDlpControllerTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()),
        rules_manager_(scoped_testing_local_state_.Get()) {
    DlpRulesManagerFactory::OverrideManagerForTesting(&rules_manager_);
  }

  ~DataTransferDlpControllerTest() override = default;

  ScopedTestingLocalState scoped_testing_local_state_;
  ::testing::StrictMock<MockDlpRulesManager> rules_manager_;
  ::testing::StrictMock<MockDlpController> dlp_controller_;
};

TEST_F(DataTransferDlpControllerTest, NullSrc) {
  EXPECT_EQ(true, dlp_controller_.IsDataReadAllowed(nullptr, nullptr));
}

TEST_F(DataTransferDlpControllerTest, NullDst) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kGoogleUrl)));
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_CALL(dlp_controller_, DoNotifyBlockedPaste);
  EXPECT_EQ(false, dlp_controller_.IsDataReadAllowed(&data_src, nullptr));
}

TEST_F(DataTransferDlpControllerTest, DefaultDst) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kGoogleUrl)));
  ui::DataTransferEndpoint data_dst_1(ui::EndpointType::kDefault);
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_CALL(dlp_controller_, DoNotifyBlockedPaste);
  EXPECT_EQ(false, dlp_controller_.IsDataReadAllowed(&data_src, &data_dst_1));
  testing::Mock::VerifyAndClearExpectations(&rules_manager_);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // Turn off notifications
  ui::DataTransferEndpoint data_dst_2(ui::EndpointType::kDefault,
                                      /*notify_if_restricted=*/false);
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_EQ(false, dlp_controller_.IsDataReadAllowed(&data_src, &data_dst_2));
}

TEST_F(DataTransferDlpControllerTest, ClipboardHistoryDst) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kGoogleUrl)));
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  EXPECT_EQ(true, dlp_controller_.IsDataReadAllowed(&data_src, &data_dst));
}

TEST_F(DataTransferDlpControllerTest, UrlSrcDst) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kGoogleUrl)));
  ui::DataTransferEndpoint data_dst_1(url::Origin::Create(GURL(kYoutubeUrl)));
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_CALL(dlp_controller_, DoNotifyBlockedPaste);
  EXPECT_EQ(false, dlp_controller_.IsDataReadAllowed(&data_src, &data_dst_1));
  testing::Mock::VerifyAndClearExpectations(&rules_manager_);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // Turn off notifications
  ui::DataTransferEndpoint data_dst_2(url::Origin::Create(GURL(kYoutubeUrl)),
                                      /*notify_if_restricted=*/false);
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_EQ(false, dlp_controller_.IsDataReadAllowed(&data_src, &data_dst_2));
}

TEST_F(DataTransferDlpControllerTest, ArcDst) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kGoogleUrl)));
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kArc);
  EXPECT_CALL(rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_CALL(dlp_controller_, DoNotifyBlockedPaste);
  EXPECT_EQ(false, dlp_controller_.IsDataReadAllowed(&data_src, &data_dst));
}

TEST_F(DataTransferDlpControllerTest, CrostiniDst) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kGoogleUrl)));
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kCrostini);
  EXPECT_CALL(rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_CALL(dlp_controller_, DoNotifyBlockedPaste);
  EXPECT_EQ(false, dlp_controller_.IsDataReadAllowed(&data_src, &data_dst));
}

}  // namespace policy
