// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/header_modification_delegate_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "base/strings/stringprintf.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/mock_bound_session_cookie_refresh_service.h"
#include "net/base/features.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(ENABLE_MIRROR)
#include "base/values.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"
#endif  // BUILDFLAG(ENABLE_MIRROR)

namespace {

using ::testing::_;
using ::testing::Property;
using ::testing::SizeIs;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
class TestResponseAdapter : public signin::ResponseAdapter {
 public:
  explicit TestResponseAdapter(const GURL& url)
      : headers_(new net::HttpResponseHeaders(std::string())),
        url_(url),
        request_top_frame_origin_(url::Origin::Create(url_)) {}

  TestResponseAdapter(const TestResponseAdapter&) = delete;
  TestResponseAdapter& operator=(const TestResponseAdapter&) = delete;

  ~TestResponseAdapter() override = default;

  // `AddHeader()` does not modify any existing headers with the same name.
  void AddHeader(std::string_view header_name, std::string_view header_value) {
    headers_->AddHeader(header_name, header_value);
  }
  void SetRequestTopFrameOrigin(const url::Origin& origin) {
    request_top_frame_origin_ = origin;
  }

  content::WebContents::Getter GetWebContentsGetter() const override {
    return base::BindRepeating(
        []() -> content::WebContents* { return nullptr; });
  }

  GURL GetUrl() const override { return url_; }

  bool IsOutermostMainFrame() const override { return true; }

  std::optional<url::Origin> GetRequestInitiator() const override {
    // Pretend the request came from the same origin.
    return url::Origin::Create(GetUrl());
  }
  const url::Origin* GetRequestTopFrameOrigin() const override {
    return &request_top_frame_origin_;
  }
  const net::HttpResponseHeaders* GetHeaders() const override {
    return headers_.get();
  }

  void RemoveHeader(const std::string& name) override {
    headers_->RemoveHeader(name);
  }

  base::SupportsUserData::Data* GetUserData(const void* key) const override {
    return nullptr;
  }

  void SetUserData(
      const void* key,
      std::unique_ptr<base::SupportsUserData::Data> data) override {}

 private:
  scoped_refptr<net::HttpResponseHeaders> headers_;
  const GURL url_;
  url::Origin request_top_frame_origin_;
};

std::unique_ptr<TestingProfile> CreateTestingProfileForDBSC() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      BoundSessionCookieRefreshServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context) {
        return MockBoundSessionCookieRefreshService::Build();
      }));
  return profile_builder.Build();
}

MockBoundSessionCookieRefreshService* GetMockBoundSessionCookieRefreshService(
    Profile* profile) {
  return static_cast<MockBoundSessionCookieRefreshService*>(
      BoundSessionCookieRefreshServiceFactory::GetForProfile(profile));
}

void AddValidGoogleRegistrationHeader(TestResponseAdapter* response_adapter,
                                      const std::string& registration_path) {
  response_adapter->AddHeader(
      "Sec-Session-Google-Registration-List",
      base::StringPrintf("(ES256);path=\"%s\";challenge=\"Y2hhbGxlbmdl\"",
                         registration_path.c_str()));
}

void AddValidStandardRegistrationHeader(TestResponseAdapter* response_adapter,
                                        const std::string& registration_path) {
  response_adapter->AddHeader(
      "Secure-Session-Registration",
      base::StringPrintf("(ES256);path=\"%s\";challenge=\"Y2hhbGxlbmdl\"",
                         registration_path.c_str()));
}

class BoundSessionHeaderModificationDelegateImplTest : public testing::Test {
 public:
  BoundSessionHeaderModificationDelegateImplTest()
      : testing_profile_(CreateTestingProfileForDBSC()),
        header_modification_delegate_(testing_profile_.get()) {}

  signin::HeaderModificationDelegateImpl& header_modification_delegate() {
    return header_modification_delegate_;
  }

  Profile* testing_profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kEnableBoundSessionCredentials};
  std::unique_ptr<TestingProfile> testing_profile_;
  signin::HeaderModificationDelegateImpl header_modification_delegate_;
};

