// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/file_attachment.h"

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr base::FilePath::CharType kFilePath[] = FILE_PATH_LITERAL("test.html");

struct FileShareType {
  std::string_view file_name;
  nearby_share::mojom::ShareType share_type;
};

constexpr FileShareType kFileTypes[] = {
    {"test.jpg", nearby_share::mojom::ShareType::kImageFile},
    {"test.mp4", nearby_share::mojom::ShareType::kVideoFile},
    {"test.wav", nearby_share::mojom::ShareType::kAudioFile},
    {"test.pdf", nearby_share::mojom::ShareType::kPdfFile},
    {"test.other", nearby_share::mojom::ShareType::kUnknownFile}};

struct GoogleAppsFileShareType {
  std::string_view file_name;
  std::string_view mime_type;
  nearby_share::mojom::ShareType share_type;
};

constexpr GoogleAppsFileShareType kGoogleAppsFileTypes[] = {
    {"test.gdoc", "application/vnd.google-apps.document",
     nearby_share::mojom::ShareType::kGoogleDocsFile},
    {"test.gsheet", "application/vnd.google-apps.spreadsheet",
     nearby_share::mojom::ShareType::kGoogleSheetsFile},
    {"test.gslides", "application/vnd.google-apps.presentation",
     nearby_share::mojom::ShareType::kGoogleSlidesFile}};

}  // namespace

class FileAttachmentShareTypeTest : public testing::TestWithParam<FileShareType> { };

class FileAttachmentGoogleAppsShareTypeTest : public testing::TestWithParam<GoogleAppsFileShareType> { };

TEST(FileAttachmentTest, CreateFileAttachment) {
  FileAttachment attachment = FileAttachment(base::FilePath(kFilePath));

  EXPECT_EQ(attachment.size(), 0u);
  EXPECT_EQ(attachment.file_name(), "test.html");
  EXPECT_EQ(attachment.mime_type(), "text/html");
  EXPECT_EQ(attachment.type(), FileAttachment::Type::kUnknown);
  EXPECT_EQ(attachment.file_path(), base::FilePath(kFilePath));
  EXPECT_EQ(attachment.GetDescription(), "test.html");
}

TEST(FileAttachmentTest, MoveShareTarget) {
  FileAttachment attachment = FileAttachment(base::FilePath(kFilePath));
  ShareTarget target;
  EXPECT_EQ(target.file_attachments.size(), 0u);
  attachment.MoveToShareTarget(target);
  EXPECT_EQ(target.file_attachments.size(), 1u);
}

TEST_P(FileAttachmentShareTypeTest, GetShareType) {
  FileShareType fileTypePair = GetParam();
  FileAttachment attachment = FileAttachment(base::FilePath(fileTypePair.file_name));
  EXPECT_EQ(attachment.GetShareType(), fileTypePair.share_type);
}

INSTANTIATE_TEST_SUITE_P(FileAttachmentTest, FileAttachmentShareTypeTest,
                         testing::ValuesIn(kFileTypes));

TEST_P(FileAttachmentGoogleAppsShareTypeTest, GetShareType) {
  GoogleAppsFileShareType fileType = GetParam();

  FileAttachment attachment = FileAttachment(
      /*id=*/0u,
      /*size=*/0u, std::string(fileType.file_name),
      std::string(fileType.mime_type), FileAttachment::Type::kUnknown);

  EXPECT_EQ(attachment.GetShareType(), fileType.share_type);
}

INSTANTIATE_TEST_SUITE_P(FileAttachmentTest, FileAttachmentGoogleAppsShareTypeTest,
                         testing::ValuesIn(kGoogleAppsFileTypes));
