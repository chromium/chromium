// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "device/fido/features.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/site_isolation/features.h"
#else
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/trusted_vault/standalone_trusted_vault_client.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::IsEmpty;

constexpr char kFakeGaiaId[] = "fake_gaia_id";

#if !BUILDFLAG(IS_ANDROID)
const AccountInfo& FakeAccount() {
  static const base::NoDestructor<AccountInfo> account([]() {
    AccountInfo account;
    account.gaia = kFakeGaiaId;
    return account;
  }());
  return *account;
}
#endif

const char kConsoleSuccessMessage[] = "trusted_vault_encryption_keys:OK";
const char kConsoleFailureMessage[] = "trusted_vault_encryption_keys:FAIL";
#if !BUILDFLAG(IS_ANDROID)
const char kConsoleUncaughtTypeErrorMessagePattern[] =
    "Uncaught TypeError: Error processing argument at index *";
#endif

// Executes JS to call chrome.setSyncEncryptionKeys(). Either
// |kConsoleSuccessMessage| or |kConsoleFailureMessage| is logged to the console
// upon completion.
void ExecJsSetSyncEncryptionKeys(content::RenderFrameHost* render_frame_host,
                                 const std::vector<uint8_t>& key,
                                 int key_version) {
  // To simplify the test, it limits the size of `key` to 1.
  DCHECK_EQ(key.size(), 1u);
  const std::string script = base::StringPrintf(
      R"(
      if (chrome.setSyncEncryptionKeys === undefined) {
        console.log('%s');
      } else {
        let buffer = new ArrayBuffer(1);
        let view = new Uint8Array(buffer);
        view[0] = %d;
        chrome.setSyncEncryptionKeys(
            () => {console.log('%s');},
            "%s", [buffer], %d);
      }
    )",
      kConsoleFailureMessage, key[0], kConsoleSuccessMessage, kFakeGaiaId,
      key_version);

  std::ignore = content::ExecJs(render_frame_host, script);
}

void ExecJsSetClientEncryptionKeysForSecurityDomain(
    content::RenderFrameHost* render_frame_host,
    const char* security_domain_name,
    const std::vector<uint8_t>& key) {
  // To simplify the test, it limits the size of `key` to 1.
  DCHECK_EQ(key.size(), 1u);
  const std::string script = base::StringPrintf(
      R"(
      if (chrome.setClientEncryptionKeys === undefined) {
        console.log('%s');
      } else {
        let key = new ArrayBuffer(1);
        let view = new Uint8Array(key);
        view[0] = %d;
        chrome.setClientEncryptionKeys(
            () => {console.log('%s');},
            "%s",
            new Map([['%s', [{epoch: 0, key}]]]));
      }
    )",
      kConsoleFailureMessage, key[0], kConsoleSuccessMessage, kFakeGaiaId,
      security_domain_name);

  std::ignore = content::ExecJs(render_frame_host, script);
}

void ExecJsSetClientEncryptionKeys(content::RenderFrameHost* render_frame_host,
                                   const std::vector<uint8_t>& key) {
  ExecJsSetClientEncryptionKeysForSecurityDomain(
      render_frame_host, trusted_vault::kSyncSecurityDomainName, key);
}

#if !BUILDFLAG(IS_ANDROID)
void ExecJsSetClientEncryptionKeysForInvalidSecurityDomain(
    content::RenderFrameHost* render_frame_host,
    const std::vector<uint8_t>& key) {
  // To simplify the test, it limits the size of `key` to 1.
  DCHECK_EQ(key.size(), 1u);
  const std::string script = base::StringPrintf(
      R"(
      if (chrome.setClientEncryptionKeys === undefined) {
        console.log('%s');
      } else {
        let key = new ArrayBuffer(1);
        let view = new Uint8Array(key);
        view[0] = %d;
        chrome.setClientEncryptionKeys(
            () => {console.log('%s');},
            "%s",
            new Map([['invalid', [{epoch: 0, key}]]]));
      }
    )",
      kConsoleFailureMessage, key[0], kConsoleSuccessMessage, kFakeGaiaId);

  std::ignore = content::ExecJs(render_frame_host, script);
}

