// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/login_detection/oauth_login_detector.h"

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
  auto login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://foo.com/redirect?code=secret")});

  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://foo.com"), login_info->oauth_requestor_site);
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithMultipleQueryParams) {
  auto login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/"
            "authenticate?client_id=123&redirect_uri=foo.com"),
       GURL("https://foo.com/redirect?scope=userinfo&code=secret")});

  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://foo.com"), login_info->oauth_requestor_site);
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithParamInRef) {
  // OAuth start with client_id in ref.
  // This should no longer be detected as an OAuth start because start params
  // are not checked in the ref.
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate#client_id=123"),
       GURL("https://foo.com/redirect?code=secret")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithCompleteParamInRef) {
  // OAuth completion with code in ref.
  auto login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://foo.com/redirect#code=secret")});
  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://foo.com"), login_info->oauth_requestor_site);

  login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://foo.com/redirect#access_token=token")});
  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://foo.com"), login_info->oauth_requestor_site);

  login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://foo.com/redirect#id_token=token")});
  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://foo.com"), login_info->oauth_requestor_site);

  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithAllParamsInRef) {
  // OAuth start and completion with params in ref.
  // This should no longer be detected as an OAuth start because start params
  // are not checked in the ref. Completion params in ref are still checked.
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate#client_id=123"),
       GURL("https://foo.com/redirect#code=secret")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithNonKeyValueRefAtStart) {
  // OAuth start with a non-key-value ref.
  // This should not be detected as an OAuth start.
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate#randomstring"),
       GURL("https://foo.com/redirect?code=secret")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithNonKeyValueRefAtComplete) {
  // OAuth completion with a non-key-value ref.
  // This should not be detected as an OAuth completion.
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://foo.com/redirect#randomstring")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithNonKeyValueRefAtStartAndComplete) {
  // OAuth start and completion with non-key-value refs.
  // This should not be detected as an OAuth flow.
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate#randomstring1"),
       GURL("https://foo.com/redirect#randomstring2")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, OAuthLoginWithMixedRefAtStart) {
  // OAuth start with a ref that has a valid param and a non-key-value part.
  // This should no longer be detected as an OAuth start because start params
  // are not checked in the ref.
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate#client_id=123&random"),
       GURL("https://foo.com/redirect?code=secret")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST(OAuthLoginDetectorTestWithParams, OAuthLoginRequiringMultipleQueryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kLoginDetection,
      {{"oauth_login_start_request_params", "client_id,redirect_uri"},
       {"oauth_login_complete_request_params", "scope,code"}});
  OAuthLoginDetector detector;

  auto login_info = detector.GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/"
            "authenticate?client_id=123&redirect_uri=foo.com"),

       GURL("https://foo.com/redirect?scope=userinfo&code=secret")});

  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://foo.com"), login_info->oauth_requestor_site);
  EXPECT_FALSE(detector.GetPopUpLoginFlowSite());
}

