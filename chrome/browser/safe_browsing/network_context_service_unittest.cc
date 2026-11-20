// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/network_context_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_auth_preferences.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#endif

namespace safe_browsing {

class NetworkContextServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    SystemNetworkContextManager::CreateInstance(
        TestingBrowserProcess::GetGlobal()->local_state());
  }

  void TearDown() override { SystemNetworkContextManager::DeleteInstance(); }

  void SetTestCookieAndFlush(NetworkContextService& service) {
    EXPECT_NE(nullptr, service.GetURLLoaderFactory());
    network::mojom::NetworkContext* network_context =
        service.GetNetworkContext();
    ASSERT_NE(nullptr, network_context);

    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    network_context->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());
    GURL url("https://example.test");
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateForTesting(
            url, "sb_test=1; max-age=3600; Secure", base::Time::Now(),
            net::CookieSourceType::kHTTP);
    ASSERT_TRUE(cookie);
    base::test::TestFuture<net::CookieAccessResult> set_cookie_future;
    cookie_manager->SetCanonicalCookie(*cookie, url,
                                       net::CookieOptions::MakeAllInclusive(),
                                       set_cookie_future.GetCallback());
    EXPECT_TRUE(set_cookie_future.Get().status.IsInclude());

    base::test::TestFuture<void> flush_future;
    cookie_manager->FlushCookieStore(flush_future.GetCallback());
    ASSERT_TRUE(flush_future.Wait());
  }

  bool HasSafeBrowsingNetworkFilesOnDisk(
      const base::FilePath& profile_path) const {
    base::FilePath network_dir = profile_path.Append(
        base::FilePath(base::FilePath::StringType(kSafeBrowsingBaseFilename) +
                       FILE_PATH_LITERAL(" Network")));
    base::FilePath legacy_cookie_file = profile_path.Append(base::FilePath(
        base::FilePath::StringType(kSafeBrowsingBaseFilename) + kCookiesFile));
    return base::PathExists(network_dir) ||
           base::PathExists(legacy_cookie_file);
  }

  void VerifyAmbientHttpAuth(Profile* profile, bool expect_allowed) {
    NetworkContextService service(profile);
    network::mojom::NetworkContextParamsPtr params =
        service.CreateNetworkContextParams();
    ASSERT_TRUE(params);
    if (expect_allowed) {
      EXPECT_FALSE(params->http_auth_static_network_context_params);
    } else {
      ASSERT_TRUE(params->http_auth_static_network_context_params);
      EXPECT_EQ(net::HttpAuthPreferences::DISALLOW_DEFAULT_CREDENTIALS,
                params->http_auth_static_network_context_params
                    ->allow_default_credentials);
    }
    service.Shutdown();
  }

  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
#endif
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

TEST_F(NetworkContextServiceTest, RegularProfileUsesOnDiskNetworkContext) {
  TestingProfile* regular_profile =
      profile_manager_.CreateTestingProfile("regular_profile");
  NetworkContextService service(regular_profile);
  SetTestCookieAndFlush(service);
  EXPECT_TRUE(HasSafeBrowsingNetworkFilesOnDisk(regular_profile->GetPath()));
  service.Shutdown();
}

TEST_F(NetworkContextServiceTest,
       OffTheRecordProfileUsesInMemoryNetworkContext) {
  TestingProfile* regular_profile =
      profile_manager_.CreateTestingProfile("regular_profile");
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(regular_profile);
  NetworkContextService service(incognito_profile);
  SetTestCookieAndFlush(service);
  EXPECT_FALSE(HasSafeBrowsingNetworkFilesOnDisk(regular_profile->GetPath()));
  EXPECT_FALSE(HasSafeBrowsingNetworkFilesOnDisk(incognito_profile->GetPath()));
  service.Shutdown();
}

TEST_F(NetworkContextServiceTest, RegularProfileAllowsAmbientAuth) {
  TestingProfile* regular_profile =
      profile_manager_.CreateTestingProfile("regular_profile");
  VerifyAmbientHttpAuth(regular_profile, /*expect_allowed=*/true);
}

TEST_F(NetworkContextServiceTest, OffTheRecordProfileDoesNotAllowAmbientAuth) {
  TestingProfile* regular_profile =
      profile_manager_.CreateTestingProfile("regular_profile");
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(regular_profile);
  VerifyAmbientHttpAuth(incognito_profile, /*expect_allowed=*/false);
}

}  // namespace safe_browsing