TEST_F(BoundSessionHeaderModificationDelegateImplTest, GaiaResponse) {
  const GURL response_url("https://accounts.google.com");
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"startsession");
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      SizeIs(1));

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  EXPECT_CALL(*mock_service, CreateRegistrationRequest);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest,
       GaiaMultiSessionResponse) {
  const GURL response_url("https://accounts.google.com");
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"startsession");
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"startsession2");
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      SizeIs(2));

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  EXPECT_CALL(*mock_service, CreateRegistrationRequest).Times(2);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

// Verifies that the session registration requests coming from other Google top
// frames get handled.
TEST_F(BoundSessionHeaderModificationDelegateImplTest,
       GaiaGoogleOriginResponse) {
  const GURL response_url("https://accounts.google.com");
  TestResponseAdapter gaia_response_adapter(response_url);
  gaia_response_adapter.SetRequestTopFrameOrigin(
      url::Origin::Create(GURL("https://mail.google.com")));
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"startsession");
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      SizeIs(1));

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  EXPECT_CALL(*mock_service, CreateRegistrationRequest);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

// Verifies that the session registration requests coming from third-party
// top frames are ignored.
TEST_F(BoundSessionHeaderModificationDelegateImplTest, GaiaThirdPartyResponse) {
  TestResponseAdapter gaia_response_adapter(
      GURL("https://accounts.google.com"));
  gaia_response_adapter.SetRequestTopFrameOrigin(
      url::Origin::Create(GURL("https://example.org")));
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"startsession");
  // Header itself is set correctly.
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      SizeIs(1));

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, MaybeTerminateSession).Times(0);
  EXPECT_CALL(*mock_service, CreateRegistrationRequest(_)).Times(0);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest, NonGaiaResponse) {
  TestResponseAdapter response_adapter(GURL("https://google.com"));
  AddValidGoogleRegistrationHeader(&response_adapter,
                                   /*registration_path=*/"startsession");
  ASSERT_THAT(BoundSessionRegistrationFetcherParam::CreateFromHeaders(
                  response_adapter.GetUrl(), response_adapter.GetHeaders()),
              SizeIs(1));
  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, MaybeTerminateSession).Times(0);
  EXPECT_CALL(*mock_service, CreateRegistrationRequest).Times(0);
  header_modification_delegate().ProcessResponse(&response_adapter, GURL());
}

TEST(BoundSessionDisabledHeaderModificationDelegateImplTest,
     BoundSessionCredentialsDisabled) {
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kEnableBoundSessionCredentials);

  std::unique_ptr<TestingProfile> profile = CreateTestingProfileForDBSC();
  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(profile.get());
  ASSERT_TRUE(mock_service);

  signin::HeaderModificationDelegateImpl header_modification_delegate(
      profile.get());
  const GURL response_url("https://accounts.google.com");
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"startsession");
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      SizeIs(1));

  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  EXPECT_CALL(*mock_service, CreateRegistrationRequest);
  header_modification_delegate.ProcessResponse(&gaia_response_adapter, GURL());
}

