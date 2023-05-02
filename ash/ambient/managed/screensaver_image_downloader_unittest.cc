// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/managed/screensaver_image_downloader.h"
#include "build/build_config.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/sha1.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

constexpr char kImageUrl1[] = "http://example.com/image1.jpg";
constexpr char kImageUrl2[] = "http://example.com/image2.jpg";
constexpr char kImageUrl3[] = "http://example.com/image3.jpg";
constexpr char kFileContents[] = "file contents";
constexpr char kCacheFileExt[] = ".cache";

constexpr char kTestDownloadFolder[] = "test_download_folder";

}  // namespace

using DownloadResultFuture =
    base::test::TestFuture<ScreensaverImageDownloadResult,
                           absl::optional<base::FilePath>>;

class ScreensaverImageDownloaderTest : public testing::Test {
 public:
  ScreensaverImageDownloaderTest() = default;

  ScreensaverImageDownloaderTest(const ScreensaverImageDownloaderTest&) =
      delete;
  ScreensaverImageDownloaderTest& operator=(
      const ScreensaverImageDownloaderTest&) = delete;

  ~ScreensaverImageDownloaderTest() override = default;

  // testing::Test:
  void SetUp() override {
    EXPECT_TRUE(tmp_dir_.CreateUniqueTempDir());
    test_download_folder_ = tmp_dir_.GetPath().AppendASCII(kTestDownloadFolder);

    screensaver_image_downloader_ =
        std::make_unique<ScreensaverImageDownloader>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_),
            test_download_folder_);
  }

  ScreensaverImageDownloader* screensaver_image_downloader() {
    return screensaver_image_downloader_.get();
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  const base::FilePath& test_download_folder() { return test_download_folder_; }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void DeleteTestDownloadFolder() {
    EXPECT_TRUE(base::DeletePathRecursively(test_download_folder_));
  }

  void VerifyDownloadingQueueSize(size_t expected_size) const {
    EXPECT_EQ(expected_size,
              screensaver_image_downloader_->downloading_queue_.size());
  }

  std::unique_ptr<DownloadResultFuture> QueueNewJobWithFuture(
      const std::string& url) {
    std::unique_ptr<DownloadResultFuture> future_callback =
        std::make_unique<DownloadResultFuture>();
    auto job = std::make_unique<ScreensaverImageDownloader::Job>(
        url, future_callback->GetCallback());
    screensaver_image_downloader_->QueueDownloadJob(std::move(job));

    return future_callback;
  }

  base::FilePath GetExpectedFilePath(const std::string url) {
    const std::string hash = base::SHA1HashString(url);
    const std::string encoded_hash = base::HexEncode(hash.data(), hash.size());
    return test_download_folder_.AppendASCII(encoded_hash + kCacheFileExt);
  }

  void VerifySucessfulImageRequest(
      std::unique_ptr<DownloadResultFuture> result_future,
      const std::string& url,
      const std::string& file_contents) {
    ASSERT_TRUE(result_future.get());
    ASSERT_TRUE(result_future->Wait()) << "Callback expected to be called.";

    auto [result, optional_path] = result_future->Take();
    EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess, result);
    ASSERT_TRUE(optional_path.has_value());
    EXPECT_EQ(GetExpectedFilePath(url), *optional_path);

    ASSERT_TRUE(base::PathExists(*optional_path));
    std::string actual_file_contents;
    EXPECT_TRUE(base::ReadFileToString(*optional_path, &actual_file_contents));
    EXPECT_EQ(file_contents, actual_file_contents);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir tmp_dir_;
  base::FilePath test_download_folder_;
  network::TestURLLoaderFactory url_loader_factory_;

  // Class under test
  std::unique_ptr<ScreensaverImageDownloader> screensaver_image_downloader_;
};

TEST_F(ScreensaverImageDownloaderTest, DownloadImagesTest) {
  // Test successful download.
  url_loader_factory()->AddResponse(kImageUrl1, kFileContents);
  VerifySucessfulImageRequest(QueueNewJobWithFuture(kImageUrl1), kImageUrl1,
                              kFileContents);

  // Test download with a fake network error.
  {
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    response_head->headers->SetHeader("Content-Type", "image/jpg");
    response_head->headers->ReplaceStatusLine("HTTP/1.1 404 Not found");
    url_loader_factory()->AddResponse(
        GURL(kImageUrl2), std::move(response_head), std::string(),
        network::URLLoaderCompletionStatus(net::OK));

    std::unique_ptr<DownloadResultFuture> result_future =
        QueueNewJobWithFuture(kImageUrl2);
    EXPECT_EQ(ScreensaverImageDownloadResult::kNetworkError,
              result_future->Get<0>());
    EXPECT_FALSE(result_future->Get<1>().has_value());
  }

  // Test a file save error result by deleting the destination folder before the
  // URL request is solved.
  {
    std::unique_ptr<DownloadResultFuture> result_future =
        QueueNewJobWithFuture(kImageUrl3);

    // Wait until the request has been made to delete the tmp folder
    url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          ASSERT_TRUE(request.url.is_valid());
          EXPECT_EQ(kImageUrl3, request.url);

          DeleteTestDownloadFolder();
          url_loader_factory()->AddResponse(kImageUrl3, kFileContents);
        }));
    EXPECT_EQ(ScreensaverImageDownloadResult::kFileSaveError,
              result_future->Get<0>());
    EXPECT_FALSE(result_future->Get<1>().has_value());
  }
}

