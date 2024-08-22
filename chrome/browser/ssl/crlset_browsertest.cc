// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/ssl/cert_verifier_platform_browser_test.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

namespace AuthState = ssl_test_util::AuthState;
namespace CertError = ssl_test_util::CertError;

namespace {

class CRLSetBrowserTest : public PlatformBrowserTest {
 public:
  CRLSetBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~CRLSetBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/");
  }

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  net::EmbeddedTestServer https_server_;
};

const char kHstsTestHostName[] = "hsts-example.test";

}  // namespace

IN_PROC_BROWSER_TEST_F(CRLSetBrowserTest, TestCRLSetRevoked) {
  ASSERT_TRUE(https_server_.Start());

  std::string crl_set_bytes;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(
        net::GetTestCertsDirectory().AppendASCII("crlset_by_leaf_spki.raw"),
        &crl_set_bytes);
  }
  base::RunLoop run_loop;
  content::GetCertVerifierServiceFactory()->UpdateCRLSet(
      base::as_bytes(base::make_span(crl_set_bytes)), run_loop.QuitClosure());
  run_loop.Run();

  bool interstitial_expected =
      ssl_test_util::CertVerifierSupportsCRLSetBlocking();

  // If an interstitial is expected, NavigateToURL should fail, because the
  // interstitial will be the committed URL, rather than the navigated URL.
  EXPECT_EQ(!interstitial_expected,
            content::NavigateToURL(GetActiveWebContents(),
                                   https_server_.GetURL("/ssl/google.html")));

  ASSERT_EQ(interstitial_expected,
            chrome_browser_interstitials::IsShowingInterstitial(
                GetActiveWebContents()));
  ASSERT_TRUE(
      WaitForRenderFrameReady(GetActiveWebContents()->GetPrimaryMainFrame()));

  if (interstitial_expected) {
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(
        GetActiveWebContents()));

    ssl_test_util::CheckAuthenticationBrokenState(
        GetActiveWebContents(), net::CERT_STATUS_REVOKED,
        AuthState::SHOWING_INTERSTITIAL);
  } else {
    ssl_test_util::CheckAuthenticatedState(GetActiveWebContents(),
                                           AuthState::NONE);
  }
}

// Test that CRLSets configured to block MITM certificates cause the
// known interception interstitial.
IN_PROC_BROWSER_TEST_F(CRLSetBrowserTest, TestCRLSetBlockedInterception) {
  ASSERT_TRUE(https_server_.Start());

  // Load a CRLSet that marks the root as a blocked MITM.
  std::string crl_set_bytes;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(net::GetTestCertsDirectory().AppendASCII(
                               "crlset_blocked_interception_by_root.raw"),
                           &crl_set_bytes);
  }
  base::RunLoop run_loop;
  content::GetCertVerifierServiceFactory()->UpdateCRLSet(
      base::as_bytes(base::make_span(crl_set_bytes)), run_loop.QuitClosure());
  run_loop.Run();

  bool interstitial_expected =
      ssl_test_util::CertVerifierSupportsCRLSetBlocking();

  // If an interstitial is expected, NavigateToURL should fail, because the
  // interstitial will be the committed URL, rather than the navigated URL.
  EXPECT_EQ(!interstitial_expected,
            content::NavigateToURL(GetActiveWebContents(),
                                   https_server_.GetURL("/ssl/google.html")));

  ASSERT_EQ(interstitial_expected,
            chrome_browser_interstitials::IsShowingInterstitial(
                GetActiveWebContents()));
  ASSERT_TRUE(
      WaitForRenderFrameReady(GetActiveWebContents()->GetPrimaryMainFrame()));

  if (interstitial_expected) {
    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingBlockedInterceptionInterstitial(
            GetActiveWebContents()));

    ssl_test_util::CheckAuthenticationBrokenState(
        GetActiveWebContents(),
        net::CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED | net::CERT_STATUS_REVOKED,
        AuthState::SHOWING_INTERSTITIAL);
  } else {
    ssl_test_util::CheckAuthenticatedState(GetActiveWebContents(),
                                           AuthState::NONE);
  }
}

