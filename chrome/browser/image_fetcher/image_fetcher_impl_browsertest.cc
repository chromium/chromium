// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
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
                                 ->GetProfile()
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

IN_PROC_BROWSER_TEST_F(ImageFetcherImplBrowserTest, DestroyInCallback) {
  // Verify that destroying the ImageFetcher (and its owned ImageDecoderImpl)
  // synchronously inside the image completion callback is safe.
  std::unique_ptr<ImageFetcher> image_fetcher(CreateImageFetcher());
  image_fetcher::ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS,
                                           kImageFetcherUmaClientName);
  GURL image_url(test_server_.GetURL(kTestImagePath).spec());

  base::RunLoop run_loop;
  image_fetcher->FetchImageAndData(
      image_url,
      base::BindLambdaForTesting(
          [&](const std::string& image_data,
              const image_fetcher::RequestMetadata& metadata) {
            if (!image_data.empty()) {
              num_data_callback_valid_called_++;
            }
          }),
      base::BindLambdaForTesting(
          [&](const gfx::Image& image,
              const image_fetcher::RequestMetadata& metadata) {
            if (!image.IsEmpty()) {
              num_callback_valid_called_++;
            }
            image_fetcher.reset();
            run_loop.Quit();
          }),
      std::move(params));
  run_loop.Run();

  EXPECT_FALSE(image_fetcher);
  EXPECT_EQ(1, num_callback_valid_called_);
  EXPECT_EQ(1, num_data_callback_valid_called_);

  // Also verify that destroying ImageDecoderImpl directly inside a failed
  // decode callback is safe while another decode request is pending.
  data_decoder::DataDecoder data_decoder;
  auto decoder = std::make_unique<ImageDecoderImpl>();
  base::RunLoop fail_loop;
  decoder->DecodeImage("invalid image data", gfx::Size(), &data_decoder,
                       base::BindLambdaForTesting([&](const gfx::Image& image) {
                         if (image.IsEmpty()) {
                           num_callback_null_called_++;
                         }
                         decoder.reset();
                         fail_loop.Quit();
                       }));
  decoder->DecodeImage(
      "other pending request", gfx::Size(), &data_decoder,
      base::BindOnce([](const gfx::Image& image) { ADD_FAILURE(); }));
  fail_loop.Run();

  EXPECT_FALSE(decoder);
  EXPECT_EQ(1, num_callback_null_called_);
}
