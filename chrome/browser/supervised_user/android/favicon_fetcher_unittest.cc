// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/favicon_fetcher.h"
#include <cstddef>
#include <string>
#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/gmock_callback_support.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "favicon_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace {

const GURL& StringToGURL(std::string uri) {
  static const base::NoDestructor<GURL> url(uri);
  return *url;
}
const base::CancelableTaskTracker::TaskId kTaskId = 1;

favicon_base::LargeIconImageResult CreateEmptyTestImageResult() {
  SkBitmap bitmap_result = SkBitmap();
  return favicon_base::LargeIconImageResult(
      gfx::Image::CreateFrom1xBitmap(bitmap_result), StringToGURL("icon.com"));
}

favicon_base::LargeIconImageResult CreateTestImageResult() {
  return favicon_base::LargeIconImageResult(gfx::test::CreateImage(/*size=*/10),
                                            StringToGURL("icon.com"));
}

class MockLargeIconService : public favicon::LargeIconService {
 public:
  MockLargeIconService() = default;
  MockLargeIconService(const MockLargeIconService&) = delete;
  MockLargeIconService& operator=(const MockLargeIconService&) = delete;
  ~MockLargeIconService() override = default;

  // LargeIconService overrides.
  MOCK_METHOD(void,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
              (const GURL& page_url,
               bool should_trim_page_url_path,
               const net::NetworkTrafficAnnotationTag& traffic_annotation,
               favicon_base::GoogleFaviconServerCallback callback),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconImageOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconImageCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(
      base::CancelableTaskTracker::TaskId,
      GetLargeIconRawBitmapForPageUrl,
      (const GURL& page_url,
       int min_source_size_in_pixel,
       std::optional<int> size_in_pixel_to_resize_to,
       LargeIconService::NoBigEnoughIconBehavior no_big_enough_icon_behavior,
       favicon_base::LargeIconCallback callback,
       base::CancelableTaskTracker* tracker),
      (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
              (const GURL& icon_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetIconRawBitmapOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(void,
              GetLargeIconFromCacheFallbackToGoogleServer,
              (const GURL& page_url,
               StandardIconSize min_source_size_in_pixel,
               std::optional<StandardIconSize> size_in_pixel_to_resize_to,
               NoBigEnoughIconBehavior no_big_enough_icon_behavior,
               bool should_trim_page_url_path,
               const net::NetworkTrafficAnnotationTag& traffic_annotation,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(void,
              TouchIconFromGoogleServer,
              (const GURL& icon_url),
              (override));
};

// FaviconFetcher is the class under test, however we need to mock some of its
// methods.
class MockFaviconFetcher : public ::FaviconFetcher {
 public:
  explicit MockFaviconFetcher(
      raw_ptr<favicon::LargeIconService> large_icon_service)
      : FaviconFetcher(large_icon_service) {}

  MOCK_METHOD(void,
              ExecuteFaviconCallback,
              (const base::android::ScopedJavaGlobalRef<jobject>& callback,
               SkBitmap bitmap),
              (override));
};

class FaviconFetcherTest : public ::testing::Test {
 public:
  FaviconFetcherTest() = default;
  FaviconFetcherTest(const FaviconFetcherTest&) = delete;
  FaviconFetcherTest& operator=(const FaviconFetcherTest&) = delete;

 protected:
  MockLargeIconService mock_large_icon_service_;
  raw_ptr<MockFaviconFetcher> favicon_fetcher_ =
      new MockFaviconFetcher(&mock_large_icon_service_);
};

// Checks that an image was successfully returned from the server
// after an image fetch request. Checks that the image is returned to the
// caller and the fetcher object is successfully destroyed.
TEST_F(FaviconFetcherTest, SucessfullImageRequestSuccessfulImageFetch) {
  // Successful image request from server.
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<3>(
          favicon_base::GoogleFaviconServerRequestStatus::SUCCESS));

  // Returns an empty image at first, then a valid image (after requesting it
  // from server).
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconImageOrFallbackStyleForPageUrl(
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(2)
      .WillOnce([=](auto, auto, auto,
                    favicon_base::LargeIconImageCallback image_callback, auto) {
        std::move(image_callback).Run(CreateEmptyTestImageResult());
        return kTaskId;
      })
      .WillOnce([=](auto, auto, auto,
                    favicon_base::LargeIconImageCallback image_callback, auto) {
        std::move(image_callback).Run(CreateTestImageResult());
        return kTaskId;
      });

  // Tests that favicon is returned to the caller (Android callback execution).
  EXPECT_CALL(*favicon_fetcher_, ExecuteFaviconCallback(testing::_, testing::_))
      .Times(1);

  base::WeakPtr<FaviconFetcher> faviconFetcherWeakPtr =
      favicon_fetcher_->GetWeakPtr();
  favicon_fetcher_->FetchFavicon(StringToGURL("page.com"), true, 16, 32,
                                 base::android::ScopedJavaGlobalRef<jobject>());

  // Tests that the favicon_fetcher_ instance test is destroyed.
  EXPECT_TRUE(faviconFetcherWeakPtr.WasInvalidated());
}

// Checks that an image that does not meet our specs is returned from the server
// after an image fetch request. Checks that no image is returned to the
// caller and the fetcher object is successfully destroyed.
TEST_F(FaviconFetcherTest, SucessfullImageRequestUnsuccessfulImageFetch) {
  // Successful image request from server.
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<3>(
          favicon_base::GoogleFaviconServerRequestStatus::SUCCESS));

  // Returns an empty image twice (when image does not comply to our specs).
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconImageOrFallbackStyleForPageUrl(
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(2)
      .WillRepeatedly([=](auto, auto, auto,
                          favicon_base::LargeIconImageCallback image_callback,
                          auto) {
        std::move(image_callback).Run(CreateEmptyTestImageResult());
        return kTaskId;
      });

  // No favicon is returned to the caller.
  EXPECT_CALL(*favicon_fetcher_, ExecuteFaviconCallback(testing::_, testing::_))
      .Times(0);

  base::WeakPtr<FaviconFetcher> faviconFetcherWeakPtr =
      favicon_fetcher_->GetWeakPtr();
  favicon_fetcher_->FetchFavicon(StringToGURL("page.com"), true, 16, 32,
                                 base::android::ScopedJavaGlobalRef<jobject>());

  // Tests that the favicon_fetcher_ instance test is destroyed.
  EXPECT_TRUE(faviconFetcherWeakPtr.WasInvalidated());
}

// Checks that an image is successfully returned from the cache without
// making a request to retrieve it from the server. Checks that the image is
// returned to the caller and the fetcher object is successfully destroyed.
TEST_F(FaviconFetcherTest, SuccessfullImageFetchFromCache) {
  // Returns a valid image (already cached).
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconImageOrFallbackStyleForPageUrl(
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce([=](auto, auto, auto,
                    favicon_base::LargeIconImageCallback image_callback, auto) {
        std::move(image_callback).Run(CreateTestImageResult());
        return kTaskId;
      });

  // Tests that favicon is returned to the caller (Android callback execution).
  EXPECT_CALL(*favicon_fetcher_, ExecuteFaviconCallback(testing::_, testing::_))
      .Times(1);

  // No server request is done when image exists in cache.
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  base::WeakPtr<FaviconFetcher> faviconFetcherWeakPtr =
      favicon_fetcher_->GetWeakPtr();
  favicon_fetcher_->FetchFavicon(StringToGURL("page.com"), true, 16, 32,
                                 base::android::ScopedJavaGlobalRef<jobject>());

  // Tests that the favicon_fetcher_ instance test is destroyed.
  EXPECT_TRUE(faviconFetcherWeakPtr.WasInvalidated());
}

// Checks that no image is successfuly returned from the server after requesting
// it. Checks that no image is returned to the caller and the fetcher object is
// successfully destroyed.
TEST_F(FaviconFetcherTest, UnuccessfullImageRequest) {
  // Unsuccessful image request from server.
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<3>(
          favicon_base::GoogleFaviconServerRequestStatus::FAILURE_HTTP_ERROR));

  // Returns an empty image.
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconImageOrFallbackStyleForPageUrl(
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce([=](auto, auto, auto,
                    favicon_base::LargeIconImageCallback image_callback, auto) {
        std::move(image_callback).Run(CreateEmptyTestImageResult());
        return kTaskId;
      });

  // Nothing is returned to the caller.
  EXPECT_CALL(*favicon_fetcher_, ExecuteFaviconCallback(testing::_, testing::_))
      .Times(0);

  base::WeakPtr<FaviconFetcher> faviconFetcherWeakPtr =
      favicon_fetcher_->GetWeakPtr();
  favicon_fetcher_->FetchFavicon(StringToGURL("page.com"), true, 16, 32,
                                 base::android::ScopedJavaGlobalRef<jobject>());

  // Tests that the favicon_fetcher_ instance test is destroyed.
  EXPECT_TRUE(faviconFetcherWeakPtr.WasInvalidated());
}

}  // namespace