TEST_F(ScreensaverImageDownloaderTest, ReuseFilesInCacheTest) {
  // Track how many URL requests will be sent by the downloader
  size_t urls_requested = 0;
  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ++urls_requested;
        url_loader_factory()->AddResponse(kImageUrl1, kFileContents);
      }));

  // Test initial download.
  {
    std::unique_ptr<DownloadResultFuture> result_future =
        QueueNewJobWithFuture(kImageUrl1);
    EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess,
              result_future->Get<0>());
    ASSERT_TRUE(result_future->Get<1>().has_value());
    EXPECT_EQ(GetExpectedFilePath(kImageUrl1), result_future->Get<1>());
    EXPECT_EQ(1u, urls_requested);
  }

  // Attempting to download the same URL should not create a new network
  // request.
  {
    std::unique_ptr<DownloadResultFuture> result_future =
        QueueNewJobWithFuture(kImageUrl1);
    EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess,
              result_future->Get<0>());
    ASSERT_TRUE(result_future->Get<1>().has_value());
    EXPECT_EQ(GetExpectedFilePath(kImageUrl1), result_future->Get<1>());
    EXPECT_EQ(1u, urls_requested);
  }

  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ++urls_requested;
        url_loader_factory()->AddResponse(kImageUrl2, kFileContents);
      }));

  // A different URL should create a new network request.
  {
    std::unique_ptr<DownloadResultFuture> result_future =
        QueueNewJobWithFuture(kImageUrl2);
    EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess,
              result_future->Get<0>());
    ASSERT_TRUE(result_future->Get<1>().has_value());
    EXPECT_EQ(GetExpectedFilePath(kImageUrl2), result_future->Get<1>());
    EXPECT_EQ(2u, urls_requested);
  }
}

TEST_F(ScreensaverImageDownloaderTest, VerifySerializedDownloadTest) {
  // Push two jobs and check the internal downloading queue
  std::unique_ptr<DownloadResultFuture> result_future1 =
      QueueNewJobWithFuture(kImageUrl1);
  std::unique_ptr<DownloadResultFuture> result_future2 =
      QueueNewJobWithFuture(kImageUrl2);

  // First job should be executing and expecting the URL response, verify that
  // the second job is in the queue
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(1u);

  // Resolve the first job
  url_loader_factory()->AddResponse(kImageUrl1, kFileContents);
  VerifySucessfulImageRequest(std::move(result_future1), kImageUrl1,
                              kFileContents);

  // First job has been resolved, second job should be executing and expecting
  // the URL response.
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);

  // Queue a third job while the second job is still waiting
  std::unique_ptr<DownloadResultFuture> result_future3 =
      QueueNewJobWithFuture(kImageUrl3);

  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(1u);

  // Resolve the second job
  url_loader_factory()->AddResponse(kImageUrl2, kFileContents);
  VerifySucessfulImageRequest(std::move(result_future2), kImageUrl2,
                              kFileContents);

  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);

  // Resolve the third job
  url_loader_factory()->AddResponse(kImageUrl3, kFileContents);
  VerifySucessfulImageRequest(std::move(result_future3), kImageUrl3,
                              kFileContents);

  // Ensure that the queue remains empty
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);
}

TEST_F(ScreensaverImageDownloaderTest, DeleteDownloadedImagesTest) {
  // Download two images to attempt clearing later.
  url_loader_factory()->AddResponse(kImageUrl1, kFileContents);
  url_loader_factory()->AddResponse(kImageUrl2, kFileContents);
  VerifySucessfulImageRequest(QueueNewJobWithFuture(kImageUrl1), kImageUrl1,
                              kFileContents);
  VerifySucessfulImageRequest(QueueNewJobWithFuture(kImageUrl2), kImageUrl2,
                              kFileContents);

  // Verify that images saved into disk are deleted properly.
  screensaver_image_downloader()->DeleteDownloadedImages();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(test_download_folder()));
}

TEST_F(ScreensaverImageDownloaderTest, ClearRequestQueueTest) {
  // Queue 3 download request, the first one one will be executed, the latter
  // will be queued.
  std::unique_ptr<DownloadResultFuture> result_future1 =
      QueueNewJobWithFuture(kImageUrl1);
  std::unique_ptr<DownloadResultFuture> result_future2 =
      QueueNewJobWithFuture(kImageUrl2);
  std::unique_ptr<DownloadResultFuture> result_future3 =
      QueueNewJobWithFuture(kImageUrl3);

  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(2u);

  // Clear the queue and resolve the first request.
  url_loader_factory()->AddResponse(kImageUrl1, kFileContents);
  screensaver_image_downloader()->ClearRequestQueue();

  // Verify that the pending request was executed until completion.
  VerifySucessfulImageRequest(std::move(result_future1), kImageUrl1,
                              kFileContents);

  // Verify that the other requests were notified of them being cancelled.
  EXPECT_EQ(ScreensaverImageDownloadResult::kCancelled,
            result_future2->Get<0>());
  EXPECT_FALSE(result_future2->Get<1>().has_value());

  EXPECT_EQ(ScreensaverImageDownloadResult::kCancelled,
            result_future3->Get<0>());
  EXPECT_FALSE(result_future3->Get<1>().has_value());
}

}  // namespace ash
