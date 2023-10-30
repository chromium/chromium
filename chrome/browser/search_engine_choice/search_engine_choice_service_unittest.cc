// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class SearchEngineChoiceServiceTest : public testing::Test {
 public:
  SearchEngineChoiceServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

    PrefService* pref_service = profile()->GetPrefs();
    // The search engine choice feature is only enabled for countries in the EEA
    // region.
    const int kBelgiumCountryId =
        country_codes::CountryCharsToCountryID('B', 'E');
    pref_service->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_engine_choice_service_ = std::make_unique<SearchEngineChoiceService>(
        *profile_, *template_url_service);
  }

  Profile* profile() const { return profile_; }

  SearchEngineChoiceService* search_engine_choice_service() {
    return search_engine_choice_service_.get();
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::test::ScopedFeatureList feature_list_{switches::kSearchEngineChoice};
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<SearchEngineChoiceService> search_engine_choice_service_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SearchEngineChoiceServiceTest, HandleLearnMoreLinkClicked) {
  search_engine_choice_service()->NotifyLearnMoreLinkClicked(
      SearchEngineChoiceService::EntryPoint::kDialog);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed,
      1);

  search_engine_choice_service()->NotifyLearnMoreLinkClicked(
      SearchEngineChoiceService::EntryPoint::kProfilePicker);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kFreLearnMoreWasDisplayed,
      1);
}
