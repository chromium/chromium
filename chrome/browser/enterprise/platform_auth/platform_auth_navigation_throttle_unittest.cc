// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_navigation_throttle.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/platform_auth/mock_platform_auth_provider.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/browser/enterprise/platform_auth/scoped_set_provider_for_testing.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationThrottle;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

void EnableManager(enterprise_auth::PlatformAuthProviderManager& manager,
                   bool enabled) {
  base::MockCallback<base::OnceClosure> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run()).WillOnce([&run_loop]() {
    run_loop.QuitWhenIdle();
  });
  manager.SetEnabled(enabled, callback.Get());
  EXPECT_EQ(manager.IsEnabled(), enabled);
  run_loop.Run();
  EXPECT_EQ(manager.IsEnabled(), enabled);
}

}  // namespace

namespace enterprise_auth {

class PlatformAuthNavigationThrottleTest : public testing::Test {
 public:
  PlatformAuthNavigationThrottleTest() : mock_provider_(owned_provider_.get()) {
    // Set up a default that will clear the raw pointer upon destruction.
    ON_CALL(*mock_provider_, Die()).WillByDefault([this]() {
      this->mock_provider_ = nullptr;
    });
    // Expect the provider to be destroyed at some point.
    EXPECT_CALL(*mock_provider_, Die());
  }

  PlatformAuthProviderManager& manager() {
    return PlatformAuthProviderManager::GetInstance();
  }

  std::unique_ptr<PlatformAuthNavigationThrottle> CreateThrottle(
      content::NavigationHandle* navigation_handle) {
    return std::make_unique<PlatformAuthNavigationThrottle>(navigation_handle);
  }

  content::WebContents* web_contents() const { return web_contents_.get(); }
  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  std::unique_ptr<PlatformAuthProvider> TakeProvider() {
    return std::move(owned_provider_);
  }

  virtual void SetupOriginFilteringExpectations() {
    EXPECT_CALL(*mock_provider_, SupportsOriginFiltering())
        .WillOnce(::testing::Return(true));
  }

  ::testing::StrictMock<MockPlatformAuthProvider>* mock_provider() {
    return mock_provider_;
  }

 protected:
  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SetupOriginFilteringExpectations();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<::testing::StrictMock<MockPlatformAuthProvider>>
      owned_provider_{
          std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>()};
  raw_ptr<::testing::StrictMock<MockPlatformAuthProvider>> mock_provider_ =
      nullptr;
};

// The manager is disabled, so no origins or data are fetched.
TEST_F(PlatformAuthNavigationThrottleTest, ManagerDisabled) {
  EXPECT_CALL(*mock_provider(), FetchOrigins(_)).Times(0);
  ScopedSetProviderForTesting set_provider(TakeProvider());

  content::MockNavigationHandle test_handle(GURL("https://www.example.test/"),
                                            main_frame());
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_CALL(test_handle, SetAllowCookiesFromBrowser(false));
  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);

  EXPECT_FALSE(manager().IsEnabled());
  EXPECT_EQ(manager().GetOriginsForTesting(), std::vector<url::Origin>());
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  EXPECT_STREQ("PlatformAuthNavigationThrottle", throttle->GetNameForLogging());
}

// The manager is enabled, so an origin fetch happens. No data is fetched when
// an empty set of origins is returned.
TEST_F(PlatformAuthNavigationThrottleTest, EmptyOrigins) {
  ScopedSetProviderForTesting set_provider(TakeProvider());

  EXPECT_CALL(*mock_provider(), FetchOrigins(_))
      .WillOnce([](PlatformAuthProvider::FetchOriginsCallback callback) {
        std::move(callback).Run(nullptr);
      });
  EnableManager(manager(), true);
  EXPECT_TRUE(manager().IsEnabled());

  content::MockNavigationHandle test_handle(GURL("https://www.example.test/"),
                                            main_frame());
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_CALL(test_handle, SetAllowCookiesFromBrowser(true));
  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);

  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
  EXPECT_EQ(manager().GetOriginsForTesting(), std::vector<url::Origin>());
}

