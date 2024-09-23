// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/os_crypt/async/common/encryptor_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/features.h"
#endif

namespace {

constexpr char kCookieName[] = "Name";
constexpr char kCookieValue[] = "Value";

net::CookieList GetCookies(network::mojom::CookieManager* cookie_manager) {
  base::RunLoop run_loop;
  net::CookieList cookies_out;
  cookie_manager->GetAllCookies(
      base::BindLambdaForTesting([&](const net::CookieList& cookies) {
        cookies_out = cookies;
        run_loop.Quit();
      }));
  run_loop.Run();
  return cookies_out;
}

void SetCookie(network::mojom::CookieManager* cookie_manager) {
  base::Time t = base::Time::Now();
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      kCookieName, kCookieValue, "www.test.com", "/", t, t + base::Days(1),
      base::Time(), base::Time(), /*secure=*/true, /*http-only=*/false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      net::CookieOptions(),
      base::BindLambdaForTesting(
          [&](net::CookieAccessResult result) { run_loop.Quit(); }));
  run_loop.Run();
}

void FlushCookies(network::mojom::CookieManager* cookie_manager) {
  base::RunLoop run_loop;
  cookie_manager->FlushCookieStore(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();
}

// See |NetworkServiceBrowserTest| for content's version of tests.
class ChromeNetworkServiceBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple</*kNetworkServiceInProcess*/ bool,
                     /*kProtectEncryptionKey*/ bool>> {
 public:
  ChromeNetworkServiceBrowserTest() {
    // Verify that cookie encryption works both in-process and out of process.
    if (std::get<0>(GetParam())) {
      content::ForceInProcessNetworkService();
    } else {
      content::ForceOutOfProcessNetworkService();
    }
#if BUILDFLAG(IS_WIN)
    // Key protection only supported on Windows.
    scoped_feature_list_.InitWithFeatureState(
        os_crypt_async::features::kProtectEncryptionKey,
        std::get<1>(GetParam()));
#endif  // BUILDFLAG(IS_WIN)
  }

  ChromeNetworkServiceBrowserTest(const ChromeNetworkServiceBrowserTest&) =
      delete;
  ChromeNetworkServiceBrowserTest& operator=(
      const ChromeNetworkServiceBrowserTest&) = delete;

 protected:
  mojo::PendingRemote<network::mojom::NetworkContext> CreateNetworkContext(
      bool enable_encrypted_cookies) {
    mojo::PendingRemote<network::mojom::NetworkContext> network_context;
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    context_params->enable_encrypted_cookies = enable_encrypted_cookies;
    // Mirrors behavior in
    // ProfileNetworkContextService::ConfigureNetworkContextParamsInternal into
    // this test.
    g_browser_process->system_network_context_manager()
        ->AddCookieEncryptionManagerToNetworkContextParams(
            context_params.get());
    context_params->file_paths = network::mojom::NetworkContextFilePaths::New();

    // Network files for the test context need to differ from the ones created
    // for the current profile.
    base::FilePath data_path =
        g_browser_process->profile_manager()->user_data_dir().AppendASCII(
            "Test Context");
    context_params->file_paths->unsandboxed_data_path = data_path;
    context_params->file_paths->data_directory =
        data_path.Append(chrome::kNetworkDataDirname);
    context_params->file_paths->trigger_migration = true;
    context_params->file_paths->cookie_database_name =
        base::FilePath(chrome::kCookieFilename);
    context_params->cert_verifier_params = content::GetCertVerifierParams(
        cert_verifier::mojom::CertVerifierCreationParams::New());
    content::CreateNetworkContextInNetworkService(
        network_context.InitWithNewPipeAndPassReceiver(),
        std::move(context_params));
    return network_context;
  }

#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

IN_PROC_BROWSER_TEST_P(ChromeNetworkServiceBrowserTest,
                       PRE_PRE_EncryptedCookies) {
  // These test is only valid if crypto is enabled on the platform.
  auto crypto_delegate = cookie_config::GetCookieCryptoDelegate();
  if (!crypto_delegate) {
    GTEST_SKIP() << "No crypto on this platform.";
  }
  std::string ciphertext;
  crypto_delegate->EncryptString(kCookieValue, &ciphertext);
  ASSERT_NE(ciphertext, kCookieValue) << "Crypto should really encrypt.";

  // First set a cookie with cookie encryption enabled.
  mojo::Remote<network::mojom::NetworkContext> context(
      CreateNetworkContext(/*enable_encrypted_cookies=*/true));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  context->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());

  SetCookie(cookie_manager.get());

  net::CookieList cookies = GetCookies(cookie_manager.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());

  FlushCookies(cookie_manager.get());
}

IN_PROC_BROWSER_TEST_P(ChromeNetworkServiceBrowserTest, PRE_EncryptedCookies) {
  // Now attempt to read the cookie with encryption still enabled.
  mojo::Remote<network::mojom::NetworkContext> context(
      CreateNetworkContext(/*enable_encrypted_cookies=*/true));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  context->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());

  net::CookieList cookies = GetCookies(cookie_manager.get());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());
}

