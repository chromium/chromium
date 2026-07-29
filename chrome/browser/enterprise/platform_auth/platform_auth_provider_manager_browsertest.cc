// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"

#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/platform_auth/mock_platform_auth_provider.h"
#include "chrome/browser/enterprise/platform_auth/scoped_set_provider_for_testing.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#endif
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/default_handlers.h"

using ::testing::_;

namespace enterprise_auth {

class PlatformAuthManagerBrowserTest : public PlatformBrowserTest {
 public:
  PlatformAuthManagerBrowserTest() = default;
  ~PlatformAuthManagerBrowserTest() override = default;

  PlatformAuthManagerBrowserTest(const PlatformAuthManagerBrowserTest&) =
      delete;
  PlatformAuthManagerBrowserTest& operator=(
      const PlatformAuthManagerBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(PlatformAuthManagerBrowserTest,
                       DataWithoutOriginFiltering) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Install a mock provider.
  auto mock_provider =
      std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>();
  EXPECT_CALL(*mock_provider, SupportsOriginFiltering())
      .WillOnce(::testing::Return(false));

  MockPlatformAuthProvider* unsafe_mock_provider = mock_provider.get();
  ScopedSetProviderForTesting set_provider(std::move(mock_provider));

  EXPECT_CALL(*unsafe_mock_provider, FetchOrigins(_)).Times(0);
  // Issue a request to that origin and ensure that auth data is collected.
  EXPECT_CALL(*unsafe_mock_provider, GetData(_, _))
      .WillOnce([](const GURL& url,
                   PlatformAuthProviderManager::GetDataCallback callback) {
        net::HttpRequestHeaders auth_headers;
        auth_headers.SetHeader(net::HttpRequestHeaders::kCookie,
                               "new-cookie=new-cookie-data");
        std::move(callback).Run(std::move(auth_headers));
      });

  PlatformAuthProviderManager::GetInstance().SetEnabled(true,
                                                        base::OnceClosure());

  EXPECT_TRUE(
      content::NavigateToURL(chrome_test_utils::GetActiveWebContents(this),
                             embedded_test_server()->GetURL("/empty.html")));
  ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);

  // The provider instance will be destroyed when `set_provider` is destroyed.
  EXPECT_CALL(*unsafe_mock_provider, Die());
}

IN_PROC_BROWSER_TEST_F(PlatformAuthManagerBrowserTest,
                       DataWithOriginFiltering) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a mock provider.
  auto mock_provider =
      std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>();
  EXPECT_CALL(*mock_provider, SupportsOriginFiltering())
      .WillOnce(::testing::Return(true));
  MockPlatformAuthProvider* unsafe_mock_provider = mock_provider.get();
  ScopedSetProviderForTesting set_provider(std::move(mock_provider));