TEST(OAuthLoginDetectorTestWithParams, OAuthLoginMissingMultipleQueryParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kLoginDetection,
      {{"oauth_login_start_request_params", "client_id,redirect_uri"},
       {"oauth_login_complete_request_params", "scope,code"}});
  OAuthLoginDetector detector;

  // Success if any of the params exist in the URL.
  EXPECT_TRUE(detector.GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://foo.com/redirect?code=secret")}));
  EXPECT_FALSE(detector.GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, LoginNotDetectedForHTTP) {
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("http://oauth.com/authenticate?client_id=123"),
       GURL("http://foo.com/redirect?code=secret")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

// Test that small number of intermediate navigations within OAuth start and
// completion are allowed.
TEST_F(OAuthLoginDetectorTest, IntermediateNavigationsAfterOAuthStart) {
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {
          GURL("https://oauth.com/authenticate?client_id=123"),
          GURL("https://oauth.com/login"),
      }));
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://oauth.com/login"),
      {
          GURL("https://oauth.com/login?username=123&password=123"),
          GURL("https://oauth.com/relogin"),
      }));
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://oauth.com/relogin"),
      {
          GURL("https://oauth.com/login?username=123&password=123"),
          GURL("https://oauth.com/relogin"),
      }));
  EXPECT_TRUE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://oauth.com/relogin"),
      {GURL("https://oauth.com/login?username=123&password=123"),
       GURL("https://oauth.com/loginsuccess"),
       GURL("https://foo.com/redirect?code=secret")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

// Test that OAuth login is not allowed when too many intermediate navigations
// happen within OAuth start and completion.
TEST_F(OAuthLoginDetectorTest, TooManyIntermediateNavigationsAfterOAuthStart) {
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {
          GURL("https://oauth.com/authenticate?client_id=123"),
          GURL("https://oauth.com/login"),
      }));
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://oauth.com/login"),
      {
          GURL("https://oauth.com/login?username=123&password=123"),
          GURL("https://oauth.com/relogin"),
      }));
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://oauth.com/relogin"),
      {
          GURL("https://oauth.com/login?username=123&password=123"),
          GURL("https://oauth.com/relogin"),
      }));
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://oauth.com/relogin"),
      {
          GURL("https://oauth.com/login?username=123&password=123"),
          GURL("https://oauth.com/relogin"),
      }));
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://oauth.com/relogin"),
      {GURL("https://oauth.com/login?username=123&password=123"),
       GURL("https://oauth.com/loginsuccess"),
       GURL("https://foo.com/redirect?code=secret")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

// Test that too many redirects are allowed within the same navigation.
TEST_F(OAuthLoginDetectorTest, RedirectNavigationsAfterOAuthStart) {
  auto login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://oauth.com/login"), GURL("https://oauth.com/loginfailed"),
       GURL("https://oauth.com/relogin"),
       GURL("https://oauth.com/loginsuccess"),
       GURL("https://foo.com/redirect?code=secret")});
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

// Test that OAuth login is detected when there are intermediate navigations to
// other sites.
TEST_F(OAuthLoginDetectorTest, IntermediateNavigationsToOtherSites) {
  auto login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://bar.com/page.html"),
       GURL("https://foo.com/redirect?code=secret")});

  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://foo.com"), login_info->oauth_requestor_site);
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

// Test that OAuth requestor site is correctly detected when the site that
// performs the OAuth completion step is different.
TEST_F(OAuthLoginDetectorTest, DifferentOAuthCompletionSite) {
  auto login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://fooauth.com/redirect?code=secret")});

  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://foo.com"), login_info->oauth_requestor_site);
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

// Test that OAuth completion navigation does not happen for the OAuth provider
// site.
TEST_F(OAuthLoginDetectorTest, OAuthCompletionOnOAuthProviderSite) {
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL("https://foo.com/login.html"),
      {GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://oauth.com/redirect?code=secret")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

TEST_F(OAuthLoginDetectorTest, PopUpLoginFlow) {
  oauth_login_detector_->DidOpenAsPopUp(GURL("https://www.foo.com"));
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL(), {GURL("https://oauth.com/authenticate?client_id=123")}));
  auto login_info = oauth_login_detector_->GetPopUpLoginFlowSite();

  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://www.foo.com"), login_info->oauth_requestor_site);
}

// Tests the popup flow where navigation to non-provider site happens, and login
// is not detected in that case.
TEST_F(OAuthLoginDetectorTest, PopUpLoginFlowNonOAuthProviderNavigations) {
  oauth_login_detector_->DidOpenAsPopUp(GURL("https://www.foo.com"));
  EXPECT_FALSE(oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL(), {GURL("https://oauth.com/authenticate?client_id=123"),
               GURL("https://bar.com/non-oauth-provider.html")}));
  EXPECT_FALSE(oauth_login_detector_->GetPopUpLoginFlowSite());
}

// Tests the login flow where a new window is opened to perform login.
TEST_F(OAuthLoginDetectorTest, NewWindowLoginFlow) {
  auto login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
      GURL() /* Empty URL due to the initial navigation*/,
      {GURL("https://www.foo.com/login.html"),
       GURL("https://oauth.com/authenticate?client_id=123"),
       GURL("https://foo.com/redirect?code=secret")});

  EXPECT_EQ(GURL("https://oauth.com/"), login_info->oauth_provider_site);
  EXPECT_EQ(GURL("https://www.foo.com"), login_info->oauth_requestor_site);
}

}  // namespace login_detection
