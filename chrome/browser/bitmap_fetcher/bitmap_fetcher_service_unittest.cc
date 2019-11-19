// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

class TestNotificationInterface {
 public:
  virtual ~TestNotificationInterface() {}
  virtual void OnImageChanged() = 0;
  virtual void OnRequestFinished() = 0;
};

class TestObserver : public BitmapFetcherService::Observer {
 public:
  explicit TestObserver(TestNotificationInterface* target) : target_(target) {}
  ~TestObserver() override { target_->OnRequestFinished(); }

  void OnImageChanged(BitmapFetcherService::RequestId request_id,
                      const SkBitmap& answers_image) override {
    target_->OnImageChanged();
  }

 private:
  TestNotificationInterface* target_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestService : public BitmapFetcherService {
 public:
  explicit TestService(content::BrowserContext* context)
      : BitmapFetcherService(context) {}
  ~TestService() override {}

  // Create a fetcher, but don't start downloading. That allows side-stepping
  // the decode step, which requires a utility process.
  std::unique_ptr<BitmapFetcher> CreateFetcher(
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    return std::make_unique<BitmapFetcher>(url, this, traffic_annotation);
  }
};

}  // namespace

class BitmapFetcherServiceTest : public testing::Test,
                                 public TestNotificationInterface {
 public:
  BitmapFetcherServiceTest()
      : url1_(GURL("http://example.org/sample-image-1.png")),
        url2_(GURL("http://example.org/sample-image-2.png")) {
  }

  void SetUp() override {
    service_.reset(new TestService(&profile_));
    requests_finished_ = 0;
    images_changed_ = 0;
  }

  const std::vector<std::unique_ptr<BitmapFetcherRequest>>& requests() const {
    return service_->requests_;
  }
  const std::vector<std::unique_ptr<BitmapFetcher>>& active_fetchers() const {
    return service_->active_fetchers_;
  }
  size_t cache_size() const { return service_->cache_.size(); }

  void OnImageChanged() override { images_changed_++; }

  void OnRequestFinished() override { requests_finished_++; }

  // Simulate finishing a URL fetch and decode for the given fetcher.
  void CompleteFetch(const GURL& url) {
    const BitmapFetcher* fetcher = service_->FindFetcherForUrl(url);
    ASSERT_TRUE(fetcher);

    // Create a non-empty bitmap.
    SkBitmap image;
    image.allocN32Pixels(2, 2);
    image.eraseColor(SK_ColorGREEN);

    const_cast<BitmapFetcher*>(fetcher)->OnImageDecoded(image);
  }

  void FailFetch(const GURL& url) {
    const BitmapFetcher* fetcher = service_->FindFetcherForUrl(url);
    ASSERT_TRUE(fetcher);
    const_cast<BitmapFetcher*>(fetcher)->OnImageDecoded(SkBitmap());
  }

  // A failed decode results in a nullptr image.
  void FailDecode(const GURL& url) {
    const BitmapFetcher* fetcher = service_->FindFetcherForUrl(url);
    ASSERT_TRUE(fetcher);
    const_cast<BitmapFetcher*>(fetcher)->OnDecodeImageFailed();
  }

 protected:
  std::unique_ptr<BitmapFetcherService> service_;

  int images_changed_;
  int requests_finished_;

  const GURL url1_;
  const GURL url2_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BitmapFetcherServiceTest, RequestInvalidUrl) {
  const BitmapFetcherService::RequestId invalid_request_id =
      BitmapFetcherService::REQUEST_ID_INVALID;
  GURL invalid_url;
  ASSERT_FALSE(invalid_url.is_valid());

  BitmapFetcherService::RequestId request_id = service_->RequestImage(
      invalid_url, new TestObserver(this), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(invalid_request_id, request_id);
}

TEST_F(BitmapFetcherServiceTest, CancelInvalidRequest) {
  service_->CancelRequest(BitmapFetcherService::REQUEST_ID_INVALID);
  service_->CancelRequest(23);
}

TEST_F(BitmapFetcherServiceTest, OnlyFirstRequestCreatesFetcher) {
  EXPECT_EQ(0U, active_fetchers().size());

  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(1U, active_fetchers().size());

  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(1U, active_fetchers().size());
}

TEST_F(BitmapFetcherServiceTest, CompletedFetchNotifiesAllObservers) {
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(1U, active_fetchers().size());
  EXPECT_EQ(4U, requests().size());

  CompleteFetch(url1_);
  EXPECT_EQ(4, images_changed_);
  EXPECT_EQ(4, requests_finished_);
}

TEST_F(BitmapFetcherServiceTest, CancelRequest) {
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  BitmapFetcherService::RequestId requestId = service_->RequestImage(
      url2_, new TestObserver(this), TRAFFIC_ANNOTATION_FOR_TESTS);
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(5U, requests().size());

  service_->CancelRequest(requestId);
  EXPECT_EQ(4U, requests().size());

  CompleteFetch(url2_);
  EXPECT_EQ(0, images_changed_);

  CompleteFetch(url1_);
  EXPECT_EQ(4, images_changed_);
}

TEST_F(BitmapFetcherServiceTest, FailedNullRequestsAreHandled) {
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  service_->RequestImage(url2_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(0U, cache_size());

  CompleteFetch(url1_);
  EXPECT_EQ(1U, cache_size());

  FailDecode(url2_);
  EXPECT_EQ(1U, cache_size());
}
TEST_F(BitmapFetcherServiceTest, FailedRequestsDontEnterCache) {
  service_->RequestImage(url1_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  service_->RequestImage(url2_, new TestObserver(this),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(0U, cache_size());

  CompleteFetch(url1_);
  EXPECT_EQ(1U, cache_size());

  FailFetch(url2_);
  EXPECT_EQ(1U, cache_size());
}
