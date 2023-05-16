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
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {
constexpr char kImageUrl1[] = "https://example.com/image1.jpg";
constexpr char kImageUrl2[] = "https://example.com/image2.jpg";
constexpr char kImageUrl3[] = "https://example.com/image3.jpg";
constexpr char kFileContents[] = "file contents";
constexpr char kCacheFileExt[] = ".cache";

constexpr char kTestDownloadFolder[] = "test_download_folder";

}  // namespace

class ScreensaverImageDownloaderTest : public testing::Test {
 public:
  using ImageListUpdatedFuture =
      base::test::RepeatingTestFuture<const std::vector<base::FilePath>&>;

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
            test_download_folder_, image_list_updated_future_.GetCallback());
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

  void QueueNewImageDownload(const std::string& url) {
    auto job = std::make_unique<ScreensaverImageDownloader::Job>(url);
    screensaver_image_downloader_->QueueDownloadJob(std::move(job));
  }

  base::FilePath GetExpectedFilePath(const std::string url) {
    const std::string hash = base::SHA1HashString(url);
    const std::string encoded_hash = base::HexEncode(hash.data(), hash.size());
    return test_download_folder_.AppendASCII(encoded_hash + kCacheFileExt);
  }

  void VerifySucessfulImageRequest(
      const std::vector<std::pair<base::FilePath, std::string>>&
          expected_images) {
    ASSERT_TRUE(image_list_updated_future_.Wait())
        << "Callback expected to be called.";

    const std::vector<base::FilePath> image_list =
        image_list_updated_future_.Take();
    ASSERT_EQ(expected_images.size(), image_list.size());

    for (const auto& [path, file_content] : expected_images) {
      bool found = std::find(image_list.begin(), image_list.end(), path) !=
                   image_list.end();
      ASSERT_TRUE(found);
      ASSERT_TRUE(base::PathExists(path));

      std::string actual_file_contents;
      EXPECT_TRUE(base::ReadFileToString(path, &actual_file_contents));
      EXPECT_EQ(file_content, actual_file_contents);
    }
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir tmp_dir_;
  base::FilePath test_download_folder_;
  network::TestURLLoaderFactory url_loader_factory_;
  ImageListUpdatedFuture image_list_updated_future_;

  // Class under test
  std::unique_ptr<ScreensaverImageDownloader> screensaver_image_downloader_;
};

TEST_F(ScreensaverImageDownloaderTest, DownloadImagesTest) {
  // Setup the fake URL responses:
  //   * kImageUrl1 returns a valid response.
  //   * kImageUrl2 returns a 404 error.
  //   * kImageUrl3 deletes the download dir before returning a valid response.
  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url.is_valid());
        if (request.url == kImageUrl1) {
          url_loader_factory()->AddResponse(kImageUrl1, kFileContents);
        }
        if (request.url == kImageUrl2) {
          auto response_head = network::mojom::URLResponseHead::New();
          response_head->headers =
              base::MakeRefCounted<net::HttpResponseHeaders>("");
          response_head->headers->SetHeader("Content-Type", "image/jpg");
          response_head->headers->ReplaceStatusLine("HTTP/1.1 404 Not found");
          url_loader_factory()->AddResponse(
              GURL(kImageUrl2), std::move(response_head), std::string(),
              network::URLLoaderCompletionStatus(net::OK));
        }
        if (request.url == kImageUrl3) {
          DeleteTestDownloadFolder();
          url_loader_factory()->AddResponse(kImageUrl3, kFileContents);
        }
      }));

  // Test successful download.
  std::vector<std::pair<base::FilePath, std::string>> expected_images;
  expected_images.emplace_back(GetExpectedFilePath(kImageUrl1),
                               std::string(kFileContents));

  QueueNewImageDownload(kImageUrl1);
  VerifySucessfulImageRequest(expected_images);

  // Queue the request that should not download any file.
  QueueNewImageDownload(kImageUrl2);
  QueueNewImageDownload(kImageUrl3);

  // Verify that the downloader did not create image files for the error
  // downloads.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(GetExpectedFilePath(kImageUrl2)));
  EXPECT_FALSE(base::PathExists(GetExpectedFilePath(kImageUrl3)));
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
  std::vector<std::pair<base::FilePath, std::string>> expected_images;
  expected_images.emplace_back(GetExpectedFilePath(kImageUrl1),
                               std::string(kFileContents));
  QueueNewImageDownload(kImageUrl1);
  VerifySucessfulImageRequest(expected_images);
  EXPECT_EQ(1u, urls_requested);

  // Attempting to download the same URL should not create a new network
  // request.
  QueueNewImageDownload(kImageUrl1);
  VerifySucessfulImageRequest(expected_images);
  EXPECT_EQ(1u, urls_requested);

  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ++urls_requested;
        url_loader_factory()->AddResponse(kImageUrl2, kFileContents);
      }));

  // A different URL should create a new network request.
  expected_images.emplace_back(GetExpectedFilePath(kImageUrl2),
                               std::string(kFileContents));
  QueueNewImageDownload(kImageUrl2);
  VerifySucessfulImageRequest(expected_images);
  EXPECT_EQ(2u, urls_requested);
}