// The manager is enabled and a set of origins is returned, so the throttle
// tries to fetch data. The fetched data is empty, so the throttle does not
// attach any headers to the request.
TEST_F(PlatformAuthNavigationThrottleTest, EmptyData) {
  auto url = GURL("https://www.example.test/");
  ScopedSetProviderForTesting set_provider(TakeProvider());
  EXPECT_CALL(*mock_provider(), FetchOrigins(_))
      .WillOnce([&url](PlatformAuthProvider::FetchOriginsCallback callback) {
        std::move(callback).Run(std::make_unique<std::vector<url::Origin>>(
            std::vector<url::Origin>{url::Origin::Create(url)}));
      });
  EnableManager(manager(), true);
  EXPECT_TRUE(manager().IsEnabled());

  content::MockNavigationHandle test_handle(url, main_frame());
  auto throttle = CreateThrottle(&test_handle);
  EXPECT_CALL(test_handle, SetAllowCookiesFromBrowser(true));
  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);

  // The provider returns an empty set of authentication headers.
  EXPECT_CALL(*mock_provider(), GetData(_, _))
      .WillOnce([](const GURL& url,
                   PlatformAuthProviderManager::GetDataCallback callback) {
        std::move(callback).Run(net::HttpRequestHeaders());
      });

  EXPECT_EQ(manager().GetOriginsForTesting(),
            std::vector<url::Origin>({url::Origin::Create(url)}));
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

// The manager is enabled and a set of origins is returned, so the throttle
// fetches data and attaches all received headers to the request. On redirect to
// another origin, all previously added headers are removed from the request.
TEST_F(PlatformAuthNavigationThrottleTest, DataReceived) {
  const char kOldCookieValue[] = "old-cookie=old-data";
  const char kCookieValue[] = "cookie-1=cookie-1-data; cookie-2=cookie-2-data";
  const char kOldHeaderName[] = "old-header";
  const char kOldHeaderValue[] = "old-header-data";
  const char kHeader1Name[] = "header-1";
  const char kHeader1Value[] = "header-1-data";
  const char kHeader2Name[] = "header-2";
  const char kHeader2Value[] = "header-2-data";

  auto url = GURL("https://www.example.test/");
  ScopedSetProviderForTesting set_provider(TakeProvider());
  EXPECT_CALL(*mock_provider(), FetchOrigins(_))
      .WillOnce([&url](PlatformAuthProvider::FetchOriginsCallback callback) {
        std::move(callback).Run(std::make_unique<std::vector<url::Origin>>(
            std::vector<url::Origin>{url::Origin::Create(url)}));
      });
  EnableManager(manager(), true);
  EXPECT_TRUE(manager().IsEnabled());

  content::MockNavigationHandle test_handle(url, main_frame());
  // Set some existing request headers.
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kCookie,
                    base::JoinString({kOldCookieValue, kCookieValue}, "; "));
  headers.SetHeader(kOldHeaderName, kOldHeaderValue);
  headers.SetHeader(kHeader1Name, kHeader1Value);
  test_handle.set_request_headers(std::move(headers));

  auto throttle = CreateThrottle(&test_handle);
  throttle->set_resume_callback_for_testing(base::DoNothing());

  // Headers are added to the request whose origin matches the origin stored in
  // the manager.
  EXPECT_CALL(test_handle, SetAllowCookiesFromBrowser(true));
  EXPECT_CALL(test_handle,
              SetRequestHeader(net::HttpRequestHeaders::kCookie, kCookieValue));
  EXPECT_CALL(test_handle, SetRequestHeader(kHeader1Name, kHeader1Value));
  EXPECT_CALL(test_handle, SetRequestHeader(kHeader2Name, kHeader2Value));

  // The provider returns a non-empty set of authentication headers.
  EXPECT_CALL(*mock_provider(), GetData(_, _))
      .WillOnce([&kCookieValue, &kHeader1Name, &kHeader1Value, &kHeader2Name,
                 &kHeader2Value](
                    const GURL& url,
                    PlatformAuthProviderManager::GetDataCallback callback) {
        net::HttpRequestHeaders auth_headers;
        auth_headers.SetHeader(net::HttpRequestHeaders::kCookie, kCookieValue);
        auth_headers.SetHeader(kHeader1Name, kHeader1Value);
        auth_headers.SetHeader(kHeader2Name, kHeader2Value);
        base::ThreadPool::PostTask(base::BindOnce(
            [](net::HttpRequestHeaders auth_headers,
               PlatformAuthProviderManager::GetDataCallback callback) {
              // Simulates a data fetch delay to cause the throttle to be
              // deferred.
              base::PlatformThread::Sleep(base::Milliseconds(10));
              std::move(callback).Run(std::move(auth_headers));
            },
            std::move(auth_headers), std::move(callback)));
      });

  EXPECT_EQ(manager().GetOriginsForTesting(),
            std::vector<url::Origin>({url::Origin::Create(url)}));
  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());

  // Ensure the async data fetch completes.
  task_environment_.RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(&test_handle);

  // The redirect is to another origin, so cookies and headers set by the
  // throttle are removed.
  EXPECT_CALL(test_handle,
              RemoveRequestHeader(net::HttpRequestHeaders::kCookie));
  EXPECT_CALL(test_handle, RemoveRequestHeader(kHeader1Name));
  EXPECT_CALL(test_handle, RemoveRequestHeader(kHeader2Name));
  auto redirect_url = GURL("https://www.redirect.test/");
  test_handle.set_url(redirect_url);
  EXPECT_EQ(NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest().action());
}

