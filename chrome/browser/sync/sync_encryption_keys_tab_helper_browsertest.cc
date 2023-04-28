// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#include "components/site_isolation/features.h"
#else
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

const char kFakeGaiaId[] = "fake_gaia_id";
const char kConsoleSuccessMessage[] = "setSyncEncryptionKeys:Done";
const char kConsoleFailureMessage[] = "setSyncEncryptionKeys:Undefined";

// Executes JS to call chrome.setSyncEncryptionKeys(). Either
// |kConsoleSuccessMessage| or |kConsoleFailureMessage| is logged to the console
// upon completion.
void ExecJsSetSyncEncryptionKeys(content::RenderFrameHost* render_frame_host,
                                 const std::vector<uint8_t>& key) {
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
            "%s", [buffer], 0);
      }
    )",
      kConsoleFailureMessage, key[0], kConsoleSuccessMessage, kFakeGaiaId);

  std::ignore = content::ExecJs(render_frame_host, script);
}

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
    const AccountInfo& account_info) {
  syncer::SyncServiceImpl* sync_service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(profile);
  syncer::TrustedVaultClient* trusted_vault_client =
      sync_service->GetSyncClientForTest()->GetTrustedVaultClient();

  // Waits until the sync trusted vault keys have been received and stored.
  base::RunLoop loop;
  std::vector<std::vector<uint8_t>> actual_keys;

  trusted_vault_client->FetchKeys(
      account_info, base::BindLambdaForTesting(
                        [&](const std::vector<std::vector<uint8_t>>& keys) {
                          actual_keys = keys;
                          loop.Quit();
                        }));
  loop.Run();
  return actual_keys;
}

#endif  // !BUILDFLAG(IS_ANDROID)

class SyncEncryptionKeysTabHelperBrowserTest : public PlatformBrowserTest {
 public:
  SyncEncryptionKeysTabHelperBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        prerender_helper_(base::BindRepeating(
            &SyncEncryptionKeysTabHelperBrowserTest::web_contents,
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
#endif  // BUILDFLAG(IS_ANDROID)
  }

  ~SyncEncryptionKeysTabHelperBrowserTest() override {
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
        SyncEncryptionKeysTabHelper::FromWebContents(web_contents());
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

// Tests that chrome.setSyncEncryptionKeys() works in the main frame, except on
// Android. On Android, this particular Javascript API isn't defined.
#if BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
                       ShouldNotBindEncryptionKeysApiOnAndroid) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is created for the primary page as the origin is allowed.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleFailureMessage);

  // Calling setSyncEncryptionKeys() in the main frame shouldn't work.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetSyncEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                              kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
}

#else

IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
                       ShouldBindEncryptionKeysApiInMainFrame) {
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
  ExecJsSetSyncEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                              kEncryptionKey);
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

  AccountInfo account;
  account.gaia = kFakeGaiaId;
  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(browser()->profile(), account);
  EXPECT_THAT(actual_keys, testing::ElementsAre(kEncryptionKey));
}

// Tests that chrome.setSyncEncryptionKeys() works in a fenced frame.
IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
                       ShouldBindEncryptionKeysApiInFencedFrame) {
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
  ExecJsSetSyncEncryptionKeys(fenced_frame_host, kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  AccountInfo account;
  account.gaia = kFakeGaiaId;
  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(browser()->profile(), account);
  EXPECT_THAT(actual_keys, testing::ElementsAre(kEncryptionKey));
}

IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
                       ShouldIgnoreEncryptionsKeysInIncognito) {
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
                              kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());

  // Collect histograms from the renderer process, since otherwise
  // HistogramTester cannot verify the ones instrumented in the renderer.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysValidArgs", 1 /*Valid*/, 1);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
      1 /*Incognito*/, 1);

  AccountInfo account;
  account.gaia = kFakeGaiaId;
  std::vector<std::vector<uint8_t>> actual_keys =
      FetchTrustedVaultKeysForProfile(browser()->profile(), account);
  // In incognito, the keys should actually be ignored, never forwarded to
  // SyncService.
  EXPECT_THAT(actual_keys, testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
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

#endif  // BUILDFLAG(IS_ANDROID)

// Tests that chrome.addTrustedSyncEncryptionRecoveryMethod() works in the main
// frame.
IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
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
IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
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

  int host_id = prerender_helper().AddPrerender(prerendering_url);
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
        4 /*PrerenderCancelledInterface::kSyncEncryptionKeysExtension*/, 1);
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
class SyncEncryptionKeysTabHelperWithoutAllowedOriginBrowserTest
    : public SyncEncryptionKeysTabHelperBrowserTest {
 public:
  SyncEncryptionKeysTabHelperWithoutAllowedOriginBrowserTest() = default;
  ~SyncEncryptionKeysTabHelperWithoutAllowedOriginBrowserTest() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncEncryptionKeysTabHelperBrowserTest::SetUpCommandLine(command_line);
    // Override kGaiaUrl to the default so the embedded test server isn't
    // treated as an allowed origin.
    command_line->RemoveSwitch(::switches::kGaiaUrl);
  }
};

// Tests that chrome.setSyncEncryptionKeys() doesn't work in disallowed origins.
IN_PROC_BROWSER_TEST_F(
    SyncEncryptionKeysTabHelperWithoutAllowedOriginBrowserTest,
    ShouldNotBindEncryptionKeys) {
  const GURL initial_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  // EncryptionKeysApi is NOT created for the primary page as the origin is
  // disallowed.
  EXPECT_FALSE(HasEncryptionKeysApi(web_contents()->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kConsoleFailureMessage);

  // Calling setSyncEncryptionKeys() should fail because the API is not even
  // defined.
  const std::vector<uint8_t> kEncryptionKey = {7};
  ExecJsSetSyncEncryptionKeys(web_contents()->GetPrimaryMainFrame(),
                              kEncryptionKey);
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
}

}  // namespace
