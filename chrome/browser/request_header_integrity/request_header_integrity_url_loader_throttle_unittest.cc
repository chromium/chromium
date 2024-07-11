// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/request_header_integrity_url_loader_throttle.h"

#include <memory>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/scoped_channel_override.h"
#endif

namespace request_header_integrity {

namespace {

class RequestHeaderIntegrityURLLoaderThrottleTest : public testing::Test {
 public:
  RequestHeaderIntegrityURLLoaderThrottleTest()
      : throttle_(std::make_unique<RequestHeaderIntegrityURLLoaderThrottle>()) {
  }
  RequestHeaderIntegrityURLLoaderThrottleTest(
      const RequestHeaderIntegrityURLLoaderThrottleTest&) = delete;
  RequestHeaderIntegrityURLLoaderThrottleTest& operator=(
      const RequestHeaderIntegrityURLLoaderThrottleTest&) = delete;

  ~RequestHeaderIntegrityURLLoaderThrottleTest() override = default;

 protected:
  RequestHeaderIntegrityURLLoaderThrottle& throttle() { return *throttle_; }

 private:
  std::unique_ptr<RequestHeaderIntegrityURLLoaderThrottle> throttle_;
};

}  // namespace

TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, NonGoogleSite) {
  network::ResourceRequest request;
  request.url = GURL("https://www.somesite.com/");

  ASSERT_TRUE(request.headers.IsEmpty());
  bool ignored;
  throttle().WillStartRequest(&request, &ignored);
  EXPECT_TRUE(request.headers.IsEmpty());
}

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, GoogleSite) {
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/");

  ASSERT_TRUE(request.headers.IsEmpty());
  bool ignored;
  throttle().WillStartRequest(&request, &ignored);
  EXPECT_EQ(0u, request.headers.GetHeaderVector().size());
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_ANDROID)
TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, GoogleSiteWithBranding) {
  chrome::ScopedChannelOverride override(
      chrome::ScopedChannelOverride::Channel::kStable);
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/");

  ASSERT_TRUE(request.headers.IsEmpty());
  bool ignored;
  throttle().WillStartRequest(&request, &ignored);
  EXPECT_EQ(1u, request.headers.GetHeaderVector().size());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
        // !BUILDFLAG(IS_ANDROID)

}  // namespace request_header_integrity
