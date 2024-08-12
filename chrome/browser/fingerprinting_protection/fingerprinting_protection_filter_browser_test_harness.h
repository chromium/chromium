// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINGERPRINTING_PROTECTION_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_HARNESS_H_
#define CHROME_BROWSER_FINGERPRINTING_PROTECTION_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_HARNESS_H_

#include "build/build_config.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

class FingerprintingProtectionFilterBrowserTest : public PlatformBrowserTest {
 public:
  FingerprintingProtectionFilterBrowserTest();

  FingerprintingProtectionFilterBrowserTest(
      const FingerprintingProtectionFilterBrowserTest&) = delete;
  FingerprintingProtectionFilterBrowserTest& operator=(
      const FingerprintingProtectionFilterBrowserTest&) = delete;

  ~FingerprintingProtectionFilterBrowserTest() override;

 protected:
  // InProcessBrowserTest:
  void SetUp() override;
  void TearDown() override;
  void SetUpOnMainThread() override;
};

#endif  // CHROME_BROWSER_FINGERPRINTING_PROTECTION_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_HARNESS_H_
