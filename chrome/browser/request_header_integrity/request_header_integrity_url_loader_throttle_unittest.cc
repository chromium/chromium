// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/request_header_integrity_url_loader_throttle.h"

#include <memory>
#include <optional>
#include <string>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/request_header_integrity/internal/google_header_names.h"
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
  EXPECT_EQ(3u, request.headers.GetHeaderVector().size());
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_ANDROID)
TEST_F(RequestHeaderIntegrityURLLoaderThrottleTest, GoogleSiteWithBranding) {
  ASSERT_NE(CHANNEL_NAME_HEADER_NAME, "X-Placeholder-1");
  ASSERT_NE(LASTCHANGE_YEAR_HEADER_NAME, "X-Placeholder-2");
  ASSERT_NE(VALIDATE_HEADER_NAME, "X-Placeholder-3");
  ASSERT_NE(COPYRIGHT_HEADER_NAME, "X-Placeholder-4");

  chrome::ScopedChannelOverride override(
      chrome::ScopedChannelOverride::Channel::kStable);
  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/");

  ASSERT_TRUE(request.headers.IsEmpty());
  bool ignored;
  throttle().WillStartRequest(&request, &ignored);
  EXPECT_EQ(4u, request.headers.GetHeaderVector().size());
  EXPECT_TRUE(request.headers.HasHeader(CHANNEL_NAME_HEADER_NAME));
  EXPECT_TRUE(request.headers.HasHeader(LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(request.headers.HasHeader(VALIDATE_HEADER_NAME));
  const std::optional<std::string> copyright =
      request.headers.GetHeader(COPYRIGHT_HEADER_NAME);
  ASSERT_TRUE(copyright.has_value());
  EXPECT_NE(copyright->find("Copyright"), std::string::npos);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
        // !BUILDFLAG(IS_ANDROID)

}  // namespace request_header_integrity
