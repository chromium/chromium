// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/cert_verifier_platform_browser_test.h"

CertVerifierPlatformBrowserTest::CertVerifierPlatformBrowserTest() = default;

CertVerifierPlatformBrowserTest::~CertVerifierPlatformBrowserTest() = default;

void CertVerifierPlatformBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  mock_cert_verifier_.SetUpCommandLine(command_line);
}

void CertVerifierPlatformBrowserTest::SetUpInProcessBrowserTestFixture() {
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void CertVerifierPlatformBrowserTest::TearDownInProcessBrowserTestFixture() {
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}
