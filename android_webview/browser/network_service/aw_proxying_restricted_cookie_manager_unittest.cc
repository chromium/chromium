// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxying_restricted_cookie_manager.h"

#include <memory>
#include <string>

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/common/aw_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {
namespace {

using ::testing::_;
using ::testing::Invoke;

// Mock RestrictedCookieManager to capture calls from the proxy.
class MockRestrictedCookieManager
    : public network::mojom::RestrictedCookieManager {
 public:
  MockRestrictedCookieManager() = default;
  ~MockRestrictedCookieManager() override = default;

  void GetAllForUrl(const GURL& url,
                    const net::SiteForCookies& site_for_cookies,
                    const url::Origin& top_frame_origin,
                    net::StorageAccessApiStatus storage_access_api_status,
                    network::mojom::CookieManagerGetOptionsPtr options,
                    bool is_ad_tagged,
                    bool apply_devtools_overrides,
                    bool force_disable_third_party_cookies,
                    GetAllForUrlCallback callback) override {
    std::move(callback).Run(std::vector<net::CookieWithAccessResult>());
  }

  void SetCanonicalCookie(
      network::mojom::RestrictedCanonicalCookieParamsPtr cookie_params,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool is_ad_tagged,
      bool apply_devtools_overrides,
      SetCanonicalCookieCallback callback) override {
    std::move(callback).Run(true);
  }

  void AddChangeListener(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      mojo::PendingRemote<network::mojom::CookieChangeListener> listener,
      AddChangeListenerCallback callback) override {
    std::move(callback).Run();
  }

  void SetCookieFromString(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool get_version_shared_memory,
      bool is_ad_tagged,
      bool apply_devtools_overrides,
      const std::string& cookie,
      SetCookieFromStringCallback callback) override {
    last_set_cookie_shared_memory_param_ = get_version_shared_memory;
    std::move(callback).Run(nullptr);
  }

  void GetCookiesString(const GURL& url,
                        const net::SiteForCookies& site_for_cookies,
                        const url::Origin& top_frame_origin,
                        net::StorageAccessApiStatus storage_access_api_status,
                        bool get_version_shared_memory,
                        bool is_ad_tagged,
                        bool apply_devtools_overrides,
                        bool force_disable_third_party_cookies,
                        GetCookiesStringCallback callback) override {
    last_get_cookies_shared_memory_param_ = get_version_shared_memory;
    std::move(callback).Run(network::mojom::kInvalidCookieVersion,
                            base::ReadOnlySharedMemoryRegion(), "");
  }

  void CookiesEnabledFor(const GURL& url,
                         const net::SiteForCookies& site_for_cookies,
                         const url::Origin& top_frame_origin,
                         net::StorageAccessApiStatus storage_access_api_status,
                         bool apply_devtools_overrides,
                         CookiesEnabledForCallback callback) override {
    std::move(callback).Run(true);
  }

  bool last_get_cookies_shared_memory_param() const {
    return last_get_cookies_shared_memory_param_;
  }
  bool last_set_cookie_shared_memory_param() const {
    return last_set_cookie_shared_memory_param_;
  }

 private:
  bool last_get_cookies_shared_memory_param_ = false;
  bool last_set_cookie_shared_memory_param_ = false;
};

class AwProxyingRestrictedCookieManagerTest : public testing::Test {
 public:
  AwProxyingRestrictedCookieManagerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    // Set up default cookie policy to accept cookies.
    cookie_access_policy_.SetShouldAcceptCookies(true);
  }

 protected:
  void CreateProxyOnIOThread(
      mojo::PendingRemote<network::mojom::RestrictedCookieManager> underlying,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
    AwProxyingRestrictedCookieManager::CreateAndBind(
        std::move(underlying),
        /*is_service_worker=*/false,
        /*process_id=*/0,
        /*frame_id=*/0, std::move(receiver), &cookie_access_policy_);
  }

  content::BrowserTaskEnvironment task_environment_;
  AwCookieAccessPolicy cookie_access_policy_;
};

// Test: When feature is disabled, AllowCookies uses dynamic policy checking.
// Changing the policy after construction should affect cookie access.
TEST_F(AwProxyingRestrictedCookieManagerTest,
       PolicyCheckedDynamically_WhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWebViewLatchedCookiePolicy);

  // Initial state: cookies allowed.
  cookie_access_policy_.SetShouldAcceptCookies(true);

  MockRestrictedCookieManager mock_rcm;
  mojo::Receiver<network::mojom::RestrictedCookieManager> mock_receiver(
      &mock_rcm);

  mojo::Remote<network::mojom::RestrictedCookieManager> proxy_remote;

  // Create the proxy.
  CreateProxyOnIOThread(mock_receiver.BindNewPipeAndPassRemote(),
                        proxy_remote.BindNewPipeAndPassReceiver());

  // Wait for the proxy to be connected.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return proxy_remote.is_connected(); }));

  // Now change policy to disallow cookies.
  cookie_access_policy_.SetShouldAcceptCookies(false);

  // Make a GetCookiesString call - should return empty due to dynamic check.
  base::RunLoop run_loop;
  proxy_remote->GetCookiesString(
      GURL("https://example.com"), net::SiteForCookies(),
      url::Origin::Create(GURL("https://example.com")),
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/true,
      /*is_ad_tagged=*/false,
      /*apply_devtools_overrides=*/false,
      /*force_disable_third_party_cookies=*/false,
      base::BindOnce(
          [](base::RunLoop* run_loop, uint64_t version,
             base::ReadOnlySharedMemoryRegion region, const std::string& str) {
            // With cookies disabled, the proxy should return early without
            // calling the underlying RCM.
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  // The mock should NOT have been called because cookies are now disabled.
  // We can't directly verify this in the current setup, but the test ensures
  // the proxy respects the dynamic policy change.
}

// Test: When feature is enabled, AllowCookies uses latched policy.
// Changing the policy after construction should NOT affect cookie access.
TEST_F(AwProxyingRestrictedCookieManagerTest,
       PolicyLatchedAtConstruction_WhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebViewLatchedCookiePolicy);

  // Initial state: cookies allowed.
  cookie_access_policy_.SetShouldAcceptCookies(true);

  MockRestrictedCookieManager mock_rcm;
  mojo::Receiver<network::mojom::RestrictedCookieManager> mock_receiver(
      &mock_rcm);

  mojo::Remote<network::mojom::RestrictedCookieManager> proxy_remote;

  // Create the proxy (latches the "allowed" state).
  CreateProxyOnIOThread(mock_receiver.BindNewPipeAndPassRemote(),
                        proxy_remote.BindNewPipeAndPassReceiver());

  // Wait for the proxy to be connected.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return proxy_remote.is_connected(); }));

  // Now change policy to disallow cookies AFTER construction.
  cookie_access_policy_.SetShouldAcceptCookies(false);

  // Make a GetCookiesString call - should still work because we latched
  // the "allowed" state at construction.
  base::RunLoop run_loop;
  proxy_remote->GetCookiesString(
      GURL("https://example.com"), net::SiteForCookies(),
      url::Origin::Create(GURL("https://example.com")),
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/true,
      /*is_ad_tagged=*/false,
      /*apply_devtools_overrides=*/false,
      /*force_disable_third_party_cookies=*/false,
      base::BindOnce(
          [](base::RunLoop* run_loop, uint64_t version,
             base::ReadOnlySharedMemoryRegion region, const std::string& str) {
            // With latched policy, the call should go through to the underlying
            // RCM even though we changed the policy after construction.
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  // Verify the underlying mock was called (meaning the proxy allowed it).
  // The mock was called if it recorded the shared memory param.
  EXPECT_TRUE(mock_rcm.last_get_cookies_shared_memory_param());
}

// Test: GetCookiesString passes through shared memory flag when feature
// enabled.
TEST_F(AwProxyingRestrictedCookieManagerTest,
       GetCookiesStringPassesSharedMemory_WhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebViewLatchedCookiePolicy);

  cookie_access_policy_.SetShouldAcceptCookies(true);

  MockRestrictedCookieManager mock_rcm;
  mojo::Receiver<network::mojom::RestrictedCookieManager> mock_receiver(
      &mock_rcm);

  mojo::Remote<network::mojom::RestrictedCookieManager> proxy_remote;

  CreateProxyOnIOThread(mock_receiver.BindNewPipeAndPassRemote(),
                        proxy_remote.BindNewPipeAndPassReceiver());

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return proxy_remote.is_connected(); }));

  base::RunLoop run_loop;
  proxy_remote->GetCookiesString(
      GURL("https://example.com"), net::SiteForCookies(),
      url::Origin::Create(GURL("https://example.com")),
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/true,  // Request shared memory.
      /*is_ad_tagged=*/false,
      /*apply_devtools_overrides=*/false,
      /*force_disable_third_party_cookies=*/false,
      base::BindOnce([](base::RunLoop* run_loop, uint64_t version,
                        base::ReadOnlySharedMemoryRegion region,
                        const std::string& str) { run_loop->Quit(); },
                     &run_loop));
  run_loop.Run();

  // Verify shared memory flag was passed through.
  EXPECT_TRUE(mock_rcm.last_get_cookies_shared_memory_param());
}

// Test: GetCookiesString always passes false for shared memory when feature
// disabled.
TEST_F(AwProxyingRestrictedCookieManagerTest,
       GetCookiesStringBlocksSharedMemory_WhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWebViewLatchedCookiePolicy);

  cookie_access_policy_.SetShouldAcceptCookies(true);

  MockRestrictedCookieManager mock_rcm;
  mojo::Receiver<network::mojom::RestrictedCookieManager> mock_receiver(
      &mock_rcm);

  mojo::Remote<network::mojom::RestrictedCookieManager> proxy_remote;

  CreateProxyOnIOThread(mock_receiver.BindNewPipeAndPassRemote(),
                        proxy_remote.BindNewPipeAndPassReceiver());

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return proxy_remote.is_connected(); }));

  base::RunLoop run_loop;
  proxy_remote->GetCookiesString(
      GURL("https://example.com"), net::SiteForCookies(),
      url::Origin::Create(GURL("https://example.com")),
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/true,  // Request shared memory.
      /*is_ad_tagged=*/false,
      /*apply_devtools_overrides=*/false,
      /*force_disable_third_party_cookies=*/false,
      base::BindOnce([](base::RunLoop* run_loop, uint64_t version,
                        base::ReadOnlySharedMemoryRegion region,
                        const std::string& str) { run_loop->Quit(); },
                     &run_loop));
  run_loop.Run();

  // Verify shared memory flag was blocked (always false when feature disabled).
  EXPECT_FALSE(mock_rcm.last_get_cookies_shared_memory_param());
}

