// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/locale/locale_template_url_loader.h"

#include <stddef.h>

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestCountryCode[] = "JP";

class MockLocaleTemplateUrlLoader : public LocaleTemplateUrlLoader {
 public:
  MockLocaleTemplateUrlLoader(std::string locale,
                              TemplateURLService* service,
                              Profile* profile)
      : LocaleTemplateUrlLoader(locale, service, profile) {}

  ~MockLocaleTemplateUrlLoader() override {}

 protected:
  std::vector<std::unique_ptr<TemplateURLData>> GetLocalPrepopulatedEngines()
      override {
    std::vector<std::unique_ptr<TemplateURLData>> result;
    result.push_back(TemplateURLDataFromPrepopulatedEngine(
        TemplateURLPrepopulateData::so_360));
    result.push_back(TemplateURLDataFromPrepopulatedEngine(
        TemplateURLPrepopulateData::naver));
    result.push_back(TemplateURLDataFromPrepopulatedEngine(
        TemplateURLPrepopulateData::google));
    return result;
  }

  int GetDesignatedSearchEngineForChina() override {
    return TemplateURLPrepopulateData::naver.id;
  }
};

class LocaleTemplateUrlLoaderTest : public testing::Test {
 public:
  LocaleTemplateUrlLoaderTest() {}

  LocaleTemplateUrlLoaderTest(const LocaleTemplateUrlLoaderTest&) = delete;
  LocaleTemplateUrlLoaderTest& operator=(const LocaleTemplateUrlLoaderTest&) =
      delete;

  void SetUp() override;
  void TearDown() override;
  LocaleTemplateUrlLoader* loader() { return loader_.get(); }
  TemplateURLServiceTestUtil* test_util() { return test_util_.get(); }
  TemplateURLService* model() { return test_util_->model(); }

 private:
  content::BrowserTaskEnvironment
      task_environment_;  // To set up BrowserThreads.
  std::unique_ptr<LocaleTemplateUrlLoader> loader_;
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;
};

void LocaleTemplateUrlLoaderTest::SetUp() {
  test_util_ = std::make_unique<TemplateURLServiceTestUtil>();
  loader_ = std::make_unique<MockLocaleTemplateUrlLoader>(
      kTestCountryCode, model(), test_util()->profile());
}

void LocaleTemplateUrlLoaderTest::TearDown() {
  loader_.reset();
  test_util_.reset();
}

TEST_F(LocaleTemplateUrlLoaderTest, AddLocalSearchEngines) {
  test_util()->VerifyLoad();
  std::u16string naver = u"naver.com";
  std::u16string keyword_so = u"so.com";
  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(naver));
  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(keyword_so));

  ASSERT_TRUE(loader()->LoadTemplateUrls(nullptr));

  EXPECT_EQ(TemplateURLPrepopulateData::naver.id,
            model()->GetTemplateURLForKeyword(naver)->prepopulate_id());
  EXPECT_EQ(TemplateURLPrepopulateData::so_360.id,
            model()->GetTemplateURLForKeyword(keyword_so)->prepopulate_id());

  // Ensure multiple calls to Load do not duplicate the search engines.
  size_t existing_size = model()->GetTemplateURLs().size();
  ASSERT_TRUE(loader()->LoadTemplateUrls(nullptr));
  EXPECT_EQ(existing_size, model()->GetTemplateURLs().size());
}

TEST_F(LocaleTemplateUrlLoaderTest, RemoveLocalSearchEngines) {
  test_util()->VerifyLoad();
  ASSERT_TRUE(loader()->LoadTemplateUrls(nullptr));
  // Make sure locale engines are loaded.
  std::u16string keyword_naver = u"naver.com";
  std::u16string keyword_so = u"so.com";
  ASSERT_EQ(TemplateURLPrepopulateData::naver.id,
            model()->GetTemplateURLForKeyword(keyword_naver)->prepopulate_id());
  ASSERT_EQ(TemplateURLPrepopulateData::so_360.id,
            model()->GetTemplateURLForKeyword(keyword_so)->prepopulate_id());

  loader()->RemoveTemplateUrls(nullptr);

  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(keyword_naver));
  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(keyword_so));
}

TEST_F(LocaleTemplateUrlLoaderTest, OverrideDefaultSearch) {
  test_util()->VerifyLoad();
  ASSERT_EQ(TemplateURLPrepopulateData::google.id,
            model()->GetDefaultSearchProvider()->prepopulate_id());
  // Load local search engines first.
  ASSERT_TRUE(loader()->LoadTemplateUrls(nullptr));

  ASSERT_EQ(TemplateURLPrepopulateData::google.id,
            model()->GetDefaultSearchProvider()->prepopulate_id());

  // Set one of the local search engine as default.
  loader()->OverrideDefaultSearchProvider(nullptr);
  ASSERT_EQ(TemplateURLPrepopulateData::naver.id,
            model()->GetDefaultSearchProvider()->prepopulate_id());

  // Revert the default search engine tweak.
  loader()->SetGoogleAsDefaultSearch(nullptr);
  ASSERT_EQ(TemplateURLPrepopulateData::google.id,
            model()->GetDefaultSearchProvider()->prepopulate_id());
}

TEST_F(LocaleTemplateUrlLoaderTest, GetLocalPrepopulatedEngines) {
  auto expected_engines =
      TemplateURLPrepopulateData::GetLocalPrepopulatedEngines(
          kTestCountryCode, *test_util()->profile()->GetPrefs());

  // Creating a prod class instance to call the real implementation for
  // `GetLocalPrepopulatedEngines()`.
  auto loader = std::make_unique<LocaleTemplateUrlLoader>(
      kTestCountryCode, model(), test_util()->profile());
  auto actual_engines = loader->GetLocalPrepopulatedEngines();

  ASSERT_EQ(actual_engines.size(), expected_engines.size());
  for (size_t i = 0; i < actual_engines.size(); ++i) {
    EXPECT_EQ(actual_engines[i]->keyword(), expected_engines[i]->keyword());
  }
}

TEST_F(LocaleTemplateUrlLoaderTest, OnProfileWillBeDestroyed) {
  auto loader = std::make_unique<LocaleTemplateUrlLoader>(
      kTestCountryCode, model(), test_util()->profile());

  loader->OnProfileWillBeDestroyed(test_util()->profile());

  // For coverage of the fallbacks from b/317335096, the following calls should
  // not crash and return "harmless" values after we report that the profile is
  // destroying.
  loader->LoadTemplateUrls(/*env=*/nullptr);
  loader->RemoveTemplateUrls(/*env=*/nullptr);
  loader->OverrideDefaultSearchProvider(/*env=*/nullptr);
  loader->SetGoogleAsDefaultSearch(/*env=*/nullptr);
  EXPECT_TRUE(loader->GetLocalPrepopulatedEngines().empty());
  EXPECT_GT(loader->GetDesignatedSearchEngineForChina(), 0);
}
