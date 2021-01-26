// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for FileSystemDownloadController.

#include "chrome/browser/enterprise/connectors/file_system/download_controller.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace enterprise_connectors {

class FileSystemDownloadControllerTest : public testing::Test {
 public:
  FileSystemDownloadControllerTest()
      : controller_(item_),
        url_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    controller_.Init(
        base::BindRepeating(&FileSystemDownloadControllerTest::AuthenRetry,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&FileSystemDownloadControllerTest::DownloadComplete,
                       weak_factory_.GetWeakPtr()));
  }

  void AddFetchResult(const std::string& url,
                      net::HttpStatusCode response_code,
                      const std::string& body) {
    test_url_loader_factory_.AddResponse(url, body, response_code);
  }

  void AuthenRetry() { ++authentication_retry_; }
  void DownloadComplete(bool success) {
    download_callback_called_ = true;
    upload_success_ = success;
  }

 protected:
  download::DownloadItem* item_{nullptr};
  FileSystemDownloadController controller_;

  int authentication_retry_{0};
  bool download_callback_called_{false};
  bool upload_success_{false};

  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder decoder_;
  // Decoder can only be declared after SingleThreadTaskEnvironment.

  // For controller.TryTask().
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_factory_;
  base::WeakPtrFactory<FileSystemDownloadControllerTest> weak_factory_{this};
};

TEST_F(FileSystemDownloadControllerTest, HasExistingFolder) {
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);

  controller_.TryTask(url_factory_, "dummytoken");
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
  // Until entire upload flow is implemented to notify success in final step.
  EXPECT_EQ(controller_.GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
}

TEST_F(FileSystemDownloadControllerTest, NoExistingFolder) {
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_CREATED,
                 kFileSystemBoxCreateFolderResponseBody);

  controller_.TryTask(url_factory_, "dummytoken");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
  // Until entire upload flow is implemented to notify success in final step.
  EXPECT_EQ(controller_.GetFolderIdForTesting(),
            kFileSystemBoxCreateFolderResponseFolderId);
}

TEST_F(FileSystemDownloadControllerTest, AuthenticationFailureInTryTask) {
  // Check that authentication retry callback is called upon
  // net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_UNAUTHORIZED,
                 std::string());
  controller_.TryTask(url_factory_, "dummytoken");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(authentication_retry_, 1);

  // Should be retrying authentication, no report via callback yet.
  EXPECT_FALSE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_EQ(controller_.GetFolderIdForTesting(), "");

  // Check that it's able to continue after authentication has been refreshed.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);
  controller_.TryTask(url_factory_, "dummytoken");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
  // Until entire upload flow is implemented to notify success in final step.
  EXPECT_EQ(controller_.GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
}

TEST_F(FileSystemDownloadControllerTest, UnexpectedFailureInTryTask) {
  // Check that authentication retry callback is NOT called upon
  // any other failure code other than net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_NOT_FOUND,
                 std::string());
  controller_.TryTask(url_factory_, "dummytoken");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(authentication_retry_, 0);

  // Should just report failure via callback.
  EXPECT_TRUE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_EQ(controller_.GetFolderIdForTesting(), "");
}

}  // namespace enterprise_connectors
