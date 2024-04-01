// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sharesheet/copy_to_clipboard_share_action.h"

#include "ash/public/cpp/system/toast_data.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sharesheet/share_action/share_action_cache.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_test_util.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "url/gurl.h"

namespace ash::sharesheet {

namespace {

class MockCopyToClipboardShareAction : public CopyToClipboardShareAction {
 public:
  explicit MockCopyToClipboardShareAction(Profile* profile)
      : CopyToClipboardShareAction(profile) {}

  MOCK_METHOD(void, ShowToast, (ash::ToastData toast_data), (override));
};

}  // namespace

class CopyToClipboardShareActionTest : public ChromeAshTestBase {
 public:
  CopyToClipboardShareActionTest() = default;

  // ChromeAshTestBase:
  void SetUp() override {
    ChromeAshTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();
    share_action_cache_ =
        std::make_unique<::sharesheet::ShareActionCache>(profile_.get());
  }

  Profile* profile() { return profile_.get(); }
  ::sharesheet::ShareActionCache* share_action_cache() {
    return share_action_cache_.get();
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<::sharesheet::ShareActionCache> share_action_cache_;
};

TEST_F(CopyToClipboardShareActionTest, CopyToClipboardText) {
  base::HistogramTester histograms;
  auto* copy_action = share_action_cache()->GetActionFromType(
      ::sharesheet::ShareActionType::kCopyToClipboardShare);
  copy_action->LaunchAction(/*controller=*/nullptr, /*root_view=*/nullptr,
                            ::sharesheet::CreateValidTextIntent());
  // Check text copied correctly.
  std::u16string clipboard_text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
      &clipboard_text);
  EXPECT_EQ(::sharesheet::kTestText, base::UTF16ToUTF8(clipboard_text));
  histograms.ExpectBucketCount(
      ::sharesheet::kSharesheetCopyToClipboardMimeTypeResultHistogram,
      ::sharesheet::SharesheetMetrics::MimeType::kText, 1);
}

TEST_F(CopyToClipboardShareActionTest, CopyToClipboardUrl) {
  base::HistogramTester histograms;
  auto* copy_action = share_action_cache()->GetActionFromType(
      ::sharesheet::ShareActionType::kCopyToClipboardShare);
  copy_action->LaunchAction(/*controller=*/nullptr, /*root_view=*/nullptr,
                            ::sharesheet::CreateValidUrlIntent());
  // Check url copied correctly.
  std::u16string clipboard_url;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
      &clipboard_url);
  EXPECT_EQ(::sharesheet::kTestUrl, base::UTF16ToUTF8(clipboard_url));
  histograms.ExpectBucketCount(
      ::sharesheet::kSharesheetCopyToClipboardMimeTypeResultHistogram,
      ::sharesheet::SharesheetMetrics::MimeType::kUrl, 1);
}

TEST_F(CopyToClipboardShareActionTest, CopyToClipboardOneFile) {
  base::HistogramTester histograms;
  auto* copy_action = share_action_cache()->GetActionFromType(
      ::sharesheet::ShareActionType::kCopyToClipboardShare);
  storage::FileSystemURL url = ::sharesheet::FileInDownloads(
      profile(), base::FilePath(::sharesheet::kTestTextFile));
  copy_action->LaunchAction(
      /*controller=*/nullptr, /*root_view=*/nullptr,
      apps_util::MakeShareIntent({url.ToGURL()},
                                 {::sharesheet::kMimeTypeText}));

  // Check filenames copied correctly.
  std::vector<ui::FileInfo> filenames;
  ui::Clipboard::GetForCurrentThread()->ReadFilenames(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &filenames);
  EXPECT_EQ(filenames.size(), 1u);
  EXPECT_EQ(url.path(), filenames[0].path);
  histograms.ExpectBucketCount(
      ::sharesheet::kSharesheetCopyToClipboardMimeTypeResultHistogram,
      ::sharesheet::SharesheetMetrics::MimeType::kTextFile, 1);
}

TEST_F(CopyToClipboardShareActionTest, CopyToClipboardMultipleFiles) {
  base::HistogramTester histograms;
  auto* copy_action = share_action_cache()->GetActionFromType(
      ::sharesheet::ShareActionType::kCopyToClipboardShare);
  storage::FileSystemURL url1 = ::sharesheet::FileInDownloads(
      profile(), base::FilePath(::sharesheet::kTestPdfFile));
  storage::FileSystemURL url2 = ::sharesheet::FileInDownloads(
      profile(), base::FilePath(::sharesheet::kTestTextFile));
  copy_action->LaunchAction(
      /*controller=*/nullptr, /*root_view=*/nullptr,
      apps_util::MakeShareIntent(
          {url1.ToGURL(), url2.ToGURL()},
          {::sharesheet::kMimeTypePdf, ::sharesheet::kMimeTypeText}));

  // Check filenames copied correctly.
  std::vector<ui::FileInfo> filenames;
  ui::Clipboard::GetForCurrentThread()->ReadFilenames(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &filenames);
  EXPECT_EQ(filenames.size(), 2u);
  EXPECT_EQ(url1.path(), filenames[0].path);
  EXPECT_EQ(url2.path(), filenames[1].path);
  histograms.ExpectBucketCount(
      ::sharesheet::kSharesheetCopyToClipboardMimeTypeResultHistogram,
      ::sharesheet::SharesheetMetrics::MimeType::kTextFile, 1);
  histograms.ExpectBucketCount(
      ::sharesheet::kSharesheetCopyToClipboardMimeTypeResultHistogram,
      ::sharesheet::SharesheetMetrics::MimeType::kPdfFile, 1);
}

