// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "content/public/browser/navigation_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

class SendTabToSelfModelMock : public TestSendTabToSelfModel {
 public:
  SendTabToSelfModelMock() = default;
  ~SendTabToSelfModelMock() override = default;

  MOCK_METHOD4(AddEntry,
               const SendTabToSelfEntry*(const GURL&,
                                         const std::string&,
                                         base::Time,
                                         const std::string&));

  bool IsReady() override { return true; }
};

class TestSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService() = default;
  ~TestSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override {
    return &send_tab_to_self_model_mock_;
  }

 protected:
  SendTabToSelfModelMock send_tab_to_self_model_mock_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSendTabToSelfSyncService>();
}

class SendTabToSelfDesktopUtilTest : public BrowserWithTestWindowTest {
 public:
  SendTabToSelfDesktopUtilTest() = default;
  ~SendTabToSelfDesktopUtilTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    url_ = GURL("https://www.google.com");
    title_ = base::UTF8ToUTF16(base::StringPiece("Google"));
  }

  // Set up all test conditions to let ShouldOfferFeature() return true
  void SetUpAllTrueEnv() {
    AddTab(browser(), url_);
    NavigateAndCommitActiveTabWithTitle(browser(), url_, title_);
  }

 protected:
  GURL url_;
  base::string16 title_;
};

TEST_F(SendTabToSelfDesktopUtilTest, CreateNewEntry) {
  SetUpAllTrueEnv();
  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationEntry* entry =
      tab->GetController().GetLastCommittedEntry();

  GURL url = entry->GetURL();
  std::string title = base::UTF16ToUTF8(entry->GetTitle());
  base::Time navigation_time = entry->GetTimestamp();
  std::string target_device_sync_cache_name;
  std::string target_device_sync_cache_guid;

  SendTabToSelfModelMock* model_mock = static_cast<SendTabToSelfModelMock*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile())
          ->GetSendTabToSelfModel());

  EXPECT_CALL(*model_mock, AddEntry(url, title, navigation_time,
                                    target_device_sync_cache_guid))
      .WillOnce(testing::Return(nullptr));
  CreateNewEntry(tab, target_device_sync_cache_name,
                 target_device_sync_cache_guid);

  GURL link_url = GURL("https://www.1112233.com");
  EXPECT_CALL(*model_mock, AddEntry(link_url, "", base::Time(),
                                    target_device_sync_cache_guid))
      .WillOnce(testing::Return(nullptr));
  CreateNewEntry(tab, target_device_sync_cache_name,
                 target_device_sync_cache_guid, link_url);
}

}  // namespace

}  // namespace send_tab_to_self
