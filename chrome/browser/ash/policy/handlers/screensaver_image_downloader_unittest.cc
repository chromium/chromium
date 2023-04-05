// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_image_downloader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {
constexpr char kImageUrl1[] = "http://example.com/image1.jpg";
constexpr char kImageUrl2[] = "http://example.com/image2.jpg";
constexpr char kImageUrl3[] = "http://example.com/image3.jpg";
constexpr char kImageFileName[] = "file";
constexpr char kFileContents[] = "file contents";
}  // namespace

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

  ScreensaverImageDownloader* screensaver_image_downloader() {
    return screensaver_image_downloader_.get();
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  network::TestURLLoaderFactory url_loader_factory_;

  // Class under test
  std::unique_ptr<ScreensaverImageDownloader> screensaver_image_downloader_;
};

TEST_F(ScreensaverImageDownloaderTest, DownloadImagesTest) {
  // Test successful download.
  using Job = ScreensaverImageDownloader::Job;
  {
    url_loader_factory()->AddResponse(kImageUrl1, kFileContents);

    base::test::TestFuture<ScreensaverImageDownloadResult,
                           absl::optional<base::FilePath>>
        download_completed_cb;
    auto job = std::make_unique<Job>(kImageUrl1, kImageFileName,
                                     download_completed_cb.GetCallback());
    screensaver_image_downloader()->QueueDownloadJob(std::move(job));
    EXPECT_EQ(ScreensaverImageDownloadResult::kSuccess,
              download_completed_cb.Get<0>());
    ASSERT_TRUE(download_completed_cb.Get<1>().has_value());
    EXPECT_EQ(temp_dir().AppendASCII(kImageFileName),
              download_completed_cb.Get<1>());
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

    base::test::TestFuture<ScreensaverImageDownloadResult,
                           absl::optional<base::FilePath>>
        download_completed_cb;
    auto job = std::make_unique<Job>(kImageUrl2, kImageFileName,
                                     download_completed_cb.GetCallback());
    screensaver_image_downloader()->QueueDownloadJob(std::move(job));

    EXPECT_EQ(ScreensaverImageDownloadResult::kNetworkError,
              download_completed_cb.Get<0>());
    EXPECT_FALSE(download_completed_cb.Get<1>().has_value());
  }

  // Test a file save error result by deleting the destination folder before the
  // URL request is solved.
  {
    base::test::TestFuture<ScreensaverImageDownloadResult,
                           absl::optional<base::FilePath>>
        download_completed_cb;
    auto job = std::make_unique<Job>(kImageUrl3, kImageFileName,
                                     download_completed_cb.GetCallback());
    screensaver_image_downloader()->QueueDownloadJob(std::move(job));

    // Wait until the request have been made to delete the tmp folder
    EXPECT_TRUE(url_loader_factory()->IsPending(kImageUrl3));
    DeleteTempFolder();
    url_loader_factory()->AddResponse(kImageUrl3, kFileContents);

    EXPECT_EQ(ScreensaverImageDownloadResult::kFileSaveError,
              download_completed_cb.Get<0>());
    EXPECT_FALSE(download_completed_cb.Get<1>().has_value());
  }
}

}  // namespace policy
