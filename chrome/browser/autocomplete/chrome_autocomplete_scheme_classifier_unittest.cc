// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_storage_partition.h"

#include <memory>

#include "base/strings/strcat.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Test;

class ChromeAutocompleteSchemeClassifierTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    scheme_classifier_ =
        std::make_unique<ChromeAutocompleteSchemeClassifier>(profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeAutocompleteSchemeClassifier> scheme_classifier_;
};

TEST_F(ChromeAutocompleteSchemeClassifierTest, NormalSearch) {
  GURL url("pictures of puppies");
  // No url scheme; should default to search.
  EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme(url.scheme()),
            metrics::OmniboxInputType::EMPTY);
}
TEST_F(ChromeAutocompleteSchemeClassifierTest, HttpUrl) {
  GURL url("https://google.com/search?q=puppies");

  EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme(url.scheme()),
            metrics::OmniboxInputType::URL);
}

TEST_F(ChromeAutocompleteSchemeClassifierTest, BlockedScheme) {
  GURL url("shell://foo");
  // This should be blocked from running as a URL.
  EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme(url.scheme()),
            metrics::OmniboxInputType::QUERY);
}

// Can't test registered apps handling with mocking of shell_integration
// because shell_integration is implemented via namespace shell_integration.
