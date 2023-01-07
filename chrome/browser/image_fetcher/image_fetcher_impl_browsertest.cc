// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using image_fetcher::ImageFetcher;
using image_fetcher::ImageFetcherImpl;

namespace {

const char kImageFetcherUmaClientName[] = "TestClientName";
const char kTestImagePath[] = "/image_decoding/droids.png";
const char kInvalidImagePath[] = "/DOESNOTEXIST";

}  // namespace

class ImageFetcherImplBrowserTest : public InProcessBrowserTest {
 protected:
  ImageFetcherImplBrowserTest()
      : num_callback_valid_called_(0),
        num_callback_null_called_(0),
        num_data_callback_valid_called_(0),
        num_data_callback_null_called_(0) {
    test_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  ImageFetcherImplBrowserTest(const ImageFetcherImplBrowserTest&) = delete;
  ImageFetcherImplBrowserTest& operator=(const ImageFetcherImplBrowserTest&) =
      delete;

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(test_server_.Start());
  }

  ImageFetcher* CreateImageFetcher() {
    ImageFetcher* fetcher =
        new ImageFetcherImpl(std::make_unique<ImageDecoderImpl>(),
                             browser()
                                 ->profile()
                                 ->GetDefaultStoragePartition()
                                 ->GetURLLoaderFactoryForBrowserProcess());
    return fetcher;
  }

  void OnImageAvailable(base::RunLoop* loop,
                        const gfx::Image& image,
                        const image_fetcher::RequestMetadata& metadata) {
    if (!image.IsEmpty()) {
      num_callback_valid_called_++;
    } else {
      num_callback_null_called_++;
    }
    loop->Quit();
  }

  void OnImageDataAvailable(const std::string& image_data,
                            const image_fetcher::RequestMetadata& metadata) {
    if (!image_data.empty()) {
      num_data_callback_valid_called_++;
    } else {
      num_data_callback_null_called_++;
    }
  }

  void FetchImageAndDataHelper(const GURL& image_url) {
    std::unique_ptr<ImageFetcher> image_fetcher_(CreateImageFetcher());
    image_fetcher::ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS,
                                             kImageFetcherUmaClientName);

    base::RunLoop run_loop;
    image_fetcher_->FetchImageAndData(
        image_url,
        base::BindOnce(&ImageFetcherImplBrowserTest::OnImageDataAvailable,
                       base::Unretained(this)),
        base::BindOnce(&ImageFetcherImplBrowserTest::OnImageAvailable,
                       base::Unretained(this), &run_loop),
        std::move(params));
    run_loop.Run();
  }

  int num_callback_valid_called_;
  int num_callback_null_called_;

  int num_data_callback_valid_called_;
  int num_data_callback_null_called_;

  net::EmbeddedTestServer test_server_;
};

IN_PROC_BROWSER_TEST_F(ImageFetcherImplBrowserTest, NormalFetch) {
  GURL image_url(test_server_.GetURL(kTestImagePath).spec());
  FetchImageAndDataHelper(image_url);

  EXPECT_EQ(1, num_callback_valid_called_);
  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(1, num_data_callback_valid_called_);
  EXPECT_EQ(0, num_data_callback_null_called_);
}

IN_PROC_BROWSER_TEST_F(ImageFetcherImplBrowserTest, MultipleFetch) {
  GURL image_url(test_server_.GetURL(kTestImagePath).spec());

  for (int i = 0; i < 5; i++) {
    FetchImageAndDataHelper(image_url);
  }

  EXPECT_EQ(5, num_callback_valid_called_);
  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(5, num_data_callback_valid_called_);
  EXPECT_EQ(0, num_data_callback_null_called_);
}

IN_PROC_BROWSER_TEST_F(ImageFetcherImplBrowserTest, InvalidFetch) {
  GURL invalid_image_url(test_server_.GetURL(kInvalidImagePath).spec());
  FetchImageAndDataHelper(invalid_image_url);

  EXPECT_EQ(0, num_callback_valid_called_);
  EXPECT_EQ(1, num_callback_null_called_);
  EXPECT_EQ(0, num_data_callback_valid_called_);
  EXPECT_EQ(1, num_data_callback_null_called_);
}
