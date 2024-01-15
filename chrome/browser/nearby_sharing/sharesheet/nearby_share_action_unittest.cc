// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEmpty[] = "";
const char kMessage[] = "Message";
const char kTitle[] = "Title";
const char kMimeTypeText[] = "text/plain";
const char kMimeTypeJPG[] = "image/jpg";
const char kTextFile1[] = "file1.txt";
const char kTextFile2[] = "file2.txt";
const char kImageFile[] = "file3.jpg";
const char kUrl[] = "https://google.com";
const char kDriveShareUrl[] = "https://docs.google.com";

struct IntentTestCase {
  apps::IntentPtr intent;
  bool contains_hosted_document;
  bool should_show_action;
  std::optional<TextAttachment::Type> text_attachment_type;
  int file_count;
};

void ActionCleanupCallbackStub() {
  return;
}

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

    nearby_share_action_ = std::make_unique<NearbyShareAction>(profile_);
    nearby_share_action_->SetNearbyShareDisabledByPolicyForTesting(false);
    // Calling the cleanup callback means an error occurred in the function.
    ASSERT_DEATH(ActionCleanupCallbackStub(), "");
    nearby_share_action_->SetActionCleanupCallbackForArc(
        base::BindOnce(&ActionCleanupCallbackStub));

    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    mount_points->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile_),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        file_manager::util::GetDownloadsFolderForProfile(profile_));
  }

  GURL GetFileSystemUrl(const std::string& file_name) {
    url::Origin origin =
        url::Origin::Create(file_manager::util::GetFileManagerURL());
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(profile_);
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    return mount_points
        ->CreateExternalFileSystemURL(
            blink::StorageKey::CreateFirstParty(origin), mount_point_name,
            base::FilePath(file_name))
        .ToGURL();
  }

  std::vector<IntentTestCase> GetIntentTestCases() {
    std::vector<IntentTestCase> test_cases;
    // Simple text share, no title
    test_cases.push_back({apps_util::MakeShareIntent(kMessage, kEmpty),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/true,
                          TextAttachment::Type::kText, /*file_count=*/0});
    // Text share with title
    test_cases.push_back({apps_util::MakeShareIntent(kMessage, kTitle),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/true,
                          TextAttachment::Type::kText, /*file_count=*/0});
    // URL share
    test_cases.push_back({std::make_unique<apps::Intent>(
                              apps_util::kIntentActionView, GURL(kUrl)),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/true,
                          TextAttachment::Type::kUrl, /*file_count=*/0});
    // Drive share, one file
    test_cases.push_back(
        {apps_util::MakeShareIntent(GetFileSystemUrl(kTextFile1), kMimeTypeText,
                                    GURL(kDriveShareUrl), false),
         /*contains_hosted_document=*/true, /*should_show_action=*/true,
         TextAttachment::Type::kUrl, /*file_count=*/0});
    // File share, one file
    test_cases.push_back({apps_util::MakeShareIntent(
                              {GetFileSystemUrl(kImageFile)}, {kMimeTypeJPG}),
                          /*contains_hosted_document=*/false,
                          /*should_show_action=*/true, std::nullopt,
                          /*file_count=*/1});
    // File share, two text files
    test_cases.push_back(
        {apps_util::MakeShareIntent(
             {GetFileSystemUrl(kTextFile1), GetFileSystemUrl(kTextFile2)},
             {kMimeTypeText, kMimeTypeText}),
         /*contains_hosted_document=*/false,
         /*should_show_action=*/true, std::nullopt,
         /*file_count=*/2});
    // File share, two mixed files
    test_cases.push_back(
        {apps_util::MakeShareIntent(
             {GetFileSystemUrl(kTextFile1), GetFileSystemUrl(kImageFile)},
             {kMimeTypeText, kMimeTypeJPG}),
         /*contains_hosted_document=*/false,
         /*should_show_action=*/true, std::nullopt,
         /*file_count=*/2});
    // File share, one file with title
    test_cases.push_back(
        {apps_util::MakeShareIntent({GetFileSystemUrl(kImageFile)},
                                    {kMimeTypeJPG}, kEmpty, kTitle),
         /*contains_hosted_document=*/false, /*should_show_action=*/true,
         std::nullopt, /*file_count=*/1});
    // Invalid: File share with text body
    test_cases.push_back(
        {apps_util::MakeShareIntent(
             {GetFileSystemUrl(kTextFile1), GetFileSystemUrl(kTextFile2)},
             {kMimeTypeText, kMimeTypeText}, kMessage, kTitle),
         /*contains_hosted_document=*/false,
         /*should_show_action=*/false, TextAttachment::Type::kText,
         /*file_count=*/2});
    return test_cases;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<NearbyShareAction> nearby_share_action_;
};

TEST_F(NearbyShareActionTest, ShouldShowAction) {
  for (auto& test_case : GetIntentTestCases()) {
    EXPECT_EQ(
        nearby_share_action_->ShouldShowAction(
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

// Verify that a file attachment uses the file name given in the Intent, when
// available.
TEST_F(NearbyShareActionTest, CreateAttachmentFromIntentWithCustomName) {
  const base::FilePath kTestPath =
      base::FilePath("/some/path/with/opaque/name/0123456ABCDEF");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionSendMultiple);

  auto file =
      std::make_unique<apps::IntentFile>(net::FilePathToFileURL(kTestPath));
  file->file_name = base::SafeBaseName::Create("foo.jpg");
  intent->files.push_back(std::move(file));

  auto attachments = NearbyShareAction::CreateAttachmentsFromIntent(
      profile_, std::move(intent));

  ASSERT_EQ(attachments.size(), 1u);
  ASSERT_EQ(attachments[0]->family(), Attachment::Family::kFile);

  auto* file_attachment = static_cast<FileAttachment*>(attachments[0].get());
  ASSERT_EQ(file_attachment->file_name(), "foo.jpg");
  ASSERT_EQ(file_attachment->type(), FileAttachment::Type::kImage);
  ASSERT_EQ(file_attachment->file_path(), kTestPath);
}
