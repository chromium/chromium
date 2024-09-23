// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"

#include "chrome/browser/enterprise/platform_auth/mock_platform_auth_provider.h"
#include "chrome/browser/enterprise/platform_auth/scoped_set_provider_for_testing.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/default_handlers.h"

using ::testing::_;

namespace enterprise_auth {

class PlatformAuthManagerBrowserTest : public InProcessBrowserTest {
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

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
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
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
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
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  ::testing::Mock::VerifyAndClearExpectations(unsafe_mock_provider);

  // The provider instance will be destroyed when `set_provider` is destroyed.
  EXPECT_CALL(*unsafe_mock_provider, Die());
}

}  // namespace enterprise_auth
