// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/types/expected.h"
#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "url/gurl.h"

using page_load_metrics::CaptchaProviderManager;

struct IcuEnvironment {
  IcuEnvironment() {
    // Initialize ICU, which is required for GURL parsing.
    CHECK(base::i18n::InitializeICU());
  }
  // Used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

void SetCaptchaProvidersDoesNotCrash(std::vector<std::string> input) {
  CaptchaProviderManager manager = CaptchaProviderManager::CreateForTesting();
  manager.SetCaptchaProviders(input);
  EXPECT_TRUE(manager.loaded());
}

void IsCaptchaUrlDoesNotCrash(std::string input) {
  CaptchaProviderManager manager = CaptchaProviderManager::CreateForTesting();
  // Set some providers, covering the different pattern types.
  std::vector<std::string> providers = {
      "provider1.com", "provider2.com/captcha", "provider3.com/page/*",
      "*provider4.com/captcha", "*sub.provider5.com/*"};
  manager.SetCaptchaProviders(providers);
  EXPECT_TRUE(manager.loaded());
  EXPECT_FALSE(manager.empty());

  // Test a variety of URLs, including invalid ones.
  GURL url(input);
  manager.IsCaptchaUrl(url);
}

FUZZ_TEST(CaptchaProviderManagerFuzzTests, SetCaptchaProvidersDoesNotCrash);
FUZZ_TEST(CaptchaProviderManagerFuzzTests, IsCaptchaUrlDoesNotCrash);
