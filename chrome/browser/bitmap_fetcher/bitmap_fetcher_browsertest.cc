// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/referrer_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

const bool kAsyncCall = true;
const bool kSyncCall = false;
const char kStartTestURL[] = "/this-should-work";
const char kOnImageDecodedTestURL[] = "/this-should-work-as-well";
const char kOnURLFetchFailureTestURL[] = "/this-should-be-fetch-failure";

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

// Class to catch events from the BitmapFetcher for testing.
class BitmapFetcherTestDelegate : public BitmapFetcherDelegate {
 public:
  explicit BitmapFetcherTestDelegate(bool async) : async_(async) {}

  BitmapFetcherTestDelegate(const BitmapFetcherTestDelegate&) = delete;
  BitmapFetcherTestDelegate& operator=(const BitmapFetcherTestDelegate&) =
      delete;

  ~BitmapFetcherTestDelegate() override { EXPECT_EQ(expect_called_, called_); }

  // Method inherited from BitmapFetcherDelegate.
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override {
    called_ = true;
    url_ = url;
    if (bitmap) {
      success_ = true;
      if (bitmap_.tryAllocPixels(bitmap->info())) {
        bitmap->readPixels(bitmap_.info(), bitmap_.getPixels(),
                           bitmap_.rowBytes(), 0, 0);
      }
    }
    // For async calls, we need to quit the run loop so the test can continue.
    if (async_)
      run_loop_.Quit();
  }

  // Waits until OnFetchComplete() is called. Should only be used for
  // async tests.
  void Wait() {
    ASSERT_TRUE(async_);
    run_loop_.Run();
  }

  GURL url() const { return url_; }
  bool success() const { return success_; }
  const SkBitmap& bitmap() const { return bitmap_; }

  void SetExpectNotToGetCalled() { expect_called_ = false; }

 private:
  base::RunLoop run_loop_;
  bool called_ = false;
  bool expect_called_ = true;
  GURL url_;
  bool success_ = false;
  const bool async_;
  SkBitmap bitmap_;
};

class BitmapFetcherBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Set up the test server.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &BitmapFetcherBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Tear down the test server.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  SkBitmap test_bitmap() const {
    // Return some realistic looking bitmap data
    SkBitmap image;

    // Put a real bitmap into "image".  2x2 bitmap of green 32 bit pixels.
    image.allocN32Pixels(2, 2);
    image.eraseColor(SK_ColorGREEN);
    return image;
  }

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    std::unique_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    if (request.relative_url == kStartTestURL) {
      // Encode the bits as a PNG.
      std::vector<unsigned char> compressed;
      gfx::PNGCodec::EncodeBGRASkBitmap(test_bitmap(), true, &compressed);
      // Copy the bits into a string and return them through the embedded
      // test server
      std::string image_string(compressed.begin(), compressed.end());
      response->set_code(net::HTTP_OK);
      response->set_content(image_string);
    } else if (request.relative_url == kOnImageDecodedTestURL) {
      response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    } else if (request.relative_url == kOnURLFetchFailureTestURL) {
      response->set_code(net::HTTP_OK);
      response->set_content(std::string("Not a real bitmap"));
    }
    return std::move(response);
  }
};