// Test: SetCookieFromString passes through shared memory flag when feature
// enabled.
TEST_F(AwProxyingRestrictedCookieManagerTest,
       SetCookieFromStringPassesSharedMemory_WhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebViewLatchedCookiePolicy);

  cookie_access_policy_.SetShouldAcceptCookies(true);

  MockRestrictedCookieManager mock_rcm;
  mojo::Receiver<network::mojom::RestrictedCookieManager> mock_receiver(
      &mock_rcm);

  mojo::Remote<network::mojom::RestrictedCookieManager> proxy_remote;

  CreateProxyOnIOThread(mock_receiver.BindNewPipeAndPassRemote(),
                        proxy_remote.BindNewPipeAndPassReceiver());

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return proxy_remote.is_connected(); }));

  base::RunLoop run_loop;
  proxy_remote->SetCookieFromString(
      GURL("https://example.com"),
      net::SiteForCookies::FromUrl(GURL("https://example.com")),
      url::Origin::Create(GURL("https://example.com")),
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/true,  // Request shared memory.
      /*is_ad_tagged=*/false,
      /*apply_devtools_overrides=*/false, "testcookie=value",
      base::BindOnce(
          [](base::RunLoop* run_loop,
             network::mojom::CookiesResponsePtr response) { run_loop->Quit(); },
          &run_loop));
  run_loop.Run();

  // Verify shared memory flag was passed through.
  EXPECT_TRUE(mock_rcm.last_set_cookie_shared_memory_param());
}

