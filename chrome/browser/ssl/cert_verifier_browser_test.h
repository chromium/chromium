// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERT_VERIFIER_BROWSER_TEST_H_
#define CHROME_BROWSER_SSL_CERT_VERIFIER_BROWSER_TEST_H_

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"

// CertVerifierBrowserTest allows tests to force certificate
// verification results for requests made with any profile's main
// request context (such as navigations). To do so, tests can use the
// MockCertVerifier exposed via
// CertVerifierBrowserTest::mock_cert_verifier().
class CertVerifierBrowserTest : public InProcessBrowserTest {
 public:
  CertVerifierBrowserTest();

  CertVerifierBrowserTest(const CertVerifierBrowserTest&) = delete;
  CertVerifierBrowserTest& operator=(const CertVerifierBrowserTest&) = delete;

  ~CertVerifierBrowserTest() override;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

  content::ContentMockCertVerifier::CertVerifier* mock_cert_verifier() {
    return mock_cert_verifier_.mock_cert_verifier();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
};

#endif  // CHROME_BROWSER_SSL_CERT_VERIFIER_BROWSER_TEST_H_