TEST(BoundSessionDisabledHeaderModificationDelegateImplTest,
     BoundSessionCookieRefreshServiceIsNull) {
  content::BrowserTaskEnvironment task_environment;
  // The feature state doesn't matter so let's pretend it's enabled.
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kEnableBoundSessionCredentials);

  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      BoundSessionCookieRefreshServiceFactory::GetInstance(),
      base::BindRepeating(
          [](content::BrowserContext* context)
              -> std::unique_ptr<KeyedService> { return nullptr; }));

  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  ASSERT_FALSE(
      BoundSessionCookieRefreshServiceFactory::GetForProfile(profile.get()));

  signin::HeaderModificationDelegateImpl header_modification_delegate(
      profile.get());
  const GURL response_url("https://accounts.google.com");
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"startsession");
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      SizeIs(1));

  // This mainly verifies that no crashes happen.
  header_modification_delegate.ProcessResponse(&gaia_response_adapter, GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest,
       StandardRegistrationBlocksGoogleRegistration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{net::features::kDeviceBoundSessions,
                            net::features::
                                kDeviceBoundSessionsForRestrictedSites},
      /*disabled_features=*/{});
  const GURL response_url("https://accounts.google.com");
  const std::string registration_path = "startsession";
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter, registration_path);
  AddValidStandardRegistrationHeader(&gaia_response_adapter, registration_path);

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  // Session terminations are still processed.
  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  // New session registration should be blocked.
  EXPECT_CALL(*mock_service, CreateRegistrationRequest).Times(0);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest,
       StandardRegistrationAllowsGoogleRegistration) {
  base::test::ScopedFeatureList feature_list(
      net::features::kDeviceBoundSessions);
  const GURL response_url("https://accounts.google.com");
  const std::string registration_path = "startsession";
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter, registration_path);
  AddValidStandardRegistrationHeader(&gaia_response_adapter, registration_path);

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  // Session terminations are still processed.
  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  // New session registration should be allowed.
  EXPECT_CALL(*mock_service, CreateRegistrationRequest).Times(1);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest,
       StandardRegistrationPartiallyBlocksGoogleRegistration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{net::features::kDeviceBoundSessions,
                            net::features::
                                kDeviceBoundSessionsForRestrictedSites},
      /*disabled_features=*/{});
  const GURL response_url("https://accounts.google.com");
  const std::string common_registration_path = "common_startsession";
  const std::string google_registration_path = "google_startsession";
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   common_registration_path);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   google_registration_path);
  AddValidStandardRegistrationHeader(&gaia_response_adapter,
                                     common_registration_path);
  AddValidStandardRegistrationHeader(
      &gaia_response_adapter,
      /*registration_path=*/"standard_startsession");

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  // Session terminations are still processed.
  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  // Only the common session registration should be blocked.
  EXPECT_CALL(*mock_service,
              CreateRegistrationRequest(Property(
                  &BoundSessionRegistrationFetcherParam::registration_endpoint,
                  response_url.Resolve(google_registration_path))));
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest,
       StandardFeatureIgnoredWithoutStandardRegistration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{net::features::kDeviceBoundSessions,
                            net::features::
                                kDeviceBoundSessionsForRestrictedSites},
      /*disabled_features=*/{});
  const GURL response_url("https://accounts.google.com");
  TestResponseAdapter gaia_response_adapter(response_url);
  // Only the Google registration is provided.
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"startsession");

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  // Session terminations are still processed.
  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  // New session registration can proceed because the response didn't contain a
  // standard registration header.
  EXPECT_CALL(*mock_service, CreateRegistrationRequest);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest,
       StandardRegistrationIgnoredWhenPathsDontMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{net::features::kDeviceBoundSessions,
                            net::features::
                                kDeviceBoundSessionsForRestrictedSites},
      /*disabled_features=*/{});
  const GURL response_url("https://accounts.google.com");
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter,
                                   /*registration_path=*/"google_startsession");
  AddValidStandardRegistrationHeader(
      &gaia_response_adapter,
      /*registration_path=*/"standard_startsession");

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  // Session terminations are still processed.
  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  // New session registration can proceed because the standard registration path
  // didn't match.
  EXPECT_CALL(*mock_service, CreateRegistrationRequest);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest,
       StandardRegistrationIgnoredWhenStandardIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kDeviceBoundSessions);
  const GURL response_url("https://accounts.google.com");
  const std::string registration_path = "startsession";
  TestResponseAdapter gaia_response_adapter(response_url);
  AddValidGoogleRegistrationHeader(&gaia_response_adapter, registration_path);
  AddValidStandardRegistrationHeader(&gaia_response_adapter, registration_path);

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  // Session terminations are still processed.
  EXPECT_CALL(*mock_service, MaybeTerminateSession(response_url, _));
  // New session registration can proceed because the standard feature is
  // disabled.
  EXPECT_CALL(*mock_service, CreateRegistrationRequest);
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(ENABLE_MIRROR)
class TestChromeRequestAdapter : public signin::ChromeRequestAdapter {
 public:
  explicit TestChromeRequestAdapter(const GURL& url)
      : signin::ChromeRequestAdapter(url,
                                     original_headers_,
                                     &modified_headers_,
                                     &headers_to_remove_) {}

