// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

class SearchEngineChoiceServiceTest : public BrowserWithTestWindowTest {
 public:
  SearchEngineChoiceServiceTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kSearchEngineChoice,
                              switches::kSearchEngineChoiceFre},
        /*disabled_features=*/{});

    scoped_chrome_build_override_ = std::make_unique<base::AutoReset<bool>>(
        SearchEngineChoiceServiceFactory::ScopedChromeBuildOverrideForTesting(
            /*force_chrome_build=*/true));
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

    PrefService* pref_service = profile()->GetPrefs();
    // The search engine choice feature is only enabled for countries in the
    // EEA region.
    const int kBelgiumCountryId =
        country_codes::CountryCharsToCountryID('B', 'E');
    pref_service->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<base::AutoReset<bool>> scoped_chrome_build_override_;
};

TEST_F(SearchEngineChoiceServiceTest, HandleLearnMoreLinkClicked) {
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile());

  search_engine_choice_service->NotifyLearnMoreLinkClicked(
      SearchEngineChoiceService::EntryPoint::kDialog);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed,
      1);

  search_engine_choice_service->NotifyLearnMoreLinkClicked(
      SearchEngineChoiceService::EntryPoint::kFirstRunExperience);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kFreLearnMoreWasDisplayed,
      1);

  search_engine_choice_service->NotifyLearnMoreLinkClicked(
      SearchEngineChoiceService::EntryPoint::kProfileCreation);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::
          kProfileCreationLearnMoreDisplayed,
      1);
}

TEST_F(SearchEngineChoiceServiceTest, CanShowDialog) {
  feature_list().Reset();
  feature_list().InitWithFeatures(
      /*enabled_features=*/{switches::kSearchEngineChoiceFre},
      /*disabled_features=*/{switches::kSearchEngineChoice});

  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile());

  EXPECT_FALSE(search_engine_choice_service->CanShowDialog(*browser()));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kFeatureSuppressed,
      1);
}

TEST_F(SearchEngineChoiceServiceTest, NotifyChoiceMade) {
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile());

  search_engine_choice_service->NotifyChoiceMade(
      TemplateURLPrepopulateData::google.id,
      SearchEngineChoiceService::EntryPoint::kDialog);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet, 1);

  search_engine_choice_service->NotifyChoiceMade(
      TemplateURLPrepopulateData::google.id,
      SearchEngineChoiceService::EntryPoint::kFirstRunExperience);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet, 1);

  search_engine_choice_service->NotifyChoiceMade(
      TemplateURLPrepopulateData::google.id,
      SearchEngineChoiceService::EntryPoint::kProfileCreation);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::
          kProfileCreationDefaultWasSet,
      1);
}
