// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/android/locale/locale_template_url_loader.h>
#include <stddef.h>

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

class MockLocaleTemplateUrlLoader : public LocaleTemplateUrlLoader {
 public:
  MockLocaleTemplateUrlLoader(std::string locale, TemplateURLService* service)
      : LocaleTemplateUrlLoader(locale, service) {}

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

  DISALLOW_COPY_AND_ASSIGN(LocaleTemplateUrlLoaderTest);
};

void LocaleTemplateUrlLoaderTest::SetUp() {
  test_util_.reset(new TemplateURLServiceTestUtil);
  loader_.reset(new MockLocaleTemplateUrlLoader("jp", model()));
}

void LocaleTemplateUrlLoaderTest::TearDown() {
  loader_.reset();
  test_util_.reset();
}

TEST_F(LocaleTemplateUrlLoaderTest, AddLocalSearchEngines) {
  test_util()->VerifyLoad();
  auto naver = base::ASCIIToUTF16("naver.com");
  auto keyword_so = base::ASCIIToUTF16("so.com");
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
  auto keyword_naver = base::ASCIIToUTF16("naver.com");
  auto keyword_so = base::ASCIIToUTF16("so.com");
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
