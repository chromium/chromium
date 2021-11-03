// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action/share_action.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/share_action/share_action_cache.h"
#include "chrome/browser/sharesheet/sharesheet_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

namespace sharesheet {

class ShareActionTest : public ::testing::Test {
 public:
  ShareActionTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSharesheetCopyToClipboard);
  }

  // Test:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    share_action_cache_ = std::make_unique<ShareActionCache>(profile_.get());
  }

  Profile* profile() { return profile_.get(); }
  ShareActionCache* share_action_cache() { return share_action_cache_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<ShareActionCache> share_action_cache_;
};

TEST_F(ShareActionTest, ShareActionCacheGetAllActions) {
  auto& share_actions = share_action_cache()->GetShareActions();
  // We don't expect Nearby Share because it's not supported in this context.
  EXPECT_EQ(share_actions.size(), 2u);
  // We can check like this because ordering of actions in |share_action_cache_|
  // is predetermined.
  EXPECT_EQ(share_actions[0]->GetActionName(),
            l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL));
  EXPECT_EQ(share_actions[1]->GetActionName(),
            l10n_util::GetStringUTF16(
                IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL));
}

TEST_F(ShareActionTest, ShareActionCacheGetActionFromName) {
  auto name = l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL);
  l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL);
  auto* share_action = share_action_cache()->GetActionFromName(name);
  EXPECT_EQ(share_action->GetActionName(), name);

  name = l10n_util::GetStringUTF16(
      IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL);
  share_action = share_action_cache()->GetActionFromName(name);
  EXPECT_EQ(share_action->GetActionName(), name);
}

TEST_F(ShareActionTest, ShareActionCacheHasVisibleActions) {
  EXPECT_TRUE(share_action_cache()->HasVisibleActions(
      CreateValidTextIntent(), /*contains_hosted_document=*/false));
  // False due to invalid intent.
  EXPECT_FALSE(share_action_cache()->HasVisibleActions(
      CreateInvalidIntent(), /*contains_hosted_document=*/false));
  // False if the intent contains a hosted document that is not a drive intent.
  EXPECT_FALSE(share_action_cache()->HasVisibleActions(
      CreateValidTextIntent(), /*contains_hosted_document=*/true));
  // True as a drive intent means drive_share_action is visible.
  EXPECT_TRUE(share_action_cache()->HasVisibleActions(
      CreateDriveIntent(), /*contains_hosted_document=*/true));
}

TEST_F(ShareActionTest, CopyToClipboardText) {
  auto* copy_action =
      share_action_cache()->GetActionFromName(l10n_util::GetStringUTF16(
          IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL));
  copy_action->LaunchAction(/*controller=*/nullptr, /*root_view=*/nullptr,
                            CreateValidTextIntent());
  // Check text copied correctly.
  std::u16string clipboard_text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
      &clipboard_text);
  EXPECT_EQ(::sharesheet::kTestText, base::UTF16ToUTF8(clipboard_text));
}

TEST_F(ShareActionTest, CopyToClipboardUrl) {
  auto* copy_action =
      share_action_cache()->GetActionFromName(l10n_util::GetStringUTF16(
          IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL));
  copy_action->LaunchAction(/*controller=*/nullptr, /*root_view=*/nullptr,
                            CreateValidUrlIntent());
  // Check url copied correctly.
  std::u16string clipboard_url;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
      &clipboard_url);
  EXPECT_EQ(::sharesheet::kTestUrl, base::UTF16ToUTF8(clipboard_url));
}

TEST_F(ShareActionTest, CopyToClipboardOneFile) {
  auto* copy_action =
      share_action_cache()->GetActionFromName(l10n_util::GetStringUTF16(
          IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL));
  storage::FileSystemURL url =
      FileInDownloads(profile(), base::FilePath(kTestTextFile));
  copy_action->LaunchAction(
      /*controller=*/nullptr, /*root_view=*/nullptr,
      apps_util::CreateShareIntentFromFiles({url.ToGURL()}, {kMimeTypeText}));

  // Check filenames copied correctly.
  std::vector<ui::FileInfo> filenames;
  ui::Clipboard::GetForCurrentThread()->ReadFilenames(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &filenames);
  EXPECT_EQ(filenames.size(), 1u);
  EXPECT_EQ(url.path(), filenames[0].path);
}

TEST_F(ShareActionTest, CopyToClipboardMultipleFiles) {
  auto* copy_action =
      share_action_cache()->GetActionFromName(l10n_util::GetStringUTF16(
          IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL));
  storage::FileSystemURL url1 =
      FileInDownloads(profile(), base::FilePath(kTestPdfFile));
  storage::FileSystemURL url2 =
      FileInDownloads(profile(), base::FilePath(kTestTextFile));
  copy_action->LaunchAction(
      /*controller=*/nullptr, /*root_view=*/nullptr,
      apps_util::CreateShareIntentFromFiles({url1.ToGURL(), url2.ToGURL()},
                                            {kMimeTypePdf, kMimeTypeText}));

  // Check filenames copied correctly.
  std::vector<ui::FileInfo> filenames;
  ui::Clipboard::GetForCurrentThread()->ReadFilenames(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &filenames);
  EXPECT_EQ(filenames.size(), 2u);
  EXPECT_EQ(url1.path(), filenames[0].path);
  EXPECT_EQ(url2.path(), filenames[1].path);
}

}  // namespace sharesheet