// WARNING:  These tests work with --single-process-tests, but not
// --single-process.  The reason is that the sandbox does not get created
// for us by the test process if --single-process is used.

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, StartTest) {
  GURL url = embedded_test_server()->GetURL(kStartTestURL);

  // Set up a delegate to wait for the callback.
  BitmapFetcherTestDelegate delegate(kAsyncCall);

  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  // We expect that the image decoder will get called and return
  // an image in a callback to OnImageDecoded().
  fetcher.Init(
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      network::mojom::CredentialsMode::kInclude);
  fetcher.Start(browser()
                    ->profile()
                    ->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  ASSERT_TRUE(delegate.success());

  // Make sure we get back the bitmap we expect.
  const SkBitmap& found_image = delegate.bitmap();
  EXPECT_TRUE(gfx::BitmapsAreEqual(test_bitmap(), found_image));
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, OnImageDecodedTest) {
  GURL url = embedded_test_server()->GetURL(kOnImageDecodedTestURL);
  SkBitmap image = test_bitmap();

  BitmapFetcherTestDelegate delegate(kSyncCall);

  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.OnImageDecoded(image);

  // Ensure image is marked as succeeded.
  EXPECT_TRUE(delegate.success());

  // Test that the image is what we expect.
  EXPECT_TRUE(gfx::BitmapsAreEqual(image, delegate.bitmap()));
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, OnURLFetchFailureTest) {
  GURL url = embedded_test_server()->GetURL(kOnURLFetchFailureTestURL);

  // We intentionally put no data into the bitmap to simulate a failure.

  // Set up a delegate to wait for the callback.
  BitmapFetcherTestDelegate delegate(kAsyncCall);

  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.Init(
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      network::mojom::CredentialsMode::kInclude);
  fetcher.Start(browser()
                    ->profile()
                    ->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  EXPECT_FALSE(delegate.success());
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, HandleImageFailedTest) {
  GURL url("http://example.com/this-should-be-a-decode-failure");
  BitmapFetcherTestDelegate delegate(kAsyncCall);
  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.Init(
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      network::mojom::CredentialsMode::kInclude);
  fetcher.Start(browser()
                    ->profile()
                    ->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  EXPECT_FALSE(delegate.success());
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, DataURLNonImage) {
  GURL url("data:,Hello");
  BitmapFetcherTestDelegate delegate(kAsyncCall);
  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.Init(
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      network::mojom::CredentialsMode::kInclude);
  fetcher.Start(browser()
                    ->profile()
                    ->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());
  delegate.Wait();

  EXPECT_FALSE(delegate.success());
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, DataURLImage) {
  // This is test_bitmap() in data: URL form.
  GURL url(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVQYlWNk+"
      "M/wn4GBgYGJAQoAHhgCAh6X4CYAAAAASUVORK5CYII=");

  BitmapFetcherTestDelegate delegate(kAsyncCall);
  BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.Init(
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      network::mojom::CredentialsMode::kInclude);
  fetcher.Start(browser()
                    ->profile()
                    ->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());
  delegate.Wait();

  // Ensure image is marked as succeeded.
  EXPECT_TRUE(delegate.success());
  EXPECT_TRUE(gfx::BitmapsAreEqual(test_bitmap(), delegate.bitmap()));
}

// Verifies that bitmap fetch callback gets canceled gracefully when the fetcher
// gets deleted.
IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest,
                       FetcherDeletedBeforeDataURLImageResponse) {
  // This is test_bitmap() in data: URL form.
  GURL url(
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVQYlWNk+"
      "M/wn4GBgYGJAQoAHhgCAh6X4CYAAAAASUVORK5CYII=");

  BitmapFetcherTestDelegate delegate(kAsyncCall);
  delegate.SetExpectNotToGetCalled();
  {
    BitmapFetcher fetcher(url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

    fetcher.Init(
        net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
        network::mojom::CredentialsMode::kInclude);
    fetcher.Start(browser()
                      ->profile()
                      ->GetDefaultStoragePartition()
                      ->GetURLLoaderFactoryForBrowserProcess()
                      .get());
  }

  EXPECT_FALSE(delegate.success());
}

class BitmapFetcherInitiatorBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Set up the test server.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&BitmapFetcherInitiatorBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Tear down the test server.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  const std::string& all_headers() const { return all_headers_; }

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    all_headers_ = request.all_headers;
    std::unique_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    return response;
  }

  std::string all_headers_;
};

IN_PROC_BROWSER_TEST_F(BitmapFetcherInitiatorBrowserTest, SameOrigin) {
  GURL image_url = embedded_test_server()->GetURL(kStartTestURL);

  BitmapFetcherTestDelegate delegate(kAsyncCall);
  BitmapFetcher fetcher(image_url, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  fetcher.Init(net::ReferrerPolicy::NEVER_CLEAR,
               network::mojom::CredentialsMode::kInclude,
               net::HttpRequestHeaders(),
               /*initiator=*/url::Origin::Create(image_url));
  fetcher.Start(browser()
                    ->profile()
                    ->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());
  delegate.Wait();

  EXPECT_THAT(all_headers(), testing::HasSubstr("Sec-Fetch-Site: same-origin"));
}