TEST_F(CopyToClipboardShareActionTest,
       CopyToClipboardShouldShowActionNonNativeFile) {
  auto* copy_action = share_action_cache()->GetActionFromType(
      ::sharesheet::ShareActionType::kCopyToClipboardShare);
  storage::FileSystemURL url1 = ::sharesheet::FileInNonNativeFileSystemType(
      profile(), base::FilePath(::sharesheet::kTestPdfFile));
  EXPECT_TRUE(copy_action->ShouldShowAction(
      apps_util::MakeShareIntent({url1.ToGURL()}, {::sharesheet::kMimeTypePdf}),
      /* contains_hosted_document= */ false));
}

TEST_F(CopyToClipboardShareActionTest,
       CopyToClipboardShouldShowActionNativeFile) {
  auto* copy_action = share_action_cache()->GetActionFromType(
      ::sharesheet::ShareActionType::kCopyToClipboardShare);
  storage::FileSystemURL url1 = ::sharesheet::FileInDownloads(
      profile(), base::FilePath(::sharesheet::kTestPdfFile));
  EXPECT_TRUE(copy_action->ShouldShowAction(
      apps_util::MakeShareIntent({url1.ToGURL()}, {::sharesheet::kMimeTypePdf}),
      /* contains_hosted_document= */ false));
}

TEST_F(CopyToClipboardShareActionTest,
       CopyToClipboardShouldShowActionHostedDocument) {
  auto* copy_action = share_action_cache()->GetActionFromType(
      ::sharesheet::ShareActionType::kCopyToClipboardShare);
  EXPECT_FALSE(
      copy_action->ShouldShowAction(::sharesheet::CreateDriveIntent(),
                                    /* contains_hosted_document= */ true));
}

TEST_F(CopyToClipboardShareActionTest, CopyTextShowsToast) {
  ::testing::StrictMock<MockCopyToClipboardShareAction> copy_action(profile());
  EXPECT_CALL(copy_action, ShowToast);

  storage::FileSystemURL url = ::sharesheet::FileInDownloads(
      profile(), base::FilePath(::sharesheet::kTestTextFile));
  copy_action.LaunchAction(
      /*controller=*/nullptr, /*root_view=*/nullptr,
      apps_util::MakeShareIntent({url.ToGURL()},
                                 {::sharesheet::kMimeTypeText}));
}

TEST_F(CopyToClipboardShareActionTest, CopyFilesShowsToast) {
  ::testing::StrictMock<MockCopyToClipboardShareAction> copy_action(profile());
  EXPECT_CALL(copy_action, ShowToast);

  storage::FileSystemURL url1 = ::sharesheet::FileInDownloads(
      profile(), base::FilePath(::sharesheet::kTestPdfFile));
  storage::FileSystemURL url2 = ::sharesheet::FileInDownloads(
      profile(), base::FilePath(::sharesheet::kTestTextFile));
  copy_action.LaunchAction(
      /*controller=*/nullptr, /*root_view=*/nullptr,
      apps_util::MakeShareIntent(
          {url1.ToGURL(), url2.ToGURL()},
          {::sharesheet::kMimeTypePdf, ::sharesheet::kMimeTypeText}));
}

TEST_F(CopyToClipboardShareActionTest, CopyToClipboardMultipleImageFiles) {
  base::HistogramTester histograms;
  auto* copy_action = share_action_cache()->GetActionFromType(
      ::sharesheet::ShareActionType::kCopyToClipboardShare);
  storage::FileSystemURL url1 = ::sharesheet::FileInDownloads(
      profile(), base::FilePath("path/to/image1.png"));
  storage::FileSystemURL url2 = ::sharesheet::FileInDownloads(
      profile(), base::FilePath("path/to/image2.jpg"));
  copy_action->LaunchAction(
      /*controller=*/nullptr, /*root_view=*/nullptr,
      apps_util::MakeShareIntent({url1.ToGURL(), url2.ToGURL()},
                                 {"image/png", "image/jpg"}));

  // Check filenames copied correctly.
  std::vector<ui::FileInfo> filenames;
  ui::Clipboard::GetForCurrentThread()->ReadFilenames(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &filenames);
  EXPECT_EQ(filenames.size(), 2u);
  EXPECT_EQ(url1.path(), filenames[0].path);
  EXPECT_EQ(url2.path(), filenames[1].path);
  histograms.ExpectBucketCount(
      ::sharesheet::kSharesheetCopyToClipboardMimeTypeResultHistogram,
      ::sharesheet::SharesheetMetrics::MimeType::kImageFile, 1);
}

}  // namespace ash::sharesheet