void ExecJsSetClientEncryptionKeysWithIllformedArgs(
    content::RenderFrameHost* render_frame_host) {
  // Don't actually pass a key. This should trigger a render-side argument
  // parsing failure.
  const std::string script = base::StringPrintf(
      R"(
      if (chrome.setClientEncryptionKeys === undefined) {
        console.log('%s');
      } else {
        chrome.setClientEncryptionKeys(
            () => {console.log('%s');},
            "%s",
            new Map([['chromesync', [{epoch: 0}]]]));
      }
    )",
      kConsoleFailureMessage, kConsoleSuccessMessage, kFakeGaiaId);

  std::ignore = content::ExecJs(render_frame_host, script);
}
#endif

// Executes JS to call chrome.addTrustedSyncEncryptionRecoveryMethod. Either
// |kConsoleSuccessMessage| or |kConsoleFailureMessage| is logged to the console
// upon completion.
void ExecJsAddTrustedSyncEncryptionRecoveryMethod(
    content::RenderFrameHost* render_frame_host,
    const std::vector<uint8_t>& public_key) {
  // To simplify the test, it limits the size of `public_key` to 1.
  DCHECK_EQ(public_key.size(), 1u);
  const std::string script = base::StringPrintf(
      R"(
      if (chrome.addTrustedSyncEncryptionRecoveryMethod === undefined) {
        console.log('%s');
      } else {
        let buffer = new ArrayBuffer(1);
        let view = new Uint8Array(buffer);
        view[0] = %d;
        chrome.addTrustedSyncEncryptionRecoveryMethod(
            () => {console.log('%s');},
            "%s", buffer, 2);
      }
    )",
      kConsoleFailureMessage, public_key[0], kConsoleSuccessMessage,
      kFakeGaiaId);

  std::ignore = content::ExecJs(render_frame_host, script);
}

// Key retrieval doesn't exist on Android and cannot be verified.
#if !BUILDFLAG(IS_ANDROID)
std::vector<std::vector<uint8_t>> FetchTrustedVaultKeysForProfile(
    Profile* profile,
    trusted_vault::SecurityDomainId security_domain,
    const AccountInfo& account_info) {
  // Waits until the sync trusted vault keys have been received and stored.
  base::RunLoop loop;
  std::vector<std::vector<uint8_t>> actual_keys;

  TrustedVaultServiceFactory::GetForProfile(profile)
      ->GetTrustedVaultClient(security_domain)
      ->FetchKeys(account_info,
                  base::BindLambdaForTesting(
                      [&](const std::vector<std::vector<uint8_t>>& keys) {
                        actual_keys = keys;
                        loop.Quit();
                      }));
  loop.Run();
  return actual_keys;
}

int FetchLastTrustedVaultKeyVersionForProfile(
    Profile* profile,
    trusted_vault::SecurityDomainId security_domain,
    const AccountInfo& account_info) {
  base::RunLoop loop;
  int actual_last_key_version = -1;

  static_cast<trusted_vault::StandaloneTrustedVaultClient*>(
      TrustedVaultServiceFactory::GetForProfile(profile)->GetTrustedVaultClient(
          security_domain))
      ->GetLastKeyVersionForTesting(
          account_info.gaia,
          base::BindLambdaForTesting([&](int last_key_version) {
            actual_last_key_version = last_key_version;
            loop.Quit();
          }));
  loop.Run();
  return actual_last_key_version;
}

#endif  // !BUILDFLAG(IS_ANDROID)

