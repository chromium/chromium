// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_url_loader_throttle.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

class ContextualTasksURLLoaderThrottleTest : public testing::Test {
 public:
  ContextualTasksURLLoaderThrottleTest() { EnableRequiredFeatures(); }

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override { profile_.reset(); }

  void EnableRequiredFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            kContextualTasksRearchitecture,
            extensions_features::kApiContextualTasksPrivate,
        },
        /*disabled_features=*/{});
  }

  TestingProfile* profile() { return profile_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

// 1. Initial request to https://www.google.com/search:
// Header Chrome-Search-Capabilities-Version: 1 present in
// cors_exempt_headers.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       InitialRequest_GoogleUrl_AppendsHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/search");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(request.cors_exempt_headers.GetHeader(
                kContextualTasksSearchCapabilitiesHeaderName),
            kContextualTasksSearchCapabilitiesDefaultVersion);
}

// 2. Initial request to https://example.com: Header absent.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       InitialRequest_NonGoogleUrl_DoesNotAppendHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  network::ResourceRequest request;
  request.url = GURL("https://example.com");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_FALSE(request.cors_exempt_headers.HasHeader(
      kContextualTasksSearchCapabilitiesHeaderName));
}

// Non-HTTPS Google URL: Header absent.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       InitialRequest_NonHttpsGoogleUrl_DoesNotAppendHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  network::ResourceRequest request;
  request.url = GURL("http://www.google.com/search");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_FALSE(request.cors_exempt_headers.HasHeader(
      kContextualTasksSearchCapabilitiesHeaderName));
}

// 3. Redirect Non-Google -> Google (e.g. https://g.ai/ ->
// https://www.google.com/search?udm=50): Header added in
// modified_cors_exempt_headers.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       Redirect_NonGoogleToGoogle_AppendsHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.google.com/search?udm=50");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle->WillRedirectRequest(&redirect_info, response_head, &defer,
                                &headers_update_params);
  EXPECT_FALSE(defer);

  EXPECT_EQ(headers_update_params.modified_cors_exempt_headers.GetHeader(
                kContextualTasksSearchCapabilitiesHeaderName),
            kContextualTasksSearchCapabilitiesDefaultVersion);
  EXPECT_TRUE(headers_update_params.removed_headers.empty());
}

// 4. Redirect Google -> Non-Google (e.g. https://www.google.com ->
// https://external.com): Header added to removed_headers.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       Redirect_GoogleToNonGoogle_RemovesHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://external.com");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle->WillRedirectRequest(&redirect_info, response_head, &defer,
                                &headers_update_params);
  EXPECT_FALSE(defer);

  EXPECT_FALSE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      kContextualTasksSearchCapabilitiesHeaderName));
  EXPECT_THAT(
      headers_update_params.removed_headers,
      testing::ElementsAre(kContextualTasksSearchCapabilitiesHeaderName));
}

// 5. Ineligible profile (incognito or feature flags disabled): Throttle
// returns nullptr.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       IneligibleProfile_IncognitoReturnsNullptr) {
  Profile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(profile());
  ASSERT_NE(incognito_profile, nullptr);

  EXPECT_EQ(ContextualTasksURLLoaderThrottle::MaybeCreate(incognito_profile),
            nullptr);
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       IneligibleProfile_NullProfileReturnsNullptr) {
  EXPECT_EQ(ContextualTasksURLLoaderThrottle::MaybeCreate(nullptr), nullptr);
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       IneligibleProfile_RearchitectureDisabledReturnsNullptr) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{extensions_features::kApiContextualTasksPrivate},
      /*disabled_features=*/{kContextualTasksRearchitecture});

  EXPECT_EQ(ContextualTasksURLLoaderThrottle::MaybeCreate(profile()), nullptr);
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       IneligibleProfile_PrivateApiDisabledReturnsNullptr) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kContextualTasksRearchitecture},
      /*disabled_features=*/{extensions_features::kApiContextualTasksPrivate});

  EXPECT_EQ(ContextualTasksURLLoaderThrottle::MaybeCreate(profile()), nullptr);
}

// Custom version string via base::FeatureParam.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       CustomVersion_ConfiguredViaFeatureParam) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {kContextualTasksRearchitecture,
           {{"contextual-tasks-search-capabilities-version", "2.0"}}},
          {extensions_features::kApiContextualTasksPrivate, {}},
      },
      /*disabled_features=*/{});

  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/search");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);

  EXPECT_EQ(request.cors_exempt_headers.GetHeader(
                kContextualTasksSearchCapabilitiesHeaderName),
            "2.0");
}

}  // namespace contextual_tasks
