// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_image_downloader.h"
#include "build/build_config.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {
constexpr char kImageUrl1[] = "http://example.com/image1.jpg";
constexpr char kImageUrl2[] = "http://example.com/image2.jpg";
constexpr char kImageUrl3[] = "http://example.com/image3.jpg";
constexpr char kImageFileName1[] = "file1";
constexpr char kImageFileName2[] = "file2";
constexpr char kImageFileName3[] = "file3";
constexpr char kFileContents[] = "file contents";
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
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    screensaver_image_downloader_ =
        std::make_unique<ScreensaverImageDownloader>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_),
            temp_dir());
  }

  void DeleteTempFolder() { EXPECT_TRUE(temp_dir_.Delete()); }

  const base::FilePath& temp_dir() { return temp_dir_.GetPath(); }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  void VerifyDownloadingQueueSize(size_t expected_size) const {
    EXPECT_EQ(expected_size,
              screensaver_image_downloader_->downloading_queue_.size());
  }

  std::unique_ptr<DownloadResultFuture> QueueNewJobWithFuture(
      const std::string& url,
      const std::string& file_name) {
    std::unique_ptr<DownloadResultFuture> future_callback =
        std::make_unique<DownloadResultFuture>();
    auto job = std::make_unique<ScreensaverImageDownloader::Job>(
        url, file_name, future_callback->GetCallback());
    screensaver_image_downloader_->QueueDownloadJob(std::move(job));

    return future_callback;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  network::TestURLLoaderFactory url_loader_factory_;

  // Class under test
  std::unique_ptr<ScreensaverImageDownloader> screensaver_image_downloader_;
};

TEST_F(ScreensaverImageDownloaderTest, DownloadImagesTest) {
  // Test successful download.
  {
    url_loader_factory()->AddResponse(kImageUrl1, kFileContents);

    std::unique_ptr<DownloadResultFuture> result_future =
        QueueNewJobWithFuture(kImageUrl1, kImageFileName1);
    EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess,
              result_future->Get<0>());
    ASSERT_TRUE(result_future->Get<1>().has_value());
    EXPECT_EQ(temp_dir().AppendASCII(kImageFileName1), result_future->Get<1>());
  }

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
        QueueNewJobWithFuture(kImageUrl2, kImageFileName2);
    EXPECT_EQ(ScreensaverImageDownloadResult::kNetworkError,
              result_future->Get<0>());
    EXPECT_FALSE(result_future->Get<1>().has_value());
  }

  // Test a file save error result by deleting the destination folder before the
  // URL request is solved.
  {
    std::unique_ptr<DownloadResultFuture> result_future =
        QueueNewJobWithFuture(kImageUrl3, kImageFileName3);

    // Wait until the request have been made to delete the tmp folder
    url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          ASSERT_TRUE(request.url.is_valid());
          EXPECT_EQ(kImageUrl3, request.url);

          DeleteTempFolder();
          url_loader_factory()->AddResponse(kImageUrl3, kFileContents);
        }));
    EXPECT_EQ(ScreensaverImageDownloadResult::kFileSaveError,
              result_future->Get<0>());
    EXPECT_FALSE(result_future->Get<1>().has_value());
  }
}

TEST_F(ScreensaverImageDownloaderTest, VerifySerializedDownloadTest) {
  // Push two jobs and check the internal downloading queue
  std::unique_ptr<DownloadResultFuture> result_future1 =
      QueueNewJobWithFuture(kImageUrl1, kImageFileName1);
  std::unique_ptr<DownloadResultFuture> result_future2 =
      QueueNewJobWithFuture(kImageUrl2, kImageFileName2);

  // First job should be executing and expecting the URL response, verify that
  // the second job is in the queue
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(1u);

  // Resolve the first job
  url_loader_factory()->AddResponse(kImageUrl1, kFileContents);
  EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess, result_future1->Get<0>());
  ASSERT_TRUE(result_future1->Get<1>().has_value());
  EXPECT_EQ(temp_dir().AppendASCII(kImageFileName1), result_future1->Get<1>());

  // First job has been resolved, second job should be executing and expecting
  // the URL response.
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);

  // Queue a third job while the second job is still waiting
  std::unique_ptr<DownloadResultFuture> result_future3 =
      QueueNewJobWithFuture(kImageUrl3, kImageFileName3);

  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(1u);

  // Resolve the second job
  url_loader_factory()->AddResponse(kImageUrl2, kFileContents);
  EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess, result_future2->Get<0>());
  ASSERT_TRUE(result_future2->Get<1>().has_value());
  EXPECT_EQ(temp_dir().AppendASCII(kImageFileName2), result_future2->Get<1>());

  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);

  // Resolve the third job
  url_loader_factory()->AddResponse(kImageUrl3, kFileContents);
  EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess, result_future3->Get<0>());
  ASSERT_TRUE(result_future3->Get<1>().has_value());
  EXPECT_EQ(temp_dir().AppendASCII(kImageFileName3), result_future3->Get<1>());

  // Ensure that the queue remains empty
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);
}

}  // namespace policy
