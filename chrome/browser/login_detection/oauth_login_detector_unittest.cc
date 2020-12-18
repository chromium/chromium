// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/login_detection/oauth_login_detector.h"

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace login_detection {

class OAuthLoginDetectorTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kLoginDetection);
    oauth_login_detector_ = std::make_unique<OAuthLoginDetector>();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OAuthLoginDetector> oauth_login_detector_;
};

TEST_F(OAuthLoginDetectorTest, SimpleOAuthLogin) {
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/authenticate?client_id=123")));
  EXPECT_TRUE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://foo.com/redirect?code=secret")));
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithMultipleQueryParams) {
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(GURL(
      "https://oauth.com/authenticate?client_id=123&redirect_uri=foo.com")));
  EXPECT_TRUE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://foo.com/redirect?scope=userinfo&code=secret")));
}

TEST(OAuthLoginDetectorTestWithParams, OAuthLoginRequiringMultipleQueryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kLoginDetection,
      {{"oauth_login_start_request_params", "client_id,redirect_uri"},
       {"oauth_login_complete_request_params", "scope,code"}});
  OAuthLoginDetector detector;

  EXPECT_FALSE(detector.CheckSuccessfulLoginFlow(GURL(
      "https://oauth.com/authenticate?client_id=123&redirect_uri=foo.com")));
  EXPECT_TRUE(detector.CheckSuccessfulLoginFlow(
      GURL("https://foo.com/redirect?scope=userinfo&code=secret")));
}

TEST(OAuthLoginDetectorTestWithParams, OAuthLoginMissingMultipleQueryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kLoginDetection,
      {{"oauth_login_start_request_params", "client_id,redirect_uri"},
       {"oauth_login_complete_request_params", "scope,code"}});
  OAuthLoginDetector detector;

  EXPECT_FALSE(detector.CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/authenticate?client_id=123")));
  EXPECT_FALSE(detector.CheckSuccessfulLoginFlow(
      GURL("https://foo.com/redirect?code=secret")));
}

TEST_F(OAuthLoginDetectorTest, LoginNotDetectedForHTTP) {
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("http://oauth.com/authenticate?client_id=123")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("http://foo.com/redirect?code=secret")));
}

// Test that small number of intermediate navigations within OAuth start and
// completion are allowed.
TEST_F(OAuthLoginDetectorTest, IntermediateNavigationsAfterOAuthStart) {
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/authenticate?client_id=123")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/login")));
  EXPECT_TRUE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://foo.com/redirect?code=secret")));
}

// Test that OAuth login is not allowed when too many intermediate navigations
// happen within OAuth start and completion.
TEST_F(OAuthLoginDetectorTest, TooManyIntermediateNavigationsAfterOAuthStart) {
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/authenticate?client_id=123")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/login")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/login")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/login")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/login")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://foo.com/redirect?code=secret")));
}

// Test that OAuth login is not detected when there are intermediate navigations
// to other sites.
TEST_F(OAuthLoginDetectorTest, IntermediateNavigationsToOtherSites) {
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://oauth.com/authenticate?client_id=123")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://bar.com/page.html")));
  EXPECT_FALSE(oauth_login_detector_->CheckSuccessfulLoginFlow(
      GURL("https://foo.com/redirect?code=secret")));
}

}  // namespace login_detection
