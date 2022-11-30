// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/cert_verifier_browser_test.h"

CertVerifierBrowserTest::CertVerifierBrowserTest() = default;

CertVerifierBrowserTest::~CertVerifierBrowserTest() = default;

void CertVerifierBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  mock_cert_verifier_.SetUpCommandLine(command_line);
}

void CertVerifierBrowserTest::SetUpInProcessBrowserTestFixture() {
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void CertVerifierBrowserTest::TearDownInProcessBrowserTestFixture() {
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}
