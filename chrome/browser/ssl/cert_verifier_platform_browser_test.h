// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERT_VERIFIER_PLATFORM_BROWSER_TEST_H_
#define CHROME_BROWSER_SSL_CERT_VERIFIER_PLATFORM_BROWSER_TEST_H_

#include "build/build_config.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"

// CertVerifierPlatformBrowserTest allows tests to force certificate
// verification results for requests made with any profile's main
// request context (such as navigations). To do so, tests can use the
// MockCertVerifier exposed via
// CertVerifierPlatformBrowserTest::mock_cert_verifier().
//
// In contrast to CertVerifierBrowserTest, which only works with browser_tests,
// CertVerifierPlatformBrowserTest is platform-agnostic, and can run in both
// browser_tests and android_browsertests. However, care must be taken to
// avoid technical debt, as documented in
// https://groups.google.com/a/chromium.org/d/msg/chromium-dev/E_wqfkuO3JQ/opIZSZaEFAAJ
class CertVerifierPlatformBrowserTest : public PlatformBrowserTest {
 public:
  CertVerifierPlatformBrowserTest();
  ~CertVerifierPlatformBrowserTest() override;

  CertVerifierPlatformBrowserTest(const CertVerifierPlatformBrowserTest&) =
      delete;
  CertVerifierPlatformBrowserTest& operator=(
      const CertVerifierPlatformBrowserTest&) = delete;

  // PlatformBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

  content::ContentMockCertVerifier::CertVerifier* mock_cert_verifier() {
    return mock_cert_verifier_.mock_cert_verifier();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
};

#endif  // CHROME_BROWSER_SSL_CERT_VERIFIER_PLATFORM_BROWSER_TEST_H_
