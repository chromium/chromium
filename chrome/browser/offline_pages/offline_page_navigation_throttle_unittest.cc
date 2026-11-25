// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_navigation_throttle.h"

#include <memory>

#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

class OfflinePageNavigationThrottleTest : public testing::Test {
 public:
  // Creates a MockNavigationHandle and configures it based on the input param
  std::unique_ptr<content::MockNavigationHandle> MakeMockNavigationHandle(
      bool is_renderer_initiated,
      bool has_offline_headers) {
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        std::make_unique<content::MockNavigationHandle>();
    navigation_handle->set_is_renderer_initiated(is_renderer_initiated);

    net::HttpRequestHeaders request_headers;
    if (has_offline_headers) {
      request_headers.SetHeader(
          "X-Chrome-offline",
          "persist=1 reason=file_url_intent intent_url=not_a_real_file");
    }
    navigation_handle->set_request_headers(request_headers);

    return navigation_handle;
  }

  // Create a NavigationThrottleRegistry and configure it with the
  // MockNavigationHandle
  std::unique_ptr<content::MockNavigationThrottleRegistry>
  MakeMockNavigationThrottleRegistry(
      content::MockNavigationHandle* navigation_handle) {
    return std::make_unique<content::MockNavigationThrottleRegistry>(
        navigation_handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  }
};

TEST_F(OfflinePageNavigationThrottleTest, CancelOfflineRequestFromRenderer) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      MakeMockNavigationHandle(/*is_renderer_initiated=*/true,
                               /*has_offline_headers=*/true);
  std::unique_ptr<content::MockNavigationThrottleRegistry>
      navigation_throttle_registry =
          MakeMockNavigationThrottleRegistry(navigation_handle.get());

  OfflinePageNavigationThrottle::MaybeCreateAndAdd(
      *navigation_throttle_registry);

  // Checks that a non-null OfflinePageNavigationThrottle was created.
  ASSERT_EQ(navigation_throttle_registry->throttles().size(), 1u);

  // Ensure that the created throttle specifies to cancel and ignore the
  // navigation request.
  content::NavigationThrottle::ThrottleCheckResult throttle_check_result =
      navigation_throttle_registry->throttles().back()->WillStartRequest();
  EXPECT_EQ(throttle_check_result.action(),
            content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(OfflinePageNavigationThrottleTest, AllowOfflineRequestFromBrowser) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      MakeMockNavigationHandle(/*is_renderer_initiated=*/false,
                               /*has_offline_headers=*/true);
  std::unique_ptr<content::MockNavigationThrottleRegistry>
      navigation_throttle_registry =
          MakeMockNavigationThrottleRegistry(navigation_handle.get());
  OfflinePageNavigationThrottle::MaybeCreateAndAdd(
      *navigation_throttle_registry);

  // Checks that a OfflinePageNavigationThrottle wasn't created.
  EXPECT_EQ(navigation_throttle_registry->throttles().size(), 0u);
}

TEST_F(OfflinePageNavigationThrottleTest, AllowRegularRequestFromRenderer) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      MakeMockNavigationHandle(/*is_renderer_initiated=*/true,
                               /*has_offline_headers=*/false);
  std::unique_ptr<content::MockNavigationThrottleRegistry>
      navigation_throttle_registry =
          MakeMockNavigationThrottleRegistry(navigation_handle.get());
  OfflinePageNavigationThrottle::MaybeCreateAndAdd(
      *navigation_throttle_registry);

  // Checks that a OfflinePageNavigationThrottle wasn't created.
  EXPECT_EQ(navigation_throttle_registry->throttles().size(), 0u);
}

TEST_F(OfflinePageNavigationThrottleTest, AllowRegularRequestFromBrowser) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      MakeMockNavigationHandle(/*is_renderer_initiated=*/false,
                               /*has_offline_headers=*/false);
  std::unique_ptr<content::MockNavigationThrottleRegistry>
      navigation_throttle_registry =
          MakeMockNavigationThrottleRegistry(navigation_handle.get());
  OfflinePageNavigationThrottle::MaybeCreateAndAdd(
      *navigation_throttle_registry);

  // Checks that a OfflinePageNavigationThrottle wasn't created.
  EXPECT_EQ(navigation_throttle_registry->throttles().size(), 0u);
}

}  // namespace
}  // namespace offline_pages
