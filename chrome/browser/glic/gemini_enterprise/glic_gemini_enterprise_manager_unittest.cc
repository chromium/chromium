// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/glic_gemini_enterprise_manager.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/gemini_enterprise/gemini_enterprise.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

constexpr mojom::AuthTabPurpose kSignIn = mojom::AuthTabPurpose::kSignIn;
constexpr mojom::AuthTabPurpose kConnectorOauth =
    mojom::AuthTabPurpose::kConnectorOauth;

// Returns OpenAuthTabOptions for `purpose` and `url`.
mojom::OpenAuthTabOptionsPtr MakeOpenOptions(mojom::AuthTabPurpose purpose,
                                             std::optional<GURL> url) {
  auto options = mojom::OpenAuthTabOptions::New();
  options->purpose = purpose;
  options->url = std::move(url);
  return options;
}

mojom::CloseAuthTabOptionsPtr MakeCloseOptions(mojom::AuthTabPurpose purpose) {
  auto options = mojom::CloseAuthTabOptions::New();
  options->purpose = purpose;
  return options;
}

class GlicGeminiEnterpriseManagerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic,
          {{"enabled", "true"},
           {"geic-guest-url", "https://business.gemini.google/side-panel"}}}},
        {});
    profile_ = std::make_unique<TestingProfile>();
    manager_ = std::make_unique<GlicGeminiEnterpriseManager>(profile_.get());
    manager_->Bind(handler_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    manager_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<GlicGeminiEnterpriseManager> manager_;
  mojo::Remote<mojom::GeminiEnterpriseHandler> handler_remote_;

  mojom::OpenAuthTabResult OpenAuthTab(mojom::AuthTabPurpose purpose,
                                       const GURL& url) {
    base::test::TestFuture<mojom::OpenAuthTabResponsePtr> future;
    handler_remote_->OpenAuthTab(MakeOpenOptions(purpose, url),
                                 future.GetCallback());
    return future.Take()->result;
  }
};