  // Enable the manager with no origins configured.
  {
    EXPECT_CALL(*unsafe_mock_provider, FetchOrigins(_))
        .WillOnce([](PlatformAuthProvider::FetchOriginsCallback callback) {
          std::move(callback).Run(std::make_unique<std::vector<url::Origin>>());
        });
    base::RunLoop run_loop;
    PlatformAuthProviderManager::GetInstance().SetEnabled(
        true, run_loop.QuitClosure());
    run_loop.Run();
    ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);
  }

  // A request now should not invoke the provider for auth data.
  EXPECT_TRUE(
      content::NavigateToURL(chrome_test_utils::GetActiveWebContents(this),
                             embedded_test_server()->GetURL("/empty.html")));
  ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);

  // Configure the manager with the embedded test server as the IdP origin.
  {
    EXPECT_CALL(*unsafe_mock_provider, FetchOrigins(_))
        .WillOnce([origin = embedded_test_server()->GetOrigin()](
                      PlatformAuthProvider::FetchOriginsCallback callback) {
          std::move(callback).Run(std::make_unique<std::vector<url::Origin>>(
              std::vector<url::Origin>{origin}));
        });
    base::RunLoop run_loop;
    PlatformAuthProviderManager::GetInstance().SetEnabled(
        true, run_loop.QuitClosure());
    run_loop.Run();
    ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);
  }

  // Issue a request to that origin and ensure that auth data is collected.
  EXPECT_CALL(*unsafe_mock_provider, GetData(_, _))
      .WillOnce([](const GURL& url,
                   PlatformAuthProviderManager::GetDataCallback callback) {
        net::HttpRequestHeaders auth_headers;
        auth_headers.SetHeader(net::HttpRequestHeaders::kCookie,
                               "new-cookie=new-cookie-data");
        std::move(callback).Run(std::move(auth_headers));
      });
  EXPECT_TRUE(
      content::NavigateToURL(chrome_test_utils::GetActiveWebContents(this),
                             embedded_test_server()->GetURL("/empty.html")));
  ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);

  // The provider instance will be destroyed when `set_provider` is destroyed.
  EXPECT_CALL(*unsafe_mock_provider, Die());
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PlatformAuthManagerBrowserTest, ConcurrentNavigations) {
  base::Lock lock;
  std::map<GURL, std::string> received_cookies;

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Filter out extraneous requests (e.g., /favicon.ico) automatically
        // triggered by the browser after page load. Handling them here would
        // cause lock contention and deadlock (DCHECK failure in base::Lock)
        // against the main thread during the verification phase.
        if (request.GetURL().path() != "/title1.html" &&
            request.GetURL().path() != "/title2.html") {
          return nullptr;
        }
        base::AutoLock auto_lock(lock);
        if (request.headers.find("cookie") != request.headers.end()) {
          received_cookies[request.GetURL()] = request.headers.at("cookie");
        }
        return nullptr;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());

  auto mock_provider =
      std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>();
  EXPECT_CALL(*mock_provider, SupportsOriginFiltering())
      .WillRepeatedly(::testing::Return(false));

  base::RunLoop get_data_run_loop;
  std::map<GURL, PlatformAuthProviderManager::GetDataCallback>
      pending_callbacks;
  EXPECT_CALL(*mock_provider, GetData(_, _))
      .WillRepeatedly([&](const GURL& url,
                          PlatformAuthProviderManager::GetDataCallback cb) {
        pending_callbacks[url] = std::move(cb);
        if (pending_callbacks.size() == 2) {
          get_data_run_loop.Quit();
        }
      });

  MockPlatformAuthProvider* unsafe_mock_provider = mock_provider.get();
  ScopedSetProviderForTesting set_provider(std::move(mock_provider));

  PlatformAuthProviderManager::GetInstance().SetEnabled(true,
                                                        base::OnceClosure());

  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Start navigation in tab 1.
  content::WebContents* tab1 = chrome_test_utils::GetActiveWebContents(this);
  tab1->GetController().LoadURL(url1, content::Referrer(),
                                ui::PAGE_TRANSITION_TYPED, std::string());

  // Open a second tab and start navigation.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  content::WebContents* tab2 = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_NE(tab1, tab2);
  tab2->GetController().LoadURL(url2, content::Referrer(),
                                ui::PAGE_TRANSITION_TYPED, std::string());

  // Wait until both navigations have reached the provider's GetData().
  get_data_run_loop.Run();

  ASSERT_EQ(pending_callbacks.size(), 2u);
  ASSERT_TRUE(pending_callbacks.find(url1) != pending_callbacks.end());
  ASSERT_TRUE(pending_callbacks.find(url2) != pending_callbacks.end());

  // Resolve request 1 with token 1.
  net::HttpRequestHeaders headers1;
  headers1.SetHeader(net::HttpRequestHeaders::kCookie, "token=Token1");
  std::move(pending_callbacks[url1]).Run(std::move(headers1));

  // Resolve request 2 with token 2.
  net::HttpRequestHeaders headers2;
  headers2.SetHeader(net::HttpRequestHeaders::kCookie, "token=Token2");
  std::move(pending_callbacks[url2]).Run(std::move(headers2));

  // Wait for both tabs to finish loading.
  content::WaitForLoadStop(tab1);
  content::WaitForLoadStop(tab2);

  // Verify that each navigation only sent its own authentication headers.
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(received_cookies[url1], "token=Token1");
    EXPECT_EQ(received_cookies[url2], "token=Token2");
  }

  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  EXPECT_CALL(*unsafe_mock_provider, Die());
}
#endif

}  // namespace enterprise_auth