class PlatformAuthNavigationNoOriginFilteringThrottleTest
    : public PlatformAuthNavigationThrottleTest {
  void SetupOriginFilteringExpectations() override {
    EXPECT_CALL(*mock_provider_, SupportsOriginFiltering())
        .WillOnce(::testing::Return(false));
  }
};

// The manager is enabled and a set of origins is returned, so the throttle
// fetches data and attaches all received headers to the request. On redirect to
// another origin, all previously added headers are removed from the request.
TEST_F(PlatformAuthNavigationNoOriginFilteringThrottleTest,
       DataReceivedOriginFiltering) {
  const char kOldCookieValue[] = "old-cookie=old-data";
  const char kCookieValue[] = "cookie-1=cookie-1-data; cookie-2=cookie-2-data";
  const char kOldHeaderName[] = "old-header";
  const char kOldHeaderValue[] = "old-header-data";
  const char kHeader1Name[] = "header-1";
  const char kHeader1Value[] = "header-1-data";
  const char kHeader2Name[] = "header-2";
  const char kHeader2Value[] = "header-2-data";

  auto url = GURL("https://www.example.test/");
  ScopedSetProviderForTesting set_provider(TakeProvider());
  EXPECT_CALL(*mock_provider(), FetchOrigins(_)).Times(0);
  EnableManager(manager(), true);
  EXPECT_TRUE(manager().IsEnabled());

  content::MockNavigationHandle test_handle(url, main_frame());
  // Set some existing request headers.
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kCookie,
                    base::JoinString({kOldCookieValue, kCookieValue}, "; "));
  headers.SetHeader(kOldHeaderName, kOldHeaderValue);
  headers.SetHeader(kHeader1Name, kHeader1Value);
  test_handle.set_request_headers(std::move(headers));

  auto throttle = CreateThrottle(&test_handle);
  throttle->set_resume_callback_for_testing(base::DoNothing());

  // Headers are added to the request whose origin matches the origin stored in
  // the manager.
  EXPECT_CALL(test_handle, SetAllowCookiesFromBrowser(true));
  EXPECT_CALL(test_handle,
              SetRequestHeader(net::HttpRequestHeaders::kCookie, kCookieValue));
  EXPECT_CALL(test_handle, SetRequestHeader(kHeader1Name, kHeader1Value));
  EXPECT_CALL(test_handle, SetRequestHeader(kHeader2Name, kHeader2Value));

  // The provider returns a non-empty set of authentication headers.
  EXPECT_CALL(*mock_provider(), GetData(_, _))
      .WillOnce([&kCookieValue, &kHeader1Name, &kHeader1Value, &kHeader2Name,
                 &kHeader2Value](
                    const GURL& url,
                    PlatformAuthProviderManager::GetDataCallback callback) {
        net::HttpRequestHeaders auth_headers;
        auth_headers.SetHeader(net::HttpRequestHeaders::kCookie, kCookieValue);
        auth_headers.SetHeader(kHeader1Name, kHeader1Value);
        auth_headers.SetHeader(kHeader2Name, kHeader2Value);
        base::ThreadPool::PostTask(base::BindOnce(
            [](net::HttpRequestHeaders auth_headers,
               PlatformAuthProviderManager::GetDataCallback callback) {
              // Simulates a data fetch delay to cause the throttle to be
              // deferred.
              base::PlatformThread::Sleep(base::Milliseconds(10));
              std::move(callback).Run(std::move(auth_headers));
            },
            std::move(auth_headers), std::move(callback)));
      });

  EXPECT_EQ(manager().GetOriginsForTesting(), std::vector<url::Origin>());
  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());

  // Ensure the async data fetch completes.
  task_environment_.RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(&test_handle);

  // The redirect is to another origin, so cookies and headers set by the
  // throttle are removed.
  EXPECT_CALL(test_handle,
              RemoveRequestHeader(net::HttpRequestHeaders::kCookie));
  EXPECT_CALL(test_handle, RemoveRequestHeader(kHeader1Name));
  EXPECT_CALL(test_handle, RemoveRequestHeader(kHeader2Name));

  // The provider returns an empty set of authentication headers.
  EXPECT_CALL(*mock_provider(), GetData(_, _))
      .WillOnce([](const GURL& url,
                   PlatformAuthProviderManager::GetDataCallback callback) {
        std::move(callback).Run(net::HttpRequestHeaders());
      });

  auto redirect_url = GURL("https://www.redirect.test/");
  test_handle.set_url(redirect_url);
  EXPECT_EQ(NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest().action());
}

}  // namespace enterprise_auth
