// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_favicon_request_handler.h"

#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/supervised_user/chromeos/mock_large_icon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

using testing::_;

class SupervisedUserFaviconRequestHandlerTest : public ::testing::Test {
 public:
  SupervisedUserFaviconRequestHandlerTest() = default;
  SupervisedUserFaviconRequestHandlerTest(
      const SupervisedUserFaviconRequestHandlerTest&) = delete;
  SupervisedUserFaviconRequestHandlerTest& operator=(
      const SupervisedUserFaviconRequestHandlerTest&) = delete;

  void OnFaviconFetched(base::RunLoop* run_loop) { run_loop->Quit(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

 private:
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
              GetLargeIconRawBitmapForPageUrl(page_url, _, _, _, _, _))
      .Times(2);
  EXPECT_CALL(large_icon_service,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  page_url, _, _, _));
  // Confirm that the icon was retrieved from the cache.
  EXPECT_CALL(large_icon_service,
              TouchIconFromGoogleServer(large_icon_service.kIconUrl));

  base::RunLoop run_loop;
  handler.StartFaviconFetch(
      base::BindOnce(&SupervisedUserFaviconRequestHandlerTest::OnFaviconFetched,
                     base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(handler.GetFaviconOrFallback(),
                                         large_icon_service.favicon()));
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
              GetLargeIconRawBitmapForPageUrl(page_url, _, _, _, _, _))
      .Times(1);
  EXPECT_CALL(large_icon_service,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  page_url, _, _, _))
      .Times(0);
  // Confirm that the icon was retrieved from the cache.
  EXPECT_CALL(large_icon_service,
              TouchIconFromGoogleServer(large_icon_service.kIconUrl));

  base::RunLoop run_loop;
  handler.StartFaviconFetch(
      base::BindOnce(&SupervisedUserFaviconRequestHandlerTest::OnFaviconFetched,
                     base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(handler.GetFaviconOrFallback(),
                                         large_icon_service.favicon()));
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
                  page_url, _, _, _))
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