class TrustedVaultEncryptionKeysTabHelperBrowserTest
    : public PlatformBrowserTest {
 public:
  TrustedVaultEncryptionKeysTabHelperBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        prerender_helper_(base::BindRepeating(
            &TrustedVaultEncryptionKeysTabHelperBrowserTest::web_contents,
            base::Unretained(this))) {
#if BUILDFLAG(IS_ANDROID)
    // Avoid the disabling of site isolation due to memory constraints, required
    // on Android so that ApplyGlobalIsolatedOrigins() takes effect regardless
    // of available memory when running the test (otherwise low-memory bots may
    // run into test failures).
    feature_list_.InitAndEnableFeatureWithParameters(
        site_isolation::features::kSiteIsolationMemoryThresholds,
        {{site_isolation::features::
              kStrictSiteIsolationMemoryThresholdParamName,
          "0"},
         { site_isolation::features::
               kPartialSiteIsolationMemoryThresholdParamName,
           "0" }});
#elif BUILDFLAG(IS_CHROMEOS)
    feature_list_.InitWithFeatureStates(
        {{device::kChromeOsPasskeys, true},
         { trusted_vault::kSetClientEncryptionKeysJsApi,
           true }});
#else
    feature_list_.InitAndEnableFeature(
        trusted_vault::kSetClientEncryptionKeysJsApi);
#endif
  }

  ~TrustedVaultEncryptionKeysTabHelperBrowserTest() override {
    // An explicit reset is required here to avoid CHECK failures due to
    // unexpected reset ordering.
    feature_list_.Reset();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  bool HasEncryptionKeysApi(content::RenderFrameHost* rfh) {
    auto* tab_helper =
        TrustedVaultEncryptionKeysTabHelper::FromWebContents(web_contents());
    return tab_helper->HasEncryptionKeysApiForTesting(rfh);
  }

  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    PlatformBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Override the sign-in URL so that it includes correct port from the test
    // server.
    command_line->AppendSwitchASCII(
        ::switches::kGaiaUrl,
        https_server()->GetURL("accounts.google.com", "/").spec());

    // Ignore cert errors so that the sign-in URL can be loaded from a site
    // other than localhost (the EmbeddedTestServer serves a certificate that
    // is valid for localhost).
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    PlatformBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    https_server()->StartAcceptingConnections();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  content::test::PrerenderTestHelper prerender_helper_;
};

class TrustedVaultEncryptionKeysTabHelperWithEnclaveBrowserTest
    : public TrustedVaultEncryptionKeysTabHelperBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnEnclaveAuthenticator};
};

// Tests that chrome.setSyncEncryptionKeys() works in the main frame, except on
// Android. On Android, this particular Javascript API isn't defined.
#if BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldNotBindEncryptionKeysApiOnAndroid) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleFailureMessage);

  // Calling setSyncEncryptionKeys() or setClientEncryptionKeys() in the main
  // frame shouldn't work.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetSyncEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                              kEncryptionKey, /*key_version=*/1);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  ExecJsSetClientEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                                kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(2u, console_observer.messages().size());
}

#else