TEST_F(GlicGeminiEnterpriseManagerUnitTest, BindDropsReceiverWhenGeicDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(features::kGeic);

  TestingProfile non_geic_profile;
  GlicGeminiEnterpriseManager non_geic_manager(&non_geic_profile);
  mojo::Remote<mojom::GeminiEnterpriseHandler> remote;
  non_geic_manager.Bind(remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       SignInURLAllowlistValidatesSchemesAndOrigins) {
  // Valid HTTPS Gaia origin passes URL validation (reaches kErrorFailure
  // because unit test has no browser window):
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("https://accounts.google.com/signin")),
            mojom::OpenAuthTabResult::kErrorFailure);
  EXPECT_EQ(
      OpenAuthTab(kSignIn, GURL("https://accounts.google.com/o/oauth2/auth")),
      mojom::OpenAuthTabResult::kErrorFailure);

  // Disallowed HTTP scheme (must be HTTPS):
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("http://accounts.google.com/signin")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // Disallowed non-Gaia, non-guest HTTPS origin:
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("https://evil.com/signin")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("https://example.com/login")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // Disallowed URLs with credentials (userinfo):
  EXPECT_EQ(OpenAuthTab(kSignIn,
                        GURL("https://user:pass@accounts.google.com/signin")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(
      OpenAuthTab(kSignIn, GURL("https://user@accounts.google.com/signin")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // Disallowed URLs exceeding maximum allowed length (64KB):
  std::string long_query(65536, 'a');
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("https://accounts.google.com/signin?" +
                                      long_query)),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // Disallowed internal and script schemes:
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("chrome://settings")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("javascript:alert(1)")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("file:///etc/passwd")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // Invalid URL:
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("")),
            mojom::OpenAuthTabResult::kErrorNoUrl);
  EXPECT_EQ(OpenAuthTab(kSignIn, GURL("not-a-valid-url")),
            mojom::OpenAuthTabResult::kErrorNoUrl);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest, AllowsConfiguredGuestOrigin) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      geic::kGeicGuestURLSwitch,
      "https://localhost.corp.google.com:10443/side-panel");
  TestingProfile custom_profile;
  GlicGeminiEnterpriseManager manager(&custom_profile);
  mojo::Remote<mojom::GeminiEnterpriseHandler> remote;
  manager.Bind(remote.BindNewPipeAndPassReceiver());

  auto open_tab = [&](const GURL& url) {
    base::test::TestFuture<mojom::OpenAuthTabResponsePtr> future;
    remote->OpenAuthTab(MakeOpenOptions(kSignIn, url), future.GetCallback());
    return future.Take()->result;
  };

  // Valid guest origin passes URL validation:
  EXPECT_EQ(
      open_tab(GURL("https://localhost.corp.google.com:10443/auth/signin")),
      mojom::OpenAuthTabResult::kErrorFailure);
  // Non-guest origin is rejected:
  EXPECT_EQ(
      open_tab(GURL("https://otherhost.corp.google.com:10443/auth/signin")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       CloseSignInTabReturnsNoSignInTabWhenNeverOpened) {
  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  handler_remote_->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kNoSignInTab);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest, RejectsMissingSignInUrl) {
  mojo::Remote<mojom::GeminiEnterpriseHandler> remote;
  manager_->Bind(remote.BindNewPipeAndPassReceiver());

  {
    base::test::TestFuture<mojom::OpenSignInTabResult> future;
    remote->OpenSignInTab(nullptr, future.GetCallback());
    EXPECT_EQ(future.Take(), mojom::OpenSignInTabResult::kErrorNoUrl);
  }

  {
    base::test::TestFuture<mojom::OpenSignInTabResult> future;
    auto options = mojom::OpenSignInTabOptions::New();
    remote->OpenSignInTab(std::move(options), future.GetCallback());
    EXPECT_EQ(future.Take(), mojom::OpenSignInTabResult::kErrorNoUrl);
  }
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest, RejectsOffTheRecordProfile) {
  TestingProfile::Builder otr_builder;
  Profile* otr_profile = otr_builder.BuildIncognito(profile_.get());
  GlicGeminiEnterpriseManager otr_manager(otr_profile);
  mojo::Remote<mojom::GeminiEnterpriseHandler> otr_remote;
  otr_manager.Bind(otr_remote.BindNewPipeAndPassReceiver());

  // Calling OpenSignInTab on OTR manager should immediately fail.
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future;
  auto options = mojom::OpenSignInTabOptions::New();
  options->signin_url = GURL("https://accounts.google.com/signin");
  otr_remote->OpenSignInTab(std::move(options), open_future.GetCallback());
  EXPECT_EQ(open_future.Take(), mojom::OpenSignInTabResult::kErrorFailure);

  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  otr_remote->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kNoSignInTab);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       ConnectorOauthURLAllowlistValidatesOriginAndPath) {
  // The default GE redirector origin with the redirector path passes URL
  // validation (reaches kErrorFailure in unit test):
  EXPECT_EQ(
      OpenAuthTab(kConnectorOauth,
                  GURL("https://vertexaisearch.cloud.google.com/oauth-redirect"
                       "?continue_uri=https%3A%2F%2Flogin.microsoftonline.com%"
                       "2Fauthorize")),
      mojom::OpenAuthTabResult::kErrorFailure);

  // The guest origin (business.gemini.google in this fixture) hosts the
  // origin-specific redirector:
  EXPECT_EQ(OpenAuthTab(kConnectorOauth,
                        GURL("https://business.gemini.google/oauth-redirect")),
            mojom::OpenAuthTabResult::kErrorFailure);

  // Other GE redirector origins are only allowed when they are the guest
  // origin:
  EXPECT_EQ(
      OpenAuthTab(kConnectorOauth,
                  GURL("https://enterprise.gemini.google.com/oauth-redirect")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(OpenAuthTab(
                kConnectorOauth,
                GURL("https://business-staging.gemini.google/oauth-redirect")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // Wrong path on an allowed origin:
  EXPECT_EQ(OpenAuthTab(kConnectorOauth,
                        GURL("https://vertexaisearch.cloud.google.com/")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(
      OpenAuthTab(
          kConnectorOauth,
          GURL("https://vertexaisearch.cloud.google.com/oauth-redirect/extra")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(
      OpenAuthTab(kConnectorOauth,
                  GURL("https://vertexaisearch.cloud.google.com/side-panel")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // 3P provider URLs must go through the redirector:
  EXPECT_EQ(
      OpenAuthTab(kConnectorOauth,
                  GURL("https://login.microsoftonline.com/oauth-redirect")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(
      OpenAuthTab(kConnectorOauth, GURL("https://evil.com/oauth-redirect")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // Gaia is allowed for sign-in, but not as a connector auth start URL:
  EXPECT_EQ(OpenAuthTab(kConnectorOauth,
                        GURL("https://accounts.google.com/oauth-redirect")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);

  // Basic shape checks:
  EXPECT_EQ(OpenAuthTab(
                kConnectorOauth,
                GURL("http://vertexaisearch.cloud.google.com/oauth-redirect")),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(
      OpenAuthTab(kConnectorOauth,
                  GURL("https://user:pass@vertexaisearch.cloud.google.com/"
                       "oauth-redirect")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(
      OpenAuthTab(
          kConnectorOauth,
          GURL("https://vertexaisearch.cloud.google.com:8443/oauth-redirect")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  std::string long_query(65536, 'a');
  EXPECT_EQ(OpenAuthTab(
                kConnectorOauth,
                GURL("https://vertexaisearch.cloud.google.com/oauth-redirect?" +
                     long_query)),
            mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  EXPECT_EQ(OpenAuthTab(kConnectorOauth, GURL("")),
            mojom::OpenAuthTabResult::kErrorNoUrl);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       ConnectorAuthAllowsConfiguredGuestOrigin) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      geic::kGeicGuestURLSwitch,
      "https://localhost.corp.google.com:10443/side-panel");
  TestingProfile custom_profile;
  GlicGeminiEnterpriseManager manager(&custom_profile);
  mojo::Remote<mojom::GeminiEnterpriseHandler> remote;
  manager.Bind(remote.BindNewPipeAndPassReceiver());

  auto open_tab = [&](const GURL& url) {
    base::test::TestFuture<mojom::OpenAuthTabResponsePtr> future;
    remote->OpenAuthTab(MakeOpenOptions(kConnectorOauth, url),
                        future.GetCallback());
    return future.Take()->result;
  };

  EXPECT_EQ(
      open_tab(GURL("https://localhost.corp.google.com:10443/oauth-redirect")),
      mojom::OpenAuthTabResult::kErrorFailure);
  EXPECT_EQ(
      open_tab(GURL("https://otherhost.corp.google.com:10443/oauth-redirect")),
      mojom::OpenAuthTabResult::kErrorDisallowedUrl);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest, UnknownPurposeIsRejected) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kUnknown,
                      GURL("https://accounts.google.com/signin")),
      open_future.GetCallback());
  EXPECT_EQ(open_future.Take()->result,
            mojom::OpenAuthTabResult::kErrorInvalidPurpose);

  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kUnknown),
      close_future.GetCallback());
  EXPECT_EQ(close_future.Take()->result,
            mojom::CloseAuthTabResult::kErrorInvalidPurpose);

  histogram_tester.ExpectUniqueSample(
      "Geic.AuthTab.OpenResult.Unknown",
      mojom::OpenAuthTabResult::kErrorInvalidPurpose, 1);
  histogram_tester.ExpectUniqueSample(
      "Geic.AuthTab.CloseResult.Unknown",
      mojom::CloseAuthTabResult::kErrorInvalidPurpose, 1);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest, OpenAuthTabRejectsMissingUrl) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> future;
  handler_remote_->OpenAuthTab(MakeOpenOptions(kConnectorOauth, std::nullopt),
                               future.GetCallback());
  EXPECT_EQ(future.Take()->result, mojom::OpenAuthTabResult::kErrorNoUrl);
  histogram_tester.ExpectUniqueSample("Geic.AuthTab.OpenResult.ConnectorOAuth",
                                      mojom::OpenAuthTabResult::kErrorNoUrl, 1);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       OpenAuthTabRejectsUrlDisallowedForPurpose) {
  base::HistogramTester histogram_tester;
  // A 3P provider URL must go through the GE redirector.
  {
    base::test::TestFuture<mojom::OpenAuthTabResponsePtr> future;
    handler_remote_->OpenAuthTab(
        MakeOpenOptions(kConnectorOauth,
                        GURL("https://login.microsoftonline.com/authorize")),
        future.GetCallback());
    EXPECT_EQ(future.Take()->result,
              mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  }
  // The GE redirector is not a valid sign-in URL.
  {
    base::test::TestFuture<mojom::OpenAuthTabResponsePtr> future;
    handler_remote_->OpenAuthTab(
        MakeOpenOptions(
            kSignIn,
            GURL("https://vertexaisearch.cloud.google.com/oauth-redirect")),
        future.GetCallback());
    EXPECT_EQ(future.Take()->result,
              mojom::OpenAuthTabResult::kErrorDisallowedUrl);
  }
  histogram_tester.ExpectUniqueSample(
      "Geic.AuthTab.OpenResult.ConnectorOAuth",
      mojom::OpenAuthTabResult::kErrorDisallowedUrl, 1);
  histogram_tester.ExpectUniqueSample(
      "Geic.AuthTab.OpenResult.SignIn",
      mojom::OpenAuthTabResult::kErrorDisallowedUrl, 1);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       CloseAuthTabReturnsNoAuthTabWhenNeverOpened) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future;
  handler_remote_->CloseAuthTab(MakeCloseOptions(kConnectorOauth),
                                close_future.GetCallback());
  EXPECT_EQ(close_future.Take()->result, mojom::CloseAuthTabResult::kNoAuthTab);
  histogram_tester.ExpectUniqueSample("Geic.AuthTab.CloseResult.ConnectorOAuth",
                                      mojom::CloseAuthTabResult::kNoAuthTab, 1);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       DeprecatedSignInMethodsRecordSignInMetrics) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future;
  handler_remote_->OpenSignInTab(nullptr, open_future.GetCallback());
  EXPECT_EQ(open_future.Take(), mojom::OpenSignInTabResult::kErrorNoUrl);
  histogram_tester.ExpectUniqueSample("Geic.AuthTab.OpenResult.SignIn",
                                      mojom::OpenAuthTabResult::kErrorNoUrl, 1);

  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  handler_remote_->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kNoSignInTab);
  histogram_tester.ExpectUniqueSample("Geic.AuthTab.CloseResult.SignIn",
                                      mojom::CloseAuthTabResult::kNoAuthTab, 1);
}

TEST_F(GlicGeminiEnterpriseManagerUnitTest,
       OpenAuthTabRejectsOffTheRecordProfile) {
  TestingProfile::Builder otr_builder;
  Profile* otr_profile = otr_builder.BuildIncognito(profile_.get());
  GlicGeminiEnterpriseManager otr_manager(otr_profile);
  mojo::Remote<mojom::GeminiEnterpriseHandler> otr_remote;
  otr_manager.Bind(otr_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future;
  otr_remote->OpenAuthTab(
      MakeOpenOptions(
          kConnectorOauth,
          GURL("https://vertexaisearch.cloud.google.com/oauth-redirect")),
      open_future.GetCallback());
  EXPECT_EQ(open_future.Take()->result,
            mojom::OpenAuthTabResult::kErrorFailure);
}

}  // namespace
}  // namespace glic
