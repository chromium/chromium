// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace {

class TestSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  TestSchemeClassifier() = default;
  ~TestSchemeClassifier() override = default;

  // Overridden from AutocompleteInputSchemeClassifier:
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSchemeClassifier);
};

metrics::OmniboxInputType TestSchemeClassifier::GetInputTypeForScheme(
    const std::string& scheme) const {
  return scheme.empty() ? metrics::OmniboxInputType::EMPTY
                        : metrics::OmniboxInputType::URL;
}

}  // namespace

class ChromeAutocompleteProviderClientTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    client_ =
        std::make_unique<ChromeAutocompleteProviderClient>(profile_.get());
    storage_partition_.set_service_worker_context(&service_worker_context_);
    client_->set_storage_partition(&storage_partition_);
  }

  // Replaces the client with one using an incognito profile. Note that this is
  // a one-way operation. Once a TEST_F calls this, all interactions with
  // |client_| will be off the record.
  void GoOffTheRecord() {
    client_ = std::make_unique<ChromeAutocompleteProviderClient>(
        profile_->GetOffTheRecordProfile());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeAutocompleteProviderClient> client_;
  content::FakeServiceWorkerContext service_worker_context_;

 private:
  content::TestStoragePartition storage_partition_;
};

TEST_F(ChromeAutocompleteProviderClientTest, StartServiceWorker) {
  GURL destination_url("https://google.com/search?q=puppies");

  client_->StartServiceWorker(destination_url);
  EXPECT_TRUE(service_worker_context_
                  .start_service_worker_for_navigation_hint_called());
}

TEST_F(ChromeAutocompleteProviderClientTest,
       DontStartServiceWorkerInIncognito) {
  GURL destination_url("https://google.com/search?q=puppies");

  GoOffTheRecord();
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}

TEST_F(ChromeAutocompleteProviderClientTest,
       DontStartServiceWorkerIfSuggestDisabled) {
  GURL destination_url("https://google.com/search?q=puppies");

  profile_->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}

TEST_F(ChromeAutocompleteProviderClientTest, TestStrippedURLsAreEqual) {
  struct {
    const char* url1;
    const char* url2;
    const char* input;
    bool equal;
  } test_cases[] = {
      // Sanity check cases.
      {"http://google.com", "http://google.com", "", true},
      {"http://google.com", "http://www.google.com", "", true},
      {"http://google.com", "http://facebook.com", "", false},
      {"http://google.com", "https://google.com", "", true},
      // Because we provided scheme, must match in scheme.
      {"http://google.com", "https://google.com", "http://google.com", false},
      {"https://www.apple.com/", "http://www.apple.com/",
       "https://www.apple.com/", false},
      {"https://www.apple.com/", "https://www.apple.com/",
       "https://www.apple.com/", true},
      // Ignore ref if not in input.
      {"http://drive.google.com/doc/blablabla#page=10",
       "http://drive.google.com/doc/blablabla#page=111", "", true},
      {"http://drive.google.com/doc/blablabla#page=10",
       "http://drive.google.com/doc/blablabla#page=111",
       "http://drive.google.com/doc/blablabla", true},
      {"file:///usr/local/bin/tuxpenguin#ref1",
       "file:///usr/local/bin/tuxpenguin#ref2", "", true},
      {"file:///usr/local/bin/tuxpenguin#ref1",
       "file:///usr/local/bin/tuxpenguin#ref2",
       "file:///usr/local/bin/tuxpenguin", true},
      // Do not ignore ref if in input.
      {"http://drive.google.com/doc/blablabla#page=10",
       "http://drive.google.com/doc/blablabla#page=111",
       "http://drive.google.com/doc/blablabla#p", false},
      {"file:///usr/local/bin/tuxpenguin#ref1",
       "file:///usr/local/bin/tuxpenguin#ref2",
       "file:///usr/local/bin/tuxpenguin#r", false}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(std::string(test_case.url1) + " vs " + test_case.url2 +
                 ", input '" + test_case.input + "'");
    AutocompleteInput input(base::ASCIIToUTF16(test_case.input),
                            test_case.input[0]
                                ? metrics::OmniboxEventProto::OTHER
                                : metrics::OmniboxEventProto::BLANK,
                            TestSchemeClassifier());
    EXPECT_EQ(test_case.equal,
              client_->StrippedURLsAreEqual(GURL(test_case.url1),
                                            GURL(test_case.url2), &input));
  }
}

// TODO(crbug/950007): Remove this test when the split settings work is complete
#if defined(OS_CHROMEOS)
TEST_F(ChromeAutocompleteProviderClientTest, OSSettingsDoNotShowUp) {
  size_t builtin_urls_with_os_settings;
  size_t builtin_urls_without_os_settings;

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(chromeos::features::kSplitSettings);
    builtin_urls_with_os_settings = client_->GetBuiltinURLs().size();
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kSplitSettings);
    builtin_urls_without_os_settings = client_->GetBuiltinURLs().size();
  }

  EXPECT_GT(builtin_urls_with_os_settings, builtin_urls_without_os_settings);
}
#endif