void ExecJsSetClientEncryptionKeysWithMultipleKeys(
    content::RenderFrameHost* render_frame_host,
    const std::vector<uint8_t>& key1,
    const std::vector<uint8_t>& key2) {
  DCHECK_EQ(key1.size(), 1u);
  DCHECK_EQ(key2.size(), 1u);
  const std::string script = base::StringPrintf(
      R"(
      if (chrome.setClientEncryptionKeys === undefined) {
        console.log('%s');
      } else {
        let key1 = new ArrayBuffer(1);
        let view1 = new Uint8Array(key1);
        view1[0] = %d;
        let key2 = new ArrayBuffer(1);
        let view2 = new Uint8Array(key2);
        view2[0] = %d;
        chrome.setClientEncryptionKeys(
            () => {console.log('%s');},
            "%s",
            new Map([
                ['chromesync',
                 [{epoch: 1, key: key1}, {epoch: 2, key: key2}]]
            ]));
      }
    )",
      kConsoleFailureMessage, key1[0], key2[0], kConsoleSuccessMessage,
      kFakeGaiaId);

  std::ignore = content::ExecJs(render_frame_host, script);
}

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldBindSyncEncryptionKeysApiInMainFrame) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  // Calling setSyncEncryptionKeys() in the main frame works and it gets
  // the callback by setSyncEncryptionKeys().
  const std::vector<uint8_t> kEncryptionKey = {7};
  const int kEncryptionKeyVersion = 24;
  ExecJsSetSyncEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                              kEncryptionKey, kEncryptionKeyVersion);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  // Collect histograms from the renderer process, since otherwise
  // HistogramTester cannot verify the ones instrumented in the renderer.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysValidArgs", 1 /*Valid*/, 1);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
      0 /*Not Incognito*/, 1);

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  EXPECT_THAT(actual_keys, ElementsAre(kEncryptionKey));
  EXPECT_THAT(FetchLastTrustedVaultKeyVersionForProfile(
                  browser()->profile(),
                  trusted_vault::SecurityDomainId::kChromeSync, FakeAccount()),
              Eq(kEncryptionKeyVersion));
}

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldBindClientEncryptionKeysApiInMainFrame) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  // Calling setClientEncryptionKeys() in the main frame works.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetClientEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                                kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  // Collect histograms from the renderer process, since otherwise
  // HistogramTester cannot verify the ones instrumented in the renderer.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysValidArgs", 1 /*Valid*/,
      1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysForSecurityDomain",
      1 /*Chrome Sync*/, 1);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.AllProfiles",
      1 /*Chrome Sync*/, 1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.OffTheRecordOnly",
      1 /*Chrome Sync*/, 0);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
      0 /*Not Incognito*/, 1);

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  EXPECT_THAT(actual_keys, ElementsAre(kEncryptionKey));
}

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ClientEncryptionKeysApiShouldSetMultipleKeys) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  const std::vector<std::vector<uint8_t>> kEncryptionKeys = {{7}, {8}};
  ExecJsSetClientEncryptionKeysWithMultipleKeys(
      web_contents()->GetPrimaryMainFrame(), kEncryptionKeys[0],
      kEncryptionKeys[1]);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysValidArgs", 1 /*Valid*/,
      1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysForSecurityDomain",
      1 /*Chrome Sync*/, 1);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.AllProfiles",
      1 /*Chrome Sync*/, 1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.OffTheRecordOnly",
      1 /*Chrome Sync*/, 0);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
      0 /*Not Incognito*/, 1);

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  EXPECT_THAT(actual_keys, ElementsAreArray(kEncryptionKeys));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(
    TrustedVaultEncryptionKeysTabHelperWithEnclaveBrowserTest,
    SetPasskeysKeyInEnclaveManager) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  EnclaveManager* const enclave_manager =
      EnclaveManagerFactory::GetAsEnclaveManagerForProfile(
          browser()->profile());
  const unsigned initial_count = enclave_manager->store_keys_count();

  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetClientEncryptionKeysForSecurityDomain(
      web_contents()->GetPrimaryMainFrame(),
      trusted_vault::kPasskeysSecurityDomainName, kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(console_observer.messages().size(), 1u);

  // The keys should have been stored to the `EnclaveManager`.
  EXPECT_EQ(enclave_manager->store_keys_count(), initial_count + 1);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysValidArgs", 1 /*Valid*/,
      1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysForSecurityDomain",
      2 /*Passkeys*/, 1);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.AllProfiles",
      2 /*Passkeys*/, 1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.OffTheRecordOnly",
      2 /*Passkeys*/, 0);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
      0 /*Not Incognito*/, 1);

  // No security domain client for passkeys, so no keys could have been set.
  EXPECT_EQ(
      TrustedVaultServiceFactory::GetForProfile(browser()->profile())
          ->GetTrustedVaultClient(trusted_vault::SecurityDomainId::kPasskeys),
      nullptr);

  // No keys should have been set for chromesync either.
  EXPECT_THAT(FetchTrustedVaultKeysForProfile(
                  browser()->profile(),
                  trusted_vault::SecurityDomainId::kChromeSync, FakeAccount()),
              IsEmpty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(
    TrustedVaultEncryptionKeysTabHelperBrowserTest,
    ShouldIgnoreClientEncryptionKeysForInvalidSecurityDomain) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  // Calling setClientEncryptionKeys() in the main frame works.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetClientEncryptionKeysForInvalidSecurityDomain(
      web_contents()->GetPrimaryMainFrame(), kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  // Collect histograms from the renderer process, since otherwise
  // HistogramTester cannot verify the ones instrumented in the renderer.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysValidArgs", 1 /*Valid*/,
      1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysForSecurityDomain",
      0 /*Invalid*/, 1);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.AllProfiles",
      0 /*Invalid*/, 1);

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  EXPECT_THAT(actual_keys, IsEmpty());
}

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldIgnoreClientEncryptionKeysWithIllformedArgs) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleUncaughtTypeErrorMessagePattern);

  base::HistogramTester histogram_tester;

  ExecJsSetClientEncryptionKeysWithIllformedArgs(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  // Collect histograms from the renderer process, since otherwise
  // HistogramTester cannot verify the ones instrumented in the renderer.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysValidArgs", 0 /*Invalid*/,
      1);

  // Renderer makes no attempt of setting keys on the browser side.
  histogram_tester.ExpectTotalCount(
      "TrustedVault.JavascriptSetClientEncryptionKeysForSecurityDomain", 0);
  histogram_tester.ExpectTotalCount(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.AllProfiles", 0);

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  EXPECT_THAT(actual_keys, IsEmpty());
}