TEST_F(ScreensaverImageDownloaderTest, VerifySerializedDownloadTest) {
  // Push two jobs and check the internal downloading queue
  QueueNewImageDownload(kImageUrl1);
  QueueNewImageDownload(kImageUrl2);

  // First job should be executing and expecting the URL response, verify that
  // the second job is in the queue
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(1u);

  // Resolve the first job
  url_loader_factory()->AddResponse(kImageUrl1, kFileContents);

  std::vector<std::pair<base::FilePath, std::string>> expected_images;
  expected_images.emplace_back(GetExpectedFilePath(kImageUrl1),
                               std::string(kFileContents));
  VerifySucessfulImageRequest(expected_images);

  // First job has been resolved, second job should be executing and expecting
  // the URL response.
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);

  // Queue a third job while the second job is still waiting
  QueueNewImageDownload(kImageUrl3);

  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(1u);

  // Resolve the second job
  url_loader_factory()->AddResponse(kImageUrl2, kFileContents);

  expected_images.emplace_back(GetExpectedFilePath(kImageUrl2),
                               std::string(kFileContents));
  VerifySucessfulImageRequest(expected_images);

  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);

  // Resolve the third job
  url_loader_factory()->AddResponse(kImageUrl3, kFileContents);

  expected_images.emplace_back(GetExpectedFilePath(kImageUrl3),
                               std::string(kFileContents));
  VerifySucessfulImageRequest(expected_images);

  // Ensure that the queue remains empty
  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(0u);
}

TEST_F(ScreensaverImageDownloaderTest,
       DeleteDownloadedImagesWhenEmptyListIsPassedTest) {
  // Download two images to attempt clearing later.
  url_loader_factory()->AddResponse(kImageUrl1, kFileContents);
  url_loader_factory()->AddResponse(kImageUrl2, kFileContents);

  std::vector<std::pair<base::FilePath, std::string>> expected_images;
  expected_images.emplace_back(GetExpectedFilePath(kImageUrl1),
                               std::string(kFileContents));
  QueueNewImageDownload(kImageUrl1);
  VerifySucessfulImageRequest(expected_images);

  expected_images.emplace_back(GetExpectedFilePath(kImageUrl2),
                               std::string(kFileContents));
  QueueNewImageDownload(kImageUrl2);
  VerifySucessfulImageRequest(expected_images);

  // Verify that images saved into disk are deleted properly.
  screensaver_image_downloader()->UpdateImageUrlList(base::Value::List());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(test_download_folder()));
}

