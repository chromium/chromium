// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"

FingerprintingProtectionFilterBrowserTest::
    FingerprintingProtectionFilterBrowserTest() = default;

FingerprintingProtectionFilterBrowserTest::
    ~FingerprintingProtectionFilterBrowserTest() = default;

void FingerprintingProtectionFilterBrowserTest::SetUp() {
  PlatformBrowserTest::SetUp();
}

void FingerprintingProtectionFilterBrowserTest::TearDown() {
  PlatformBrowserTest::TearDown();
}

void FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
}