// Test that CRLSets configured to identify known MITM certificates do not
// cause an interstitial unless the MITM certificate is blocked.
IN_PROC_BROWSER_TEST_F(CRLSetBrowserTest, TestCRLSetKnownInterception) {
  ASSERT_TRUE(https_server_.Start());

  // Load a CRLSet that marks the root as a known MITM.
  std::string crl_set_bytes;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(net::GetTestCertsDirectory().AppendASCII(
                               "crlset_known_interception_by_root.raw"),
                           &crl_set_bytes);
  }
  base::RunLoop run_loop;
  content::GetCertVerifierServiceFactory()->UpdateCRLSet(
      base::as_bytes(base::make_span(crl_set_bytes)), run_loop.QuitClosure());
  run_loop.Run();

  // Navigate to the page. It should not cause an interstitial, but should
  // allow for the display of additional information that interception is
  // happening.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     https_server_.GetURL("/ssl/google.html")));
  ssl_test_util::CheckAuthenticatedState(GetActiveWebContents(),
                                         AuthState::NONE);

  content::NavigationEntry* entry =
      GetActiveWebContents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetSSL().cert_status &
              net::CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
}

// While TestCRLSetBlockedInterception and TestCRLSetKnownInterception use
// a real CertVerifier in order to test that a real CRLSet is delivered and
// processed, testing HSTS requires with a MockCertVerifier so that the
// cert will match the intended hostname, and thus only fail because it's a
// blocked MITM certificate. This requires using a CertVerifierBrowserTest,
// which is not suitable for the previous tests because it does not test
// CRLSets.
class CRLSetInterceptionBrowserTest : public CertVerifierPlatformBrowserTest {
 public:
  CRLSetInterceptionBrowserTest()
      : CertVerifierPlatformBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    CertVerifierPlatformBrowserTest::SetUpOnMainThread();
    ssl_test_util::SetHSTSForHostName(
        GetActiveWebContents()->GetBrowserContext(), kHstsTestHostName);

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/");
  }

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(CRLSetInterceptionBrowserTest,
                       KnownInterceptionWorksOnHSTS) {
  ASSERT_TRUE(https_server_.Start());

  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server_.GetCertificate();
  verify_result.cert_status =
      net::CERT_STATUS_REVOKED | net::CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED;

  // Simulate verification returning that the certificate is a blocked
  // interception certificate.
  mock_cert_verifier()->AddResultForCertAndHost(
      https_server_.GetCertificate(), kHstsTestHostName, verify_result,
      net::ERR_CERT_KNOWN_INTERCEPTION_BLOCKED);

  GURL::Replacements replacements;
  replacements.SetHostStr(kHstsTestHostName);
  GURL url =
      https_server_.GetURL("/ssl/google.html").ReplaceComponents(replacements);

  // Expect navigation to fail, because this should always result in an
  // interstitial.
  ASSERT_FALSE(content::NavigateToURL(GetActiveWebContents(), url));

  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      GetActiveWebContents()));
  ASSERT_TRUE(
      WaitForRenderFrameReady(GetActiveWebContents()->GetPrimaryMainFrame()));

  ssl_test_util::CheckSecurityState(
      GetActiveWebContents(),
      net::CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED | net::CERT_STATUS_REVOKED,
      security_state::DANGEROUS, AuthState::SHOWING_INTERSTITIAL);

  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingBlockedInterceptionInterstitial(
          GetActiveWebContents()));

  // Expect there to be a proceed link, even for HSTS.
  EXPECT_TRUE(chrome_browser_interstitials::InterstitialHasProceedLink(
      GetActiveWebContents()->GetPrimaryMainFrame()));
}