TEST_F(ScreensaverImageDownloaderTest,
       ClearRequestQueueWhenEmptyListIsPassedTest) {
  // Queue 3 download request, the first one one will be waiting for the URL
  // response, the latter will be queued.
  QueueNewImageDownload(kImageUrl1);
  QueueNewImageDownload(kImageUrl2);
  QueueNewImageDownload(kImageUrl3);

  base::RunLoop().RunUntilIdle();
  VerifyDownloadingQueueSize(2u);

  // Simulate a new policy update that clears the queue.
  screensaver_image_downloader()->UpdateImageUrlList(base::Value::List());

  // Resolve the request for the first image, the image should not be saved to
  // file.
  url_loader_factory()->AddResponse(kImageUrl1, kFileContents);

  // Verify that the downloader did not create image files for the cancelled
  // downloads.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(GetExpectedFilePath(kImageUrl1)));
  EXPECT_FALSE(base::PathExists(GetExpectedFilePath(kImageUrl2)));
  EXPECT_FALSE(base::PathExists(GetExpectedFilePath(kImageUrl3)));
}

TEST_F(ScreensaverImageDownloaderTest, ClearImagesAfterUpdateTest) {
  {
    // Add two image to the policy list and confirm that are indeed downloaded.
    base::Value::List image_urls;
    image_urls.Append(kImageUrl1);
    screensaver_image_downloader()->UpdateImageUrlList(image_urls);

    url_loader_factory()->AddResponse(kImageUrl1, kFileContents);

    std::vector<std::pair<base::FilePath, std::string>> expected_images;
    expected_images.emplace_back(GetExpectedFilePath(kImageUrl1),
                                 std::string(kFileContents));
    VerifySucessfulImageRequest(expected_images);

    image_urls.Append(kImageUrl2);
    screensaver_image_downloader()->UpdateImageUrlList(image_urls);

    url_loader_factory()->AddResponse(kImageUrl2, kFileContents);

    VerifySucessfulImageRequest(expected_images);
    expected_images.emplace_back(GetExpectedFilePath(kImageUrl2),
                                 std::string(kFileContents));
    VerifySucessfulImageRequest(expected_images);
  }

  {
    // Case: Verify that when the first file is removed from policy image list
    // and only the second file remains, the first file is indeed cleaned-up
    // from the disk and the second file is still present on the disk.
    base::Value::List image_urls;
    image_urls.Append(kImageUrl2);
    screensaver_image_downloader()->UpdateImageUrlList(image_urls);

    // Verify the update callback from clearing the first image.
    std::vector<std::pair<base::FilePath, std::string>> expected_images;
    expected_images.emplace_back(GetExpectedFilePath(kImageUrl2),
                                 std::string(kFileContents));
    VerifySucessfulImageRequest(expected_images);

    // Expect another callback from the second image found in cache.
    VerifySucessfulImageRequest(expected_images);

    // Verify files in disk.
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(base::PathExists(GetExpectedFilePath(kImageUrl1)));
    EXPECT_TRUE(base::PathExists(GetExpectedFilePath(kImageUrl2)));
  }

  {
    // Case: Verify that old unreferenced files are cleaned up from the disk.
    // This can happen if the chromebook restarted, but the policy was updated
    // while the chromebook was offline and it only receives the new policy
    // values and is unaware of the old policy values.
    std::string filename = "test";
    base::FilePath orphan_cache_file =
        test_download_folder().AppendASCII(filename + kCacheFileExt);
    base::WriteFile(orphan_cache_file, "test_data");
    EXPECT_TRUE(base::PathExists(orphan_cache_file));

    base::Value::List image_urls;
    image_urls.Append(kImageUrl2);
    screensaver_image_downloader()->UpdateImageUrlList(image_urls);
    VerifySucessfulImageRequest(
        {{GetExpectedFilePath(kImageUrl2), std::string(kFileContents)}});
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(base::PathExists(GetExpectedFilePath(kImageUrl2)));
    // Confirm that after the update the orphan file was successfully cleaned
    // up.
    EXPECT_FALSE(base::PathExists(orphan_cache_file));
  }
}

}  // namespace ash
