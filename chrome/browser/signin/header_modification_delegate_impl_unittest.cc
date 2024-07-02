// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/header_modification_delegate_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/optional_util.h"
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
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_debug_info.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace {

using testing::_;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
class MockBoundSessionCookieRefreshService
    : public BoundSessionCookieRefreshService {
 public:
  static std::unique_ptr<KeyedService> Build() {
    return std::make_unique<MockBoundSessionCookieRefreshService>();
  }

  MOCK_METHOD(void,
              MaybeTerminateSession,
              (const GURL& response_url,
               const net::HttpResponseHeaders* headers),
              (override));
  MOCK_METHOD(void,
              CreateRegistrationRequest,
              (BoundSessionRegistrationFetcherParam registration_params),
              (override));

  MOCK_METHOD(void, Initialize, (), (override));
  MOCK_METHOD(void,
              RegisterNewBoundSession,
              (const bound_session_credentials::BoundSessionParams& params),
              (override));
  MOCK_METHOD(std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>,
              GetBoundSessionThrottlerParams,
              (),
              (const, override));
  MOCK_METHOD(
      void,
      SetRendererBoundSessionThrottlerParamsUpdaterDelegate,
      (RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater),
      (override));
  MOCK_METHOD(void,
              SetBoundSessionParamsUpdatedCallbackForTesting,
              (base::RepeatingClosure updated_callback),
              (override));
  MOCK_METHOD(void,
              HandleRequestBlockedOnCookie,
              (const GURL&,
               HandleRequestBlockedOnCookieCallback resume_blocked_request),
              (override));
  MOCK_METHOD(base::WeakPtr<BoundSessionCookieRefreshService>,
              GetWeakPtr,
              (),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer* observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer* observer), (override));
  MOCK_METHOD((std::vector<BoundSessionDebugInfo>),
              GetBoundSessionDebugInfo,
              (),
              (const, override));
};

class TestResponseAdapter : public signin::ResponseAdapter {
 public:
  explicit TestResponseAdapter(const GURL& url)
      : headers_(new net::HttpResponseHeaders(std::string())),
        url_(url),
        request_top_frame_origin_(url::Origin::Create(url_)) {}

  TestResponseAdapter(const TestResponseAdapter&) = delete;
  TestResponseAdapter& operator=(const TestResponseAdapter&) = delete;

  ~TestResponseAdapter() override = default;

  void SetHeader(const std::string& header_name,
                 const std::string& header_value) {
    headers_->SetHeader(header_name, header_value);
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

void SetValidRegistrationHeader(TestResponseAdapter* response_adapter) {
  response_adapter->SetHeader(
      "Sec-Session-Google-Registration",
      "registration=startsession; supported-alg=ES256,RS256; "
      "challenge=test_challenge;");
}

void SetValidRegistrationListHeader(TestResponseAdapter* response_adapter) {
  response_adapter->SetHeader(
      "Sec-Session-Google-Registration-List",
      "(ES256);path=\"startsession\";challenge=\"Y2hhbGxlbmdl\","
      "(ES256);path=\"startsession2\";challenge=\"Y2hhbGxlbmdlMg==\"");
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
  SetValidRegistrationHeader(&gaia_response_adapter);
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      testing::SizeIs(1));

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
  base::test::ScopedFeatureList feature_list_{
      kBoundSessionRegistrationListHeaderSupport};
  const GURL response_url("https://accounts.google.com");
  TestResponseAdapter gaia_response_adapter(response_url);
  SetValidRegistrationListHeader(&gaia_response_adapter);
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      testing::SizeIs(2));

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
  SetValidRegistrationHeader(&gaia_response_adapter);
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      testing::SizeIs(1));

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
  SetValidRegistrationHeader(&gaia_response_adapter);
  // Header itself is set correctly.
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      testing::SizeIs(1));

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
  SetValidRegistrationHeader(&response_adapter);
  ASSERT_THAT(BoundSessionRegistrationFetcherParam::CreateFromHeaders(
                  response_adapter.GetUrl(), response_adapter.GetHeaders()),
              testing::SizeIs(1));
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
  TestResponseAdapter gaia_response_adapter(
      GURL("https://accounts.google.com"));
  SetValidRegistrationHeader(&gaia_response_adapter);
  ASSERT_THAT(
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()),
      testing::SizeIs(1));

  EXPECT_CALL(*mock_service, MaybeTerminateSession).Times(0);
  EXPECT_CALL(*mock_service, CreateRegistrationRequest).Times(0);
  header_modification_delegate.ProcessResponse(&gaia_response_adapter, GURL());
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
}  // namespace