IN_PROC_BROWSER_TEST_P(ChromeNetworkServiceBrowserTest, EncryptedCookies) {
  // Now attempt to read the cookie again, but this time with encryption
  // disabled.
  mojo::Remote<network::mojom::NetworkContext> context(
      CreateNetworkContext(/*enable_encrypted_cookies=*/false));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  context->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());

  net::CookieList cookies = GetCookies(cookie_manager.get());
  ASSERT_TRUE(cookies.empty());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeNetworkServiceBrowserTest,
    ::testing::Combine(testing::Bool(),
                       testing::Values(false
#if BUILDFLAG(IS_WIN)
                                       ,
                                       true
#endif
                                       )),
    [](const auto& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "InProcess_" : "OutOfProcess_",
           std::get<1>(info.param) ? "ProtectOn" : "ProtectOff"});
    });

#if BUILDFLAG(IS_WIN)
class ChromeNetworkServiceBrowserCookieLockTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kLockProfileCookieDatabase, ShouldBeLocked());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  const bool& ShouldBeLocked() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test verifies that if the kLockProfileCookieDatabase feature is enabled,
// then the cookie store cannot be opened once sqlite has an exclusive lock on
// the file.
IN_PROC_BROWSER_TEST_P(ChromeNetworkServiceBrowserCookieLockTest,
                       CookiesAreLocked) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  base::FilePath cookie_filename = browser()
                                       ->profile()
                                       ->GetPath()
                                       .Append(chrome::kNetworkDataDirname)
                                       .Append(chrome::kCookieFilename);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    ASSERT_TRUE(base::PathExists(cookie_filename));
    base::File cookie_file(
        cookie_filename,
        base::File::Flags::FLAG_OPEN_ALWAYS | base::File::Flags::FLAG_READ);
    EXPECT_EQ(ShouldBeLocked(), !cookie_file.IsValid());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeNetworkServiceBrowserCookieLockTest,
                         ::testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "Locked" : "NotLocked";
                         });

#endif  // BUILDFLAG(IS_WIN)

// See `NetworkServiceBrowserTest` for content's version of tests. This test
// merely tests that chrome's feature is wired up correctly to the migration
// code that exists in content.
class ChromeNetworkServiceMigrationBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNetworkServiceMigrationBrowserTest() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> disabled_features, enabled_features;
#if BUILDFLAG(IS_WIN)
    // On Windows, the Network Sandbox requires that data migration be enabled
    // to function correctly. Thus, in order to correctly test the case when
    // network data migration is not happening, the network sandbox must also be
    // disabled.
    disabled_features.push_back(
        sandbox::policy::features::kNetworkServiceSandbox);
#endif
    // For PRE_PRE, disable migration. For PRE_ enable it, and then disable it
    // again.
    if (GetTestPreCount() == 2 || GetTestPreCount() == 0)
      disabled_features.push_back(features::kTriggerNetworkDataMigration);
    else
      enabled_features.push_back(features::kTriggerNetworkDataMigration);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void VerifyCookiePresent() {
    auto* cookie_manager = browser()
                               ->profile()
                               ->GetDefaultStoragePartition()
                               ->GetCookieManagerForBrowserProcess();
    auto cookies = GetCookies(cookie_manager);
    ASSERT_EQ(1u, cookies.size());
    EXPECT_EQ("name", cookies[0].Name());
    EXPECT_EQ("Good", cookies[0].Value());
  }

  base::FilePath GetOldCookieLocation() {
    return browser()->profile()->GetPath().Append(chrome::kCookieFilename);
  }

  base::FilePath GetNewCookieLocation() {
    return browser()
        ->profile()
        ->GetPath()
        .Append(chrome::kNetworkDataDirname)
        .Append(chrome::kCookieFilename);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The first part starts with migration disabled and stores a cookie.
IN_PROC_BROWSER_TEST_F(ChromeNetworkServiceMigrationBrowserTest,
                       PRE_PRE_MigrateData) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(features::kTriggerNetworkDataMigration));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/setcookie.html")));
  ASSERT_NO_FATAL_FAILURE(VerifyCookiePresent());
  EXPECT_TRUE(base::PathExists(GetOldCookieLocation()));
  EXPECT_FALSE(base::PathExists(GetNewCookieLocation()));
}

// The second part enables the migration feature and checks for the cookie and
// also that the data itself has migrated on disk.
IN_PROC_BROWSER_TEST_F(ChromeNetworkServiceMigrationBrowserTest,
                       PRE_MigrateData) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(features::kTriggerNetworkDataMigration));
  ASSERT_NO_FATAL_FAILURE(VerifyCookiePresent());
  EXPECT_FALSE(base::PathExists(GetOldCookieLocation()));
  EXPECT_TRUE(base::PathExists(GetNewCookieLocation()));
}

// The third part verifies that if migration feature is disabled, then cookies
// still work and the data is still migrated (because it is not possible to
// unmigrate).
IN_PROC_BROWSER_TEST_F(ChromeNetworkServiceMigrationBrowserTest, MigrateData) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(features::kTriggerNetworkDataMigration));
  ASSERT_NO_FATAL_FAILURE(VerifyCookiePresent());
  EXPECT_FALSE(base::PathExists(GetOldCookieLocation()));
  EXPECT_TRUE(base::PathExists(GetNewCookieLocation()));
}

}  // namespace
