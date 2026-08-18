// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_notification_handler_android.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

namespace {
constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "bcdefghijklmnopabcdefghijklmnopa";
}  // namespace

class ExtensionRequestNotificationHandlerTest : public ::testing::Test {
 public:
  ExtensionRequestNotificationHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ExtensionRequestNotificationHandlerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("TestProfile");
  }

  void TearDown() override {
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  void SetPendingList(const std::vector<std::string>& ids) {
    base::DictValue id_values;
    for (const auto& id : ids) {
      id_values.Set(id, base::DictValue());
    }
    profile_->GetTestingPrefService()->SetUserPref(
        enterprise_reporting::kCloudExtensionRequestIds, std::move(id_values));
  }

  std::vector<std::string> GetPendingList() {
    std::vector<std::string> ids;
    const base::DictValue& pending_requests = profile_->GetPrefs()->GetDict(
        enterprise_reporting::kCloudExtensionRequestIds);
    for (auto it : pending_requests) {
      ids.push_back(it.first);
    }
    return ids;
  }

  TestingProfile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(ExtensionRequestNotificationHandlerTest, ParseExtensionIds) {
  EXPECT_THAT(ExtensionRequestNotificationHandler::ParseExtensionIds(
                  "ext_req|extension_approved_notification|id1,id2"),
              ::testing::ElementsAre("id1", "id2"));
  EXPECT_THAT(ExtensionRequestNotificationHandler::ParseExtensionIds(
                  "ext_req|extension_approved_notification|id1"),
              ::testing::ElementsAre("id1"));
  EXPECT_THAT(ExtensionRequestNotificationHandler::ParseExtensionIds(
                  "ext_req|id1 , id2 "),
              ::testing::ElementsAre("id1", "id2"));
  EXPECT_TRUE(ExtensionRequestNotificationHandler::ParseExtensionIds(
                  "ext_req||id1,,id2")
                  .size() == 2);
  EXPECT_DCHECK_DEATH(ExtensionRequestNotificationHandler::ParseExtensionIds(
      "invalid_prefix|id1,id2"));
  EXPECT_TRUE(ExtensionRequestNotificationHandler::ParseExtensionIds("ext_req|")
                  .empty());
}

TEST_F(ExtensionRequestNotificationHandlerTest, OnCloseByUserRemovesRequests) {
  SetPendingList({kExtensionId1, kExtensionId2});
  EXPECT_EQ(2u, GetPendingList().size());

  ExtensionRequestNotificationHandler handler;
  base::RunLoop run_loop;
  handler.OnClose(profile(), GURL("https://chrome.google.com"),
                  "ext_req|" + std::string(kExtensionId1),
                  /*by_user=*/true, run_loop.QuitClosure());
  run_loop.Run();

  std::vector<std::string> remaining = GetPendingList();
  EXPECT_EQ(1u, remaining.size());
  EXPECT_EQ(kExtensionId2, remaining[0]);
}

TEST_F(ExtensionRequestNotificationHandlerTest,
       OnCloseNotByUserLeavesRequests) {
  SetPendingList({kExtensionId1, kExtensionId2});
  EXPECT_EQ(2u, GetPendingList().size());

  ExtensionRequestNotificationHandler handler;
  base::RunLoop run_loop;
  handler.OnClose(profile(), GURL("https://chrome.google.com"),
                  "ext_req|" + std::string(kExtensionId1),
                  /*by_user=*/false, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(2u, GetPendingList().size());
}

class TestExtensionRequestNotificationHandler
    : public ExtensionRequestNotificationHandler {
 public:
  void LaunchWebStoreUrls(
      const std::vector<std::string>& extension_ids) override {
    launched_extension_ids_.push_back(extension_ids);
  }

  const std::vector<std::vector<std::string>>& launched_extension_ids() const {
    return launched_extension_ids_;
  }

 private:
  std::vector<std::vector<std::string>> launched_extension_ids_;
};

TEST_F(ExtensionRequestNotificationHandlerTest, OnClickRemovesRequests) {
  SetPendingList({kExtensionId1, kExtensionId2});
  EXPECT_EQ(2u, GetPendingList().size());

  TestExtensionRequestNotificationHandler handler;
  base::RunLoop run_loop;
  handler.OnClick(profile(), GURL("https://chrome.google.com"),
                  "ext_req|" + std::string(kExtensionId1),
                  /*action_index=*/std::nullopt, /*reply=*/std::nullopt,
                  run_loop.QuitClosure());
  run_loop.Run();

  std::vector<std::string> remaining = GetPendingList();
  EXPECT_EQ(1u, remaining.size());
  EXPECT_EQ(kExtensionId2, remaining[0]);
  EXPECT_THAT(handler.launched_extension_ids(),
              ::testing::ElementsAre(std::vector<std::string>{kExtensionId1}));
}

}  // namespace enterprise_reporting
