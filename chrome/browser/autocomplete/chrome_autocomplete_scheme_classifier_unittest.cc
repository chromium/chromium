// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/external_protocol/features.h"
#endif

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
  EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme(url.GetScheme()),
            metrics::OmniboxInputType::EMPTY);
}
TEST_F(ChromeAutocompleteSchemeClassifierTest, HttpUrl) {
  GURL url("https://google.com/search?q=puppies");

  EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme(url.GetScheme()),
            metrics::OmniboxInputType::URL);
}

TEST_F(ChromeAutocompleteSchemeClassifierTest, BlockedScheme) {
  GURL url("shell://foo");
  // This should be blocked from running as a URL.
  EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme(url.GetScheme()),
            metrics::OmniboxInputType::QUERY);
}

TEST_F(ChromeAutocompleteSchemeClassifierTest, DevToolsUrl) {
  GURL url("devtools://foo");
  // An internal scheme should also be recognized as a URL.
  EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme(url.GetScheme()),
            metrics::OmniboxInputType::URL);
}

// Can't test registered apps handling with mocking of shell_integration
// because shell_integration is implemented via namespace shell_integration.

#if BUILDFLAG(IS_LINUX)
class ChromeAutocompleteSchemeClassifierXdgTest
    : public ChromeAutocompleteSchemeClassifierTest,
      public testing::WithParamInterface<bool> {
 public:
  ChromeAutocompleteSchemeClassifierXdgTest() {
    feature_list_.InitWithFeatureState(features::kXdgSchemeHandlerDetection,
                                       GetParam());
  }

  void SetUp() override {
    // Point the XDG variables at fake directories before the base class
    // creates the classifier, which starts reading them on the thread pool.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath config_home = temp_dir_.GetPath().Append("config");
    base::FilePath data_home = temp_dir_.GetPath().Append("data");
    base::FilePath apps = data_home.Append("applications");
    ASSERT_TRUE(base::CreateDirectory(config_home));
    ASSERT_TRUE(base::CreateDirectory(apps));
    ASSERT_TRUE(base::WriteFile(apps.Append("orchtest.desktop"),
                                "[Desktop Entry]\nType=Application\n"));
    ASSERT_TRUE(
        base::WriteFile(config_home.Append("mimeapps.list"),
                        "[Default Applications]\n"
                        "x-scheme-handler/orchtest=orchtest.desktop\n"));
    env_overrides_.emplace_back("XDG_CONFIG_HOME", config_home.value());
    env_overrides_.emplace_back("XDG_CONFIG_DIRS",
                                temp_dir_.GetPath().Append("none").value());
    env_overrides_.emplace_back("XDG_DATA_HOME", data_home.value());
    env_overrides_.emplace_back("XDG_DATA_DIRS",
                                temp_dir_.GetPath().Append("none").value());

    ChromeAutocompleteSchemeClassifierTest::SetUp();
  }

  void TearDown() override {
    // Let the background reader finish before the environment is restored.
    scheme_classifier_.reset();
    task_environment_.RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::vector<base::ScopedEnvironmentVariableOverride> env_overrides_;
};

// A scheme with an XDG "x-scheme-handler" association is a URL; one without
// is still a search (crbug.com/40428807).
TEST_P(ChromeAutocompleteSchemeClassifierXdgTest, RegisteredScheme) {
  if (GetParam()) {
    // The XDG associations are read asynchronously.
    EXPECT_TRUE(base::test::RunUntil([&] {
      return scheme_classifier_->GetInputTypeForScheme("orchtest") ==
             metrics::OmniboxInputType::URL;
    }));
  } else {
    EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme("orchtest"),
              metrics::OmniboxInputType::EMPTY);
  }
  EXPECT_EQ(scheme_classifier_->GetInputTypeForScheme("unknownzz"),
            metrics::OmniboxInputType::EMPTY);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeAutocompleteSchemeClassifierXdgTest,
                         testing::Bool());
#endif  // BUILDFLAG(IS_LINUX)
