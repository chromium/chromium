// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_navigation_throttle.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

class OfflinePageNavigationThrottleTest : public testing::Test {
 public:
  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  // Creates a MockNavigationHandle and configures it based on the input param
  std::unique_ptr<content::MockNavigationHandle> MakeMockNavigationHandle(
      bool is_renderer_initiated,
      bool has_offline_headers) {
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        std::make_unique<content::MockNavigationHandle>();
    navigation_handle->set_is_renderer_initiated(is_renderer_initiated);

    net::HttpRequestHeaders request_headers;
    if (has_offline_headers)
      request_headers.SetHeader(
          "X-Chrome-offline",
          "persist=1 reason=file_url_intent intent_url=not_a_real_file");
    navigation_handle->set_request_headers(request_headers);

    return navigation_handle;
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(OfflinePageNavigationThrottleTest, CancelOfflineRequestFromRenderer) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      MakeMockNavigationHandle(/*is_renderer_initiated=*/true,
                               /*has_offline_headers=*/true);

  std::unique_ptr<content::NavigationThrottle> navigation_throttle =
      OfflinePageNavigationThrottle::MaybeCreateThrottleFor(
          navigation_handle.get());

  // Checks that a non-null OfflinePageNavigationThrottle was created, and a
  // value of true was logged to UMA.
  ASSERT_NE(navigation_throttle, nullptr);
  histogram_tester()->ExpectUniqueSample(
      kOfflinePagesDidNavigationThrottleCancelNavigation, true, 1);

  // Ensure that the created throttle specifies to cancel and ignore the
  // navigation request.
  content::NavigationThrottle::ThrottleCheckResult throttle_check_result =
      navigation_throttle->WillStartRequest();
  EXPECT_EQ(throttle_check_result.action(),
            content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(OfflinePageNavigationThrottleTest, AllowOfflineRequestFromBrowser) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      MakeMockNavigationHandle(/*is_renderer_initiated=*/false,
                               /*has_offline_headers=*/true);

  std::unique_ptr<content::NavigationThrottle> navigation_throttle =
      OfflinePageNavigationThrottle::MaybeCreateThrottleFor(
          navigation_handle.get());

  // Checks that a OfflinePageNavigationThrottle wasn't created and instead a
  // nullptr was returned. Additionally checks that a value of false was logged
  // to UMA.
  EXPECT_EQ(navigation_throttle, nullptr);
  histogram_tester()->ExpectUniqueSample(
      kOfflinePagesDidNavigationThrottleCancelNavigation, false, 1);
}

TEST_F(OfflinePageNavigationThrottleTest, AllowRegularRequestFromRenderer) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      MakeMockNavigationHandle(/*is_renderer_initiated=*/true,
                               /*has_offline_headers=*/false);

  std::unique_ptr<content::NavigationThrottle> navigation_throttle =
      OfflinePageNavigationThrottle::MaybeCreateThrottleFor(
          navigation_handle.get());

  // Checks that a OfflinePageNavigationThrottle wasn't created and instead a
  // nullptr was returned. Additionally checks that a value of false was logged
  // to UMA.
  EXPECT_EQ(navigation_throttle, nullptr);
  histogram_tester()->ExpectUniqueSample(
      kOfflinePagesDidNavigationThrottleCancelNavigation, false, 1);
}

TEST_F(OfflinePageNavigationThrottleTest, AllowRegularRequestFromBrowser) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      MakeMockNavigationHandle(/*is_renderer_initiated=*/false,
                               /*has_offline_headers=*/false);

  std::unique_ptr<content::NavigationThrottle> navigation_throttle =
      OfflinePageNavigationThrottle::MaybeCreateThrottleFor(
          navigation_handle.get());

  // Checks that a OfflinePageNavigationThrottle wasn't created and instead a
  // nullptr was returned. Additionally checks that a value of false was logged
  // to UMA.
  EXPECT_EQ(navigation_throttle, nullptr);
  histogram_tester()->ExpectUniqueSample(
      kOfflinePagesDidNavigationThrottleCancelNavigation, false, 1);
}

}  // namespace
}  // namespace offline_pages
