// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/uma_enums.gen.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager::file_tasks {

TEST(UmaEnumsTest, Test) {
  EXPECT_EQ(GetViewFileType(base::FilePath("file.jpg")), ViewFileType::kJpg);
  EXPECT_EQ(GetViewFileType(base::FilePath("file.jpeg")), ViewFileType::kJpeg);
  EXPECT_EQ(GetViewFileType(base::FilePath("file.pdf")), ViewFileType::kPdf);
  EXPECT_EQ(GetViewFileType(base::FilePath("file.tini")), ViewFileType::kTini);
  EXPECT_EQ(GetViewFileType(base::FilePath("file.xyz")), ViewFileType::kOther);
  EXPECT_EQ(GetViewFileType(base::FilePath("other")), ViewFileType::kOther);
}

}  // namespace file_manager::file_tasks
