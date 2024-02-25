// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_service/chrome_prefetch_service_delegate.h"

#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromePrefetchServiceDelegateTest : public ::testing::Test {
 protected:
  void SetUp() override { factory_util_.VerifyLoad(); }

  TestingProfile& profile() { return profile_; }
  TemplateURLService* template_url_service() { return factory_util_.model(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TemplateURLServiceFactoryTestUtil factory_util_{&profile_};
};

TEST_F(ChromePrefetchServiceDelegateTest,
       DefaultSearchEngineIsContaminationExempt) {
  TemplateURLData data;
  data.SetShortName(u"Sherlock");
  data.SetKeyword(u"sherlock");
  data.SetURL("https://sherlock.example/?q={searchTerms}");
  TemplateURL* search_engine =
      template_url_service()->Add(std::make_unique<TemplateURL>(data));
  template_url_service()->SetUserSelectedDefaultSearchProvider(search_engine);

  ChromePrefetchServiceDelegate delegate(&profile());
  EXPECT_TRUE(delegate.IsContaminationExempt(
      GURL("https://sherlock.example/?q=professor+moriarty")));
  EXPECT_FALSE(
      delegate.IsContaminationExempt(GURL("https://another.example/")));
}