// Tests that chrome.setSyncEncryptionKeys() works in a fenced frame.
IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldBindSyncEncryptionKeysApiInFencedFrame) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  ASSERT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  const GURL main_url = https_server()->GetURL("accounts.google.com",
                                               "/fenced_frames/title1.html");
  auto* fenced_frame_host = fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), main_url);
  // EncryptionKeysApi is also created for a fenced frame since it's a main
  // frame as well.
  EXPECT_TRUE(HasEncryptionKeysApi(fenced_frame_host));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleSuccessMessage);

  // Calling setSyncEncryptionKeys() in the fenced frame works and it gets
  // the callback by setSyncEncryptionKeys().
  const std::vector<uint8_t> kEncryptionKey = {7};
  const int kEncryptionKeyVersion = 24;
  ExecJsSetSyncEncryptionKeys(fenced_frame_host, kEncryptionKey,
                              kEncryptionKeyVersion);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  EXPECT_THAT(actual_keys, ElementsAre(kEncryptionKey));
  EXPECT_THAT(FetchLastTrustedVaultKeyVersionForProfile(
                  browser()->profile(),
                  trusted_vault::SecurityDomainId::kChromeSync, FakeAccount()),
              Eq(kEncryptionKeyVersion));
}

// Tests that chrome.setClientEncryptionKeys() works in a fenced frame.
IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldBindClientEncryptionKeysApiInFencedFrame) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  ASSERT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  const GURL main_url = https_server()->GetURL("accounts.google.com",
                                               "/fenced_frames/title1.html");
  auto* fenced_frame_host = fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), main_url);
  // EncryptionKeysApi is also created for a fenced frame since it's a main
  // frame as well.
  EXPECT_TRUE(HasEncryptionKeysApi(fenced_frame_host));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleSuccessMessage);

  // Calling setClientEncryptionKeys() in the fenced frame works.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetClientEncryptionKeys(fenced_frame_host, kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  EXPECT_THAT(actual_keys, ElementsAre(kEncryptionKey));
}

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldIgnoreSyncEncryptionsKeysInIncognito) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");

  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), initial_url);
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(
      HasEncryptionKeysApi(incognito_web_contents->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(incognito_web_contents);
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  // Calling setSyncEncryptionKeys() in incognito completes successfully,
  // although it does nothing.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetSyncEncryptionKeys(incognito_web_contents->GetPrimaryMainFrame(),
                              kEncryptionKey, /*key_version=*/1);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  // Collect histograms from the renderer process, since otherwise
  // HistogramTester cannot verify the ones instrumented in the renderer.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysValidArgs", 1 /*Valid*/, 1);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.AllProfiles",
      1 /*Chrome Sync*/, 1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.OffTheRecordOnly",
      1 /*Chrome Sync*/, 1);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
      1 /*Incognito*/, 1);

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  // In incognito, the keys should actually be ignored, never forwarded to
  // TrustedVaultService.
  EXPECT_THAT(actual_keys, IsEmpty());
}

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldIgnoreClientEncryptionsKeysInIncognito) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");

  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), initial_url);
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(
      HasEncryptionKeysApi(incognito_web_contents->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(incognito_web_contents);
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  // Calling setClientEncryptionKeys() in incognito completes successfully,
  // although it does nothing.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetClientEncryptionKeys(incognito_web_contents->GetPrimaryMainFrame(),
                                kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  // Collect histograms from the renderer process, since otherwise
  // HistogramTester cannot verify the ones instrumented in the renderer.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysValidArgs", 1 /*Valid*/,
      1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.JavascriptSetClientEncryptionKeysForSecurityDomain",
      1 /*Chrome Sync*/, 1);

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.AllProfiles",
      1 /*Chrome Sync*/, 1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.SetEncryptionKeysForSecurityDomain.OffTheRecordOnly",
      1 /*Chrome Sync*/, 1);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
      1 /*Incognito*/, 1);

  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(
          browser()->profile(), trusted_vault::SecurityDomainId::kChromeSync,
          FakeAccount());
  // In incognito, the keys should actually be ignored, never forwarded to
  // TrustedVaultService.
  EXPECT_THAT(actual_keys, IsEmpty());
}

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldIgnoreRecoveryMethodInIncognito) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");

  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), initial_url);
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(
      HasEncryptionKeysApi(incognito_web_contents->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(incognito_web_contents);
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  // Calling addTrustedSyncEncryptionRecoveryMethod() in incognito completes
  // successfully, although it does nothing.
  const std::vector<uint8_t> kPublicKey = {7};
  ExecJsAddTrustedSyncEncryptionRecoveryMethod(
      incognito_web_contents->GetPrimaryMainFrame(), kPublicKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptAddRecoveryMethodIsIncognito",
      1 /*Incognito*/, 1);
}

IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldNotSetKeysIfCallingFrameIsDeleted_364338802) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  const GURL frame_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "test", frame_url));
  content::RenderFrameHost* child_frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);

  // EncryptionKeysApi is created for the child frame as the origin is allowed.
  ASSERT_TRUE(HasEncryptionKeysApi(child_frame));

  content::WebContentsConsoleObserver console_observer(web_contents());
  content::RenderFrameDeletedObserver frame_deleted_observer(child_frame);

  // Ensure that deleting the calling frame in the middle of the request doesn't
  // crash. Keys will not be set successfully.
  constexpr std::string_view script = R"(
      var childFrame = document.querySelector("iframe");
      let trustedVaultKey = new Object();
      childFrame.contentWindow.Object.defineProperty(
          trustedVaultKey, "key", { get: () => {
              document.body.remove(childFrame);
              return new ArrayBuffer(1);
      }});
      trustedVaultKey.key = new ArrayBuffer(1);
      trustedVaultKey.epoch = 1;
      childFrame.contentWindow.chrome.setClientEncryptionKeys(
          () => { console.log("test:OK") },
          "fake_gaia_id",
          new Map([['chromesync', [trustedVaultKey]]]));
    )";

  ASSERT_TRUE(content::ExecJs(web_contents(), script));
  EXPECT_TRUE(frame_deleted_observer.WaitUntilDeleted());
  EXPECT_EQ(console_observer.messages().size(), 0u);
  EXPECT_THAT(FetchTrustedVaultKeysForProfile(
                  browser()->profile(),
                  trusted_vault::SecurityDomainId::kChromeSync, FakeAccount()),
              IsEmpty());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Tests that chrome.addTrustedSyncEncryptionRecoveryMethod() works in the main
