// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_item_request.h"

#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::Return;
using ::testing::ReturnRef;

class DownloadItemRequestTest : public ::testing::Test {
 public:
  DownloadItemRequestTest()
      : item_(),
        request_(&item_, /*read_immediately=*/false, base::DoNothing()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    download_path_ = temp_dir_.GetPath().AppendASCII("download_location");
    download_temporary_path_ =
        temp_dir_.GetPath().AppendASCII("temporary_location");

    base::File file(download_path_,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());

    download_contents_ = "download contents";
    file.Write(0, download_contents_.c_str(), download_contents_.size());
    file.Close();

    ON_CALL(item_, GetTotalBytes())
        .WillByDefault(Return(download_contents_.size()));
    ON_CALL(item_, GetTargetFilePath())
        .WillByDefault(ReturnRef(download_path_));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  download::MockDownloadItem item_;
  DownloadItemRequest request_;
  base::ScopedTempDir temp_dir_;
  base::FilePath download_path_;
  base::FilePath download_temporary_path_;
  std::string download_contents_;
};

TEST_F(DownloadItemRequestTest, GetsContentsWaitsUntilRename) {
  ON_CALL(item_, GetFullPath())
      .WillByDefault(ReturnRef(download_temporary_path_));

  std::string download_contents = "";
  request_.GetRequestData(base::BindOnce(
      [](std::string* target_contents, BinaryUploadService::Result result,
         const BinaryUploadService::Request::Data& data) {
        *target_contents = data.contents;
      },
      &download_contents));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(download_contents, "");

  ON_CALL(item_, GetFullPath()).WillByDefault(ReturnRef(download_path_));
  item_.NotifyObserversDownloadUpdated();

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(download_contents, "download contents");
}

}  // namespace safe_browsing
