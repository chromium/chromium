// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

// Class used to run browser tests that verify SSL UI triggered due to
// Certificate Transparency verification failures.
//
// Note: The tests in this browsertest are limited, since it isn't possible to
// test the successful case with the compiled-in CT logs as that would require
// a real-world publicly trusted certificate & private key to be checked in as
// test data.
//
// The successful case with component-updated logs is tested by
// pki_metadata_component_installer_browsertest.cc though, and the CT policy is
// tested by certificate_transparency_policy_browsertest.cc.

class CertificateTransparencyBrowserTest : public InProcessBrowserTest {
 public:
  CertificateTransparencyBrowserTest() {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }

  CertificateTransparencyBrowserTest(
      const CertificateTransparencyBrowserTest&) = delete;
  CertificateTransparencyBrowserTest& operator=(
      const CertificateTransparencyBrowserTest&) = delete;

  ~CertificateTransparencyBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

// Visit an HTTPS page that has a publicly trusted certificate issued after
// the Certificate Transparency requirement date of April 2018. The connection
// should be blocked, as the server will not be providing CT details, and the
// Chrome CT Policy should be being enforced.
IN_PROC_BROWSER_TEST_F(CertificateTransparencyBrowserTest,
                       EnforcedAfterApril2018) {
  SystemNetworkContextManager::GetInstance()->SetCTLogListTimelyForTesting();

  // Make the test root be interpreted as a known root so that CT will be
  // required.
  scoped_refptr<net::X509Certificate> root_cert =
      net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
  ASSERT_TRUE(root_cert);
  net::ScopedTestKnownRoot scoped_known_root(root_cert.get());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/ssl/google.html")));

  ssl_test_util::CheckSecurityState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED,
      security_state::DANGEROUS,
      ssl_test_util::AuthState::SHOWING_INTERSTITIAL);
}