// frame.
IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldBindAddRecoveryMethodApiInMainFrame) {
  // Out desktop platforms using StandaloneTrustedVaultClient, a primary account
  // needs to be set for the Javascript operation to complete. Otherwise, the
  // logic is deferred until a primary account is set and the test would wait
  // indefinitely until it times out.
#if !BUILDFLAG(IS_ANDROID)
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      "testusername", signin::ConsentLevel::kSync);
#endif  // !BUILDFLAG(IS_ANDROID)

  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleSuccessMessage);

  base::HistogramTester histogram_tester;

  // Calling addTrustedSyncEncryptionRecoveryMethod() in the main frame works.
  const std::vector<uint8_t> kPublicKey = {7};
  ExecJsAddTrustedSyncEncryptionRecoveryMethod(
      web_contents()->GetPrimaryMainFrame(), kPublicKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  // Collect histograms from the renderer process, since otherwise
  // HistogramTester cannot verify the ones instrumented in the renderer.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptAddRecoveryMethodValidArgs", 1 /*Valid*/, 1);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptAddRecoveryMethodIsIncognito",
      0 /*Not Incognito*/, 1);

#if BUILDFLAG(IS_ANDROID)
  // This metric is only instrumented on Android.
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptAddRecoveryMethodUserKnown", 0 /*Unknown*/,
      1);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Tests that chrome.setSyncEncryptionKeys() doesn't work in prerendering.
