// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/file_attachment.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int64_t kFileSize = 2048;
const char kFileName[] = "secure_file_name.html";
base::FilePath kFilePath = base::FilePath();
const char kMimeType[] = "text/html";
FileAttachment::Type kMetadataType =
    sharing::mojom::FileMetadata::Type::kUnknown;
}  // namespace

TEST(FileAttachmentTest, FileAttachmentTest) {
  FileAttachment attachment(/*id=*/0, kFileSize, kFileName, kMimeType,
                            kMetadataType);

  EXPECT_EQ(attachment.size(), kFileSize);
  EXPECT_EQ(attachment.file_name(), kFileName);
  EXPECT_EQ(attachment.mime_type(), kMimeType);
  EXPECT_EQ(attachment.type(), kMetadataType);

  attachment.set_file_path(kFilePath);
  EXPECT_EQ(attachment.file_path(), kFilePath);
}
