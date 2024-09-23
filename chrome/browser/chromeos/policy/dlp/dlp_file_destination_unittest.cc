// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"

#include "components/enterprise/data_controls/core/browser/component.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

constexpr char kUrl1[] = "https://1.example.com";
constexpr char kUrl2[] = "https://2.example.com";

class DlpFileDestinationTest : public testing::Test {
 public:
  DlpFileDestinationTest() = default;
  DlpFileDestinationTest(const DlpFileDestinationTest&) = delete;
  DlpFileDestinationTest& operator=(const DlpFileDestinationTest&) = delete;
  ~DlpFileDestinationTest() override = default;
};

TEST_F(DlpFileDestinationTest, ComparisonWithDifferentURLs) {
  GURL gurl;
  DlpFileDestination file1 = DlpFileDestination(GURL(kUrl1));
  DlpFileDestination file2 = DlpFileDestination(GURL(kUrl2));

  EXPECT_TRUE(file1 != file2);
  EXPECT_TRUE(file1 < file2);
  EXPECT_TRUE(file1 <= file2);
  EXPECT_FALSE(file1 > file2);
  EXPECT_FALSE(file1 >= file2);
}

TEST_F(DlpFileDestinationTest, ComparisonWithURLAndComponent) {
  GURL gurl;
  DlpFileDestination file1 = DlpFileDestination(GURL(kUrl1));
  DlpFileDestination file2 = DlpFileDestination(data_controls::Component::kArc);

  EXPECT_FALSE(file1 == file2);
  EXPECT_TRUE(file1 > file2);
}

TEST_F(DlpFileDestinationTest, ComparisonWithEmptyDestination) {
  GURL gurl;
  DlpFileDestination file1 = DlpFileDestination(GURL(kUrl1));
  DlpFileDestination file2 = DlpFileDestination();

  EXPECT_TRUE(file1 != file2);
  EXPECT_TRUE(file1 < file2);
}

TEST_F(DlpFileDestinationTest, ComparisonWithSameValues) {
  DlpFileDestination file1 = DlpFileDestination(GURL(kUrl1));
  DlpFileDestination file2 = DlpFileDestination(GURL(kUrl1));

  EXPECT_TRUE(file1 == file2);
  EXPECT_FALSE(file1 != file2);

  file1 = DlpFileDestination(data_controls::Component::kDrive);
  file2 = DlpFileDestination(data_controls::Component::kDrive);

  EXPECT_TRUE(file1 == file2);
  EXPECT_FALSE(file1 != file2);
}

TEST_F(DlpFileDestinationTest, ComparisonAfterAssignement) {
  DlpFileDestination file1 =
      DlpFileDestination(data_controls::Component::kDrive);
  DlpFileDestination file2 =
      DlpFileDestination(data_controls::Component::kOneDrive);
  EXPECT_FALSE(file1 == file2);

  file2 = file1;
  EXPECT_TRUE(file1 == file2);
}

}  // namespace policy
