// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_url_loader_throttle.h"

#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_request_headers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class GlicURLLoaderThrottleTest : public testing::Test {
 protected:
  GlicURLLoaderThrottleTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicSsr);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(GlicURLLoaderThrottleTest,
       SetHeaders_SsrDisabled_OmitsOnboardingHeaders) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicSsr);

  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  net::HttpRequestHeaders headers;
  GlicURLLoaderThrottle::SetHeaders(&headers, &profile_);

  EXPECT_EQ(headers.GetHeader(kGlicHeaderName), kGlicHeaderValue);
  EXPECT_EQ(headers.GetHeader(kGlicVersionHeaderName),
            version_info::GetVersionNumber());
  EXPECT_EQ(headers.GetHeader(kGlicChannelHeaderName),
            version_info::GetChannelString(chrome::GetChannel()));
  EXPECT_EQ(headers.GetHeader(kGlicOnboardingCompletedHeaderName),
            std::nullopt);
  EXPECT_EQ(headers.GetHeader(kGlicOnboardingArmHeaderName), std::nullopt);
}

TEST_F(GlicURLLoaderThrottleTest, SetHeaders_NullProfile) {
  net::HttpRequestHeaders headers;
  GlicURLLoaderThrottle::SetHeaders(&headers, /*profile=*/nullptr);

  EXPECT_EQ(headers.GetHeader(kGlicHeaderName), kGlicHeaderValue);
  EXPECT_EQ(headers.GetHeader(kGlicVersionHeaderName),
            version_info::GetVersionNumber());
  EXPECT_EQ(headers.GetHeader(kGlicChannelHeaderName),
            version_info::GetChannelString(chrome::GetChannel()));
  EXPECT_EQ(headers.GetHeader(kGlicOnboardingCompletedHeaderName), "false");
  EXPECT_EQ(headers.GetHeader(kGlicOnboardingArmHeaderName), std::nullopt);
}

TEST_F(GlicURLLoaderThrottleTest, SetHeaders_OnboardingNotStarted) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  net::HttpRequestHeaders headers;
  GlicURLLoaderThrottle::SetHeaders(&headers, &profile_);

  EXPECT_EQ(headers.GetHeader(kGlicOnboardingCompletedHeaderName), "false");
  EXPECT_EQ(headers.GetHeader(kGlicOnboardingArmHeaderName), "2");
}

TEST_F(GlicURLLoaderThrottleTest, SetHeaders_OnboardingCompleted) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  net::HttpRequestHeaders headers;
  GlicURLLoaderThrottle::SetHeaders(&headers, &profile_);

  EXPECT_EQ(headers.GetHeader(kGlicOnboardingCompletedHeaderName), "true");
  EXPECT_EQ(headers.GetHeader(kGlicOnboardingArmHeaderName), std::nullopt);
}

TEST_F(GlicURLLoaderThrottleTest, WillStartRequest_InjectsHeaders) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  GlicURLLoaderThrottle throttle(profile_.GetWeakPtr());
  network::ResourceRequest request;
  bool defer = false;
  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(request.cors_exempt_headers.GetHeader(kGlicHeaderName),
            kGlicHeaderValue);
  EXPECT_EQ(
      request.cors_exempt_headers.GetHeader(kGlicOnboardingCompletedHeaderName),
      "false");
  EXPECT_EQ(request.cors_exempt_headers.GetHeader(kGlicOnboardingArmHeaderName),
            "2");
}

TEST_F(GlicURLLoaderThrottleTest, WillRedirectRequest_InjectsHeaders) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  GlicURLLoaderThrottle throttle(profile_.GetWeakPtr());
  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams update_params;

  throttle.WillRedirectRequest(&redirect_info, response_head, &defer,
                               &update_params);
  EXPECT_FALSE(defer);

  EXPECT_EQ(
      update_params.modified_cors_exempt_headers.GetHeader(kGlicHeaderName),
      kGlicHeaderValue);
  EXPECT_EQ(update_params.modified_cors_exempt_headers.GetHeader(
                kGlicOnboardingCompletedHeaderName),
            "true");
  EXPECT_EQ(update_params.modified_cors_exempt_headers.GetHeader(
                kGlicOnboardingArmHeaderName),
            std::nullopt);
}

}  // namespace
}  // namespace glic
