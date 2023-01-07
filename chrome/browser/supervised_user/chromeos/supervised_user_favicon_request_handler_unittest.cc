// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_favicon_request_handler.h"

#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/favicon/core/large_icon_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

using testing::_;

namespace {
const base::CancelableTaskTracker::TaskId kTaskId = 1;
}  // namespace

class MockLargeIconService : public favicon::LargeIconService {
 public:
  MockLargeIconService() {
    ON_CALL(*this,
            GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                _, _, _, _, _))
        .WillByDefault(
            [this](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback callback) {
              StoreIconInCache();
              std::move(callback).Run(
                  favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
            });

    ON_CALL(*this, GetLargeIconImageOrFallbackStyleForPageUrl(_, _, _, _, _))
        .WillByDefault([this](auto, auto, auto,
                              favicon_base::LargeIconImageCallback callback,
                              auto) {
          std::move(callback).Run(favicon_base::LargeIconImageResult(
              gfx::Image(favicon_), kIconUrl));
          return kTaskId;
        });
  }

  MockLargeIconService(const MockLargeIconService&) = delete;
  MockLargeIconService& operator=(const MockLargeIconService&) = delete;
  ~MockLargeIconService() override = default;

  void StoreIconInCache() {
    SkBitmap bitmap = gfx::test::CreateBitmap(1, 2);
    favicon_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  gfx::ImageSkia favicon() const { return favicon_; }

  // LargeIconService overrides.
  MOCK_METHOD5(GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
               void(const GURL& page_url,
                    bool may_page_url_be_private,
                    bool should_trim_page_url_path,
                    const net::NetworkTrafficAnnotationTag& traffic_annotation,
                    favicon_base::GoogleFaviconServerCallback callback));
  MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetLargeIconImageOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconImageCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& icon_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD4(GetIconRawBitmapOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD1(TouchIconFromGoogleServer, void(const GURL& icon_url));

  const GURL kIconUrl = GURL("https://www.example.com/icon");

 private:
  gfx::ImageSkia favicon_;
};

class SupervisedUserFaviconRequestHandlerTest : public ::testing::Test {
 public:
  SupervisedUserFaviconRequestHandlerTest() = default;
  SupervisedUserFaviconRequestHandlerTest(
      const SupervisedUserFaviconRequestHandlerTest&) = delete;
  SupervisedUserFaviconRequestHandlerTest& operator=(
      const SupervisedUserFaviconRequestHandlerTest&) = delete;

  void OnFaviconFetched(base::RunLoop* run_loop,
                        const gfx::ImageSkia& favicon) {
    favicon_result_ = favicon;
    run_loop->Quit();
  }

 protected:
  gfx::ImageSkia favicon_result() const { return favicon_result_; }
  base::test::SingleThreadTaskEnvironment task_environment;

 private:
  gfx::ImageSkia favicon_result_;
  base::OnceClosure quit_closure_;
};

TEST_F(SupervisedUserFaviconRequestHandlerTest, GetUncachedFavicon) {
  base::HistogramTester histogram_tester;
  const GURL page_url = GURL("https://www.example.com");
  MockLargeIconService large_icon_service;
  SupervisedUserFaviconRequestHandler handler(page_url, &large_icon_service);

  // If the icon is not in the cache, there should be two calls to fetch it from
  // the cache. One before the network request where the icon is not yet in the
  // cache and one afterwards, when the icon should be present in the cache.
  EXPECT_CALL(large_icon_service,
              GetLargeIconImageOrFallbackStyleForPageUrl(page_url, _, _, _, _))
      .Times(2);
  EXPECT_CALL(large_icon_service,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  page_url, _, _, _, _));
  // Confirm that the icon was retrieved from the cache.
  EXPECT_CALL(large_icon_service,
              TouchIconFromGoogleServer(large_icon_service.kIconUrl));

  base::RunLoop run_loop;
  handler.StartFaviconFetch(
      base::BindOnce(&SupervisedUserFaviconRequestHandlerTest::OnFaviconFetched,
                     base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(favicon_result().bitmap(), large_icon_service.favicon().bitmap());
  histogram_tester.ExpectUniqueSample(
      SupervisedUserFaviconRequestHandler::
          GetFaviconAvailabilityHistogramForTesting(),
      SupervisedUserFaviconRequestHandler::FaviconAvailability::kAvailable, 1);
}

TEST_F(SupervisedUserFaviconRequestHandlerTest, GetCachedFavicon) {
  base::HistogramTester histogram_tester;
  const GURL page_url = GURL("https://www.example.com");
  MockLargeIconService large_icon_service;
  large_icon_service.StoreIconInCache();
  SupervisedUserFaviconRequestHandler handler(page_url, &large_icon_service);

  // Confirm that the icon was retrieved from the cache on the first attempt
  // and no network request was made.
  EXPECT_CALL(large_icon_service,
              GetLargeIconImageOrFallbackStyleForPageUrl(page_url, _, _, _, _))
      .Times(1);
  EXPECT_CALL(large_icon_service,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  page_url, _, _, _, _))
      .Times(0);
  // Confirm that the icon was retrieved from the cache.
  EXPECT_CALL(large_icon_service,
              TouchIconFromGoogleServer(large_icon_service.kIconUrl));

  base::RunLoop run_loop;
  handler.StartFaviconFetch(
      base::BindOnce(&SupervisedUserFaviconRequestHandlerTest::OnFaviconFetched,
                     base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(favicon_result().bitmap(), large_icon_service.favicon().bitmap());
  histogram_tester.ExpectUniqueSample(
      SupervisedUserFaviconRequestHandler::
          GetFaviconAvailabilityHistogramForTesting(),
      SupervisedUserFaviconRequestHandler::FaviconAvailability::kAvailable, 1);
}

TEST_F(SupervisedUserFaviconRequestHandlerTest, GetFallbackFavicon) {
  base::HistogramTester histogram_tester;
  const GURL page_url = GURL("https://www.example.com");
  MockLargeIconService large_icon_service;
  large_icon_service.StoreIconInCache();
  SupervisedUserFaviconRequestHandler handler(page_url, &large_icon_service);

  // Confirm that the favicon is not fetched from a network request or from the
  // cache.
  EXPECT_CALL(large_icon_service,
              GetLargeIconImageOrFallbackStyleForPageUrl(page_url, _, _, _, _))
      .Times(0);
  EXPECT_CALL(large_icon_service,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  page_url, _, _, _, _))
      .Times(0);
  EXPECT_CALL(large_icon_service,
              TouchIconFromGoogleServer(large_icon_service.kIconUrl))
      .Times(0);

  // Expect an icon to still be generated, even if it is not fetched.
  EXPECT_FALSE(handler.GetFaviconOrFallback().isNull());
  histogram_tester.ExpectUniqueSample(
      SupervisedUserFaviconRequestHandler::
          GetFaviconAvailabilityHistogramForTesting(),
      SupervisedUserFaviconRequestHandler::FaviconAvailability::kUnavailable,
      1);
}