  const net::HttpRequestHeaders& modified_headers() const {
    return modified_headers_;
  }

  // signin::ChromeRequestAdapter:
  content::WebContents::Getter GetWebContentsGetter() const override {
    return content::WebContents::Getter();
  }
  network::mojom::RequestDestination GetRequestDestination() const override {
    return network::mojom::RequestDestination::kDocument;
  }
  bool IsOutermostMainFrame() const override { return true; }
  bool IsFetchLikeAPI() const override { return false; }
  GURL GetReferrer() const override { return GURL(); }
  void SetDestructionCallback(base::OnceClosure closure) override {}

 private:
  net::HttpRequestHeaders original_headers_;
  net::HttpRequestHeaders modified_headers_;
  std::vector<std::string> headers_to_remove_;
};

// Tests that the SecondaryGoogleAccountSigninAllowed policy is reflected in the
// X-Chrome-Connected header sent to Google web properties.
class SecondaryAccountHeaderModificationDelegateImplTest
    : public testing::Test {
 public:
  SecondaryAccountHeaderModificationDelegateImplTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactories(
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    profile_ = profile_builder.Build();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("test@gmail.com",
                                      signin::ConsentLevel::kSignin);
  }

  void SetSecondaryGoogleAccountSigninAllowedPolicy(bool allowed) {
    profile_->GetTestingPrefService()->SetManagedPref(
        policy::policy_prefs::kSecondaryGoogleAccountSigninAllowed,
        base::Value(allowed));
  }

  // Returns the X-Chrome-Connected header added to a request to Gaia.
  std::optional<std::string> GetChromeConnectedHeader() {
#if BUILDFLAG(IS_ANDROID)
    signin::HeaderModificationDelegateImpl delegate(profile_.get(),
                                                    /*incognito_enabled=*/true);
#else
    signin::HeaderModificationDelegateImpl delegate(profile_.get());
#endif
    TestChromeRequestAdapter request(GURL("https://accounts.google.com"));
    delegate.ProcessRequest(&request, /*redirect_url=*/GURL());
    return request.modified_headers().GetHeader(signin::kChromeConnectedHeader);
  }

 private:
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list_{
      switches::kSecondaryGoogleAccountsSigninAllowedOnAndroid};
#endif
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(SecondaryAccountHeaderModificationDelegateImplTest,
       AddAccountAllowedByDefault) {
  EXPECT_THAT(GetChromeConnectedHeader(),
              testing::Optional(testing::HasSubstr("mode=0,")));
}

TEST_F(SecondaryAccountHeaderModificationDelegateImplTest,
       AddAccountAllowedByPolicy) {
  SetSecondaryGoogleAccountSigninAllowedPolicy(true);
  EXPECT_THAT(GetChromeConnectedHeader(),
              testing::Optional(testing::HasSubstr("mode=0,")));
}

TEST_F(SecondaryAccountHeaderModificationDelegateImplTest,
       AddAccountDisabledByPolicy) {
  SetSecondaryGoogleAccountSigninAllowedPolicy(false);
  // mode=2 is PROFILE_MODE_ADD_ACCOUNT_DISABLED.
  EXPECT_THAT(GetChromeConnectedHeader(),
              testing::Optional(testing::HasSubstr("mode=2,")));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(SecondaryAccountHeaderModificationDelegateImplTest,
       PolicyIgnoredWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kSecondaryGoogleAccountsSigninAllowedOnAndroid);
  SetSecondaryGoogleAccountSigninAllowedPolicy(false);
  EXPECT_THAT(GetChromeConnectedHeader(),
              testing::Optional(testing::HasSubstr("mode=0,")));
}
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_MIRROR)

}  // namespace
