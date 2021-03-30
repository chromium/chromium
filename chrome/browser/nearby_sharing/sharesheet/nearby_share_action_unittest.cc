// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEmpty[] = "";
const char kMessage[] = "Message";
const char kTitle[] = "Title";
const char kMimeTypeText[] = "text/plain";
const char kMimeTypeJPG[] = "image/jpg";
const char kTextFile1[] = "file:///path/to/file1.txt";
const char kTextFile2[] = "file:///path/to/file2.txt";
const char kImageFile[] = "file:///path/to/file3.jpg";
const char kUrl[] = "https://google.com";
const char kDriveShareUrl[] = "https://docs.google.com";

struct IntentTestCase {
  apps::mojom::IntentPtr intent;
  bool contains_hosted_document;
  bool should_show_action;
  base::Optional<TextAttachment::Type> text_attachment_type;
  int file_count;
};

}  // namespace

class NearbyShareActionTest : public testing::Test {
 public:
  NearbyShareActionTest(const NearbyShareActionTest&) = delete;
  NearbyShareActionTest& operator=(const NearbyShareActionTest&) = delete;

 protected:
  NearbyShareActionTest() = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing_profile");

    nearby_share_action_.SetNearbyShareDisabledByPolicyForTesting(false);
  }

  std::vector<IntentTestCase> GetIntentTestCases() {
    std::vector<IntentTestCase> test_cases;
    // Simple text share, no title
    test_cases.push_back(
        {apps_util::CreateShareIntentFromText(kMessage, kEmpty),
         /*contains_hosted_document=*/false, /*should_show_action=*/true,
         TextAttachment::Type::kText, /*file_count=*/0});
    // Text share with title
    test_cases.push_back(
        {apps_util::CreateShareIntentFromText(kMessage, kTitle),
         /*contains_hosted_document=*/false, /*should_show_action=*/true,
         TextAttachment::Type::kText, /*file_count=*/0});
    // URL share
    test_cases.push_back({apps_util::CreateIntentFromUrl(GURL(kUrl)),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/true,
                          TextAttachment::Type::kUrl, /*file_count=*/0});
    // Drive share, one file
    test_cases.push_back(
        {apps_util::CreateShareIntentFromDriveFile(
             GURL(kTextFile1), kMimeTypeText, GURL(kDriveShareUrl), false),
         /*contains_hosted_document=*/true, /*should_show_action=*/true,
         TextAttachment::Type::kUrl, /*file_count=*/0});
    // File share, one file
    test_cases.push_back({apps_util::CreateShareIntentFromFiles(
                              {GURL(kImageFile)}, {kMimeTypeJPG}),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/true, base::nullopt,
                          /*file_count=*/1});
    // File share, two text files
    test_cases.push_back({apps_util::CreateShareIntentFromFiles(
                              {GURL(kTextFile1), GURL(kTextFile2)},
                              {kMimeTypeText, kMimeTypeText}),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/true, base::nullopt,
                          /*file_count=*/2});
    // File share, two mixed files
    test_cases.push_back({apps_util::CreateShareIntentFromFiles(
                              {GURL(kTextFile1), GURL(kImageFile)},
                              {kMimeTypeText, kMimeTypeJPG}),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/true, base::nullopt,
                          /*file_count=*/2});
    // File share, one file with title
    test_cases.push_back(
        {apps_util::CreateShareIntentFromFiles({GURL(kImageFile)},
                                               {kMimeTypeJPG}, kEmpty, kTitle),
         /*contains_hosted_document=*/false, /*should_show_action=*/true,
         base::nullopt, /*file_count=*/1});
    // Invalid: File share with text body
    test_cases.push_back({apps_util::CreateShareIntentFromFiles(
                              {GURL(kTextFile1), GURL(kTextFile2)},
                              {kMimeTypeText, kMimeTypeText}, kMessage, kTitle),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/false,
                          TextAttachment::Type::kText, /*file_count=*/2});
    return test_cases;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  Profile* profile_;
  NearbyShareAction nearby_share_action_;
};

TEST_F(NearbyShareActionTest, ShouldShowAction) {
  for (auto& test_case : GetIntentTestCases()) {
    EXPECT_EQ(
        nearby_share_action_.ShouldShowAction(
            std::move(test_case.intent), test_case.contains_hosted_document),
        test_case.should_show_action);
  }
}

TEST_F(NearbyShareActionTest, CreateAttachmentsFromIntent) {
  for (auto& test_case : GetIntentTestCases()) {
    auto attachments = NearbyShareAction::CreateAttachmentsFromIntent(
        profile_, std::move(test_case.intent));
    int text_attachment_count = 0;
    int file_attachment_count = 0;
    for (std::unique_ptr<Attachment>& attachment : attachments) {
      if (attachment->family() == Attachment::Family::kText)
        ++text_attachment_count;
      else
        ++file_attachment_count;
    }
    if ((text_attachment_count && file_attachment_count) ||
        (!text_attachment_count && !file_attachment_count)) {
      // There should be at least one kind of attachment but not both if this is
      // a valid intent for Nearby Sharing
      EXPECT_FALSE(test_case.should_show_action);
      continue;
    }
    if (text_attachment_count) {
      ASSERT_EQ(text_attachment_count, 1);
      ASSERT_TRUE(test_case.text_attachment_type);
      auto* text_attachment =
          static_cast<TextAttachment*>(attachments[0].get());
      EXPECT_EQ(text_attachment->type(), *test_case.text_attachment_type);
      continue;
    }
    EXPECT_EQ(file_attachment_count, test_case.file_count);
  }
}
