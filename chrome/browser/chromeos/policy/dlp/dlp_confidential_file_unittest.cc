// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"

#include <sstream>
#include <string>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

constexpr char kFilePath1[] = "test1.txt";
constexpr char kFilePath2[] = "test2.txt";

class DlpConfidentialFileTest : public testing::Test {
 public:
  DlpConfidentialFileTest() = default;
  DlpConfidentialFileTest(const DlpConfidentialFileTest&) = delete;
  DlpConfidentialFileTest& operator=(const DlpConfidentialFileTest&) = delete;
  ~DlpConfidentialFileTest() override = default;
};

TEST_F(DlpConfidentialFileTest, ComparisonWithDifferentPaths) {
  DlpConfidentialFile file1 = DlpConfidentialFile(base::FilePath(kFilePath1));
  DlpConfidentialFile file2 = DlpConfidentialFile(base::FilePath(kFilePath2));

  EXPECT_TRUE(file1 != file2);
  EXPECT_TRUE(file1 < file2);
  EXPECT_TRUE(file1 <= file2);
  EXPECT_FALSE(file1 > file2);
  EXPECT_FALSE(file1 >= file2);
}

TEST_F(DlpConfidentialFileTest, ComparisonWithSamePaths) {
  DlpConfidentialFile file1 = DlpConfidentialFile(base::FilePath(kFilePath1));
  DlpConfidentialFile file2 = DlpConfidentialFile(base::FilePath(kFilePath1));

  EXPECT_TRUE(file1 == file2);
  EXPECT_FALSE(file1 != file2);
}

TEST_F(DlpConfidentialFileTest, ComparisonAfterAssignement) {
  DlpConfidentialFile file1 = DlpConfidentialFile(base::FilePath(kFilePath1));
  DlpConfidentialFile file2 = DlpConfidentialFile(base::FilePath(kFilePath2));
  EXPECT_FALSE(file1 == file2);

  file2 = file1;
  EXPECT_TRUE(file1 == file2);
}

}  // namespace policy