// If it is called in prerendering, it triggers canceling the prerendering
// and EncryptionKeyApi is not bound.
IN_PROC_BROWSER_TEST_F(TrustedVaultEncryptionKeysTabHelperBrowserTest,
                       ShouldNotBindEncryptionKeysApiInPrerendering) {
  // Out desktop platforms using StandaloneTrustedVaultClient, a primary account
  // needs to be set for the Javascript operation to complete. Otherwise, the
  // logic is deferred until a primary account is set and the test would wait
  // indefinitely until it times out.
#if !BUILDFLAG(IS_ANDROID)
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      "testusername", signin::ConsentLevel::kSync);
#endif  // !BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  const GURL signin_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), signin_url));
  // EncryptionKeysApi is created for the primary page.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  const GURL prerendering_url =
      https_server()->GetURL("accounts.google.com", "/simple.html");

  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerendering_url);
  content::RenderFrameHostWrapper prerendered_frame_host(
      prerender_helper().GetPrerenderedMainFrameHost(host_id));

  // EncryptionKeysApi is also created for prerendering since it's a main frame
  // as well.
  EXPECT_TRUE(HasEncryptionKeysApi(prerendered_frame_host.get()));

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  {
    content::WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsoleSuccessMessage);

    // Calling addTrustedSyncEncryptionRecoveryMethod() in the prerendered page
    // triggers canceling the prerendering since it's a associated interface and
    // the default policy is `MojoBinderAssociatedPolicy::kCancel`. Calling  in
    // the main frame works.
    const std::vector<uint8_t> kPublicKey = {7};
    ExecJsAddTrustedSyncEncryptionRecoveryMethod(prerendered_frame_host.get(),
                                                 kPublicKey);
    host_observer.WaitForDestroyed();
    EXPECT_EQ(0u, console_observer.messages().size());
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
        4 /*PrerenderCancelledInterface::kTrustedVaultEncryptionKeys*/, 1);
    EXPECT_TRUE(prerendered_frame_host.IsRenderFrameDeleted());
  }

  prerender_helper().NavigatePrimaryPage(prerendering_url);
  // Ensure that loading `prerendering_url` is not activated from prerendering.
  EXPECT_FALSE(host_observer.was_activated());
  auto* primary_main_frame = web_contents()->GetPrimaryMainFrame();
  // Ensure that the main frame has EncryptionKeysApi.
  EXPECT_TRUE(HasEncryptionKeysApi(primary_main_frame));

  {
    content::WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsoleSuccessMessage);

    // Calling addTrustedSyncEncryptionRecoveryMethod() in the primary page
    // works.
    const std::vector<uint8_t> kPublicKey = {7};
    ExecJsAddTrustedSyncEncryptionRecoveryMethod(primary_main_frame,
                                                 kPublicKey);
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_EQ(1u, console_observer.messages().size());
  }
}

// Same as SyncEncryptionKeysTabHelperBrowserTest but switches::kGaiaUrl does
// NOT point to the embedded test server, which means it gets treated as
// disallowed origin.
class TrustedVaultEncryptionKeysTabHelperWithoutAllowedOriginBrowserTest
    : public TrustedVaultEncryptionKeysTabHelperBrowserTest {
 public:
  TrustedVaultEncryptionKeysTabHelperWithoutAllowedOriginBrowserTest() =
      default;
  ~TrustedVaultEncryptionKeysTabHelperWithoutAllowedOriginBrowserTest()
      override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TrustedVaultEncryptionKeysTabHelperBrowserTest::SetUpCommandLine(
        command_line);
    // Override kGaiaUrl to the default so the embedded test server isn't
    // treated as an allowed origin.
    command_line->RemoveSwitch(::switches::kGaiaUrl);
  }
};

// Tests that chrome.setSyncEncryptionKeys() and
// chrome.setClientEncryptionKeys() doesn't work in disallowed origins.
IN_PROC_BROWSER_TEST_F(
    TrustedVaultEncryptionKeysTabHelperWithoutAllowedOriginBrowserTest,
    ShouldNotBindEncryptionKeys) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is NOT created for the primary page as the origin is
  // disallowed.
  EXPECT_FALSE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleFailureMessage);

  // setSyncEncryptionKeys() and setClientEncryptionKeys() should fail because
  // they aren't defined.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetSyncEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                              kEncryptionKey, /*key_version=*/1);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
  ExecJsSetClientEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                                kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(2u, console_observer.messages().size());
}

}  // namespace