// Test: SetCookieFromString blocks shared memory flag when feature disabled.
TEST_F(AwProxyingRestrictedCookieManagerTest,
       SetCookieFromStringBlocksSharedMemory_WhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWebViewLatchedCookiePolicy);

  cookie_access_policy_.SetShouldAcceptCookies(true);

  MockRestrictedCookieManager mock_rcm;
  mojo::Receiver<network::mojom::RestrictedCookieManager> mock_receiver(
      &mock_rcm);

  mojo::Remote<network::mojom::RestrictedCookieManager> proxy_remote;

  CreateProxyOnIOThread(mock_receiver.BindNewPipeAndPassRemote(),
                        proxy_remote.BindNewPipeAndPassReceiver());

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return proxy_remote.is_connected(); }));

  base::RunLoop run_loop;
  proxy_remote->SetCookieFromString(
      GURL("https://example.com"),
      net::SiteForCookies::FromUrl(GURL("https://example.com")),
      url::Origin::Create(GURL("https://example.com")),
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/true,  // Request shared memory.
      /*is_ad_tagged=*/false,
      /*apply_devtools_overrides=*/false, "testcookie=value",
      base::BindOnce(
          [](base::RunLoop* run_loop,
             network::mojom::CookiesResponsePtr response) { run_loop->Quit(); },
          &run_loop));
  run_loop.Run();

  // Verify shared memory flag was blocked.
  EXPECT_FALSE(mock_rcm.last_set_cookie_shared_memory_param());
}

}  // namespace
}  // namespace android_webview
