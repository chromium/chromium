// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/header_modification_delegate_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
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
              (const net::HttpResponseHeaders* headers),
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
  MOCK_METHOD(chrome::mojom::BoundSessionThrottlerParamsPtr,
              GetBoundSessionThrottlerParams,
              (),
              (const, override));
  MOCK_METHOD(
      void,
      SetRendererBoundSessionThrottlerParamsUpdaterDelegate,
      (RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater),
      (override));
  MOCK_METHOD(void,
              OnRequestBlockedOnCookie,
              (OnRequestBlockedOnCookieCallback resume_blocked_request),
              (override));
  MOCK_METHOD(base::WeakPtr<BoundSessionCookieRefreshService>,
              GetWeakPtr,
              (),
              (override));
};

class TestResponseAdapter : public signin::ResponseAdapter {
 public:
  explicit TestResponseAdapter(const GURL& url)
      : headers_(new net::HttpResponseHeaders(std::string())), url_(url) {}

  TestResponseAdapter(const TestResponseAdapter&) = delete;
  TestResponseAdapter& operator=(const TestResponseAdapter&) = delete;

  ~TestResponseAdapter() override = default;

  void SetHeader(const std::string& header_name,
                 const std::string& header_value) {
    headers_->SetHeader(header_name, header_value);
  }

  content::WebContents::Getter GetWebContentsGetter() const override {
    return base::BindRepeating(
        []() -> content::WebContents* { return nullptr; });
  }

  GURL GetUrl() const override { return url_; }

  bool IsOutermostMainFrame() const override { return true; }

  absl::optional<url::Origin> GetRequestInitiator() const override {
    // Pretend the request came from the same origin.
    return url::Origin::Create(GetUrl());
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
  TestResponseAdapter gaia_response_adapter(
      GURL("https://accounts.google.com"));
  SetValidRegistrationHeader(&gaia_response_adapter);
  ASSERT_TRUE(BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
      gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()));

  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, MaybeTerminateSession(_));
  EXPECT_CALL(*mock_service, CreateRegistrationRequest(_));
  header_modification_delegate().ProcessResponse(&gaia_response_adapter,
                                                 GURL());
}

TEST_F(BoundSessionHeaderModificationDelegateImplTest, NonGaiaResponse) {
  TestResponseAdapter response_adapter(GURL("https://google.com"));
  SetValidRegistrationHeader(&response_adapter);
  ASSERT_TRUE(BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
      response_adapter.GetUrl(), response_adapter.GetHeaders()));
  MockBoundSessionCookieRefreshService* mock_service =
      GetMockBoundSessionCookieRefreshService(testing_profile());
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, MaybeTerminateSession(_)).Times(0);
  EXPECT_CALL(*mock_service, CreateRegistrationRequest(_)).Times(0);
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
  ASSERT_TRUE(BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
      gaia_response_adapter.GetUrl(), gaia_response_adapter.GetHeaders()));

  EXPECT_CALL(*mock_service, MaybeTerminateSession(_)).Times(0);
  EXPECT_CALL(*mock_service, CreateRegistrationRequest(_)).Times(0);
  header_modification_delegate.ProcessResponse(&gaia_response_adapter, GURL());
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
}  // namespace
