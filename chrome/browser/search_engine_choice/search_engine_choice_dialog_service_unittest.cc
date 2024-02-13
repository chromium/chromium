// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/dialog_test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if !BUILDFLAG(CHROME_FOR_TESTING)
void SetUserSelectedDefaultSearchProvider(
    TemplateURLService* template_url_service,
    bool created_by_policy) {
  constexpr char kCustomSearchEngineDomain[] = "bar.com";
  constexpr char16_t kCustomSearchEngineKeyword[] = u"bar.com";

  TemplateURLData data;
  data.SetShortName(kCustomSearchEngineKeyword);
  data.SetKeyword(kCustomSearchEngineKeyword);
  data.SetURL(base::StringPrintf("https://%s/url?bar={searchTerms}",
                                 kCustomSearchEngineDomain));
  data.new_tab_url =
      base::StringPrintf("https://%s/newtab", kCustomSearchEngineDomain);
  data.alternate_urls.push_back(base::StringPrintf(
      "https://%s/alt#quux={searchTerms}", kCustomSearchEngineDomain));

  if (created_by_policy) {
    data.created_by_policy =
        TemplateURLData::CreatedByPolicy::kDefaultSearchProvider;
  }

  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
}
#endif

}  // namespace

class SearchEngineChoiceDialogServiceTest : public BrowserWithTestWindowTest {
 public:
  SearchEngineChoiceDialogServiceTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        switches::kSearchEngineChoiceTrigger,
        {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
          "false"}});
    scoped_chrome_build_override_ = std::make_unique<base::AutoReset<bool>>(
        SearchEngineChoiceDialogServiceFactory::
            ScopedChromeBuildOverrideForTesting(
                /*force_chrome_build=*/true));
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

    // The search engine choice feature is only enabled for countries in the
    // EEA region.
    const int kBelgiumCountryId =
        country_codes::CountryCharsToCountryID('B', 'E');
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry,
        country_codes::CountryIDToCountryString(kBelgiumCountryId));
  }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    // Dialog eligibility checks require a `WebContentsModalDialogHost`.
    return std::make_unique<DialogTestBrowserWindow>();
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  const base::UserActionTester& user_action_tester() const {
    return user_action_tester_;
  }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  std::unique_ptr<base::AutoReset<bool>> scoped_chrome_build_override_;
};

#if !BUILDFLAG(CHROME_FOR_TESTING)
TEST_F(SearchEngineChoiceDialogServiceTest, HandleLearnMoreLinkClicked) {
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());

  search_engine_choice_dialog_service->NotifyLearnMoreLinkClicked(
      SearchEngineChoiceDialogService::EntryPoint::kDialog);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed,
      1);

  search_engine_choice_dialog_service->NotifyLearnMoreLinkClicked(
      SearchEngineChoiceDialogService::EntryPoint::kFirstRunExperience);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kFreLearnMoreWasDisplayed,
      1);

  search_engine_choice_dialog_service->NotifyLearnMoreLinkClicked(
      SearchEngineChoiceDialogService::EntryPoint::kProfileCreation);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::
          kProfileCreationLearnMoreDisplayed,
      1);
}

TEST_F(SearchEngineChoiceDialogServiceTest, CanShowDialog) {
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(search_engine_choice_dialog_service);

  // The `DialogTestBrowserWindow` reports a {0,0} size window.
  EXPECT_FALSE(search_engine_choice_dialog_service->CanShowDialog(*browser()));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::
          kBrowserWindowTooSmall,
      1);
}

TEST_F(SearchEngineChoiceDialogServiceTest, NotifyDialogOpened) {
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(search_engine_choice_dialog_service);

  search_engine_choice_dialog_service->NotifyDialogOpened(browser(),
                                                          base::DoNothing());
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kChoiceScreenWasDisplayed,
      1);

  EXPECT_EQ(
      user_action_tester().GetActionCount("SearchEngineChoiceScreenShown"), 1);
}

TEST_F(SearchEngineChoiceDialogServiceTest, NotifyChoiceMade) {
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(search_engine_choice_dialog_service);

  search_engine_choice_dialog_service->NotifyChoiceMade(
      TemplateURLPrepopulateData::google.id,
      SearchEngineChoiceDialogService::EntryPoint::kDialog);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet, 1);
  // Recorded when we call `SetUserSelectedDefaultSearchProvider()`.
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);

  search_engine_choice_dialog_service->NotifyChoiceMade(
      TemplateURLPrepopulateData::google.id,
      SearchEngineChoiceDialogService::EntryPoint::kFirstRunExperience);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet, 1);

  search_engine_choice_dialog_service->NotifyChoiceMade(
      TemplateURLPrepopulateData::google.id,
      SearchEngineChoiceDialogService::EntryPoint::kProfileCreation);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::
          kProfileCreationDefaultWasSet,
      1);
}

TEST_F(SearchEngineChoiceDialogServiceTest, NotifyChoiceMade_Unknown) {
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id =
      TemplateURLPrepopulateData::kMaxPrepopulatedEngineID + 1;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  template_url_service->SetUserSelectedDefaultSearchProvider(
      template_url_service->Add(
          std::make_unique<TemplateURL>(template_url_data)));

  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(search_engine_choice_dialog_service);

  search_engine_choice_dialog_service->NotifyChoiceMade(
      TemplateURLPrepopulateData::kMaxPrepopulatedEngineID + 1,
      SearchEngineChoiceDialogService::EntryPoint::kDialog);
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet, 1);

  // We don't end up calling `SetUserSelectedDefaultSearchProvider()` so this
  // doesn't get recorded.
  histogram_tester().ExpectTotalCount(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      0);
}

TEST_F(SearchEngineChoiceDialogServiceTest,
       DoNotDisplayDialogIfPolicyIsSetDynamically) {
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(search_engine_choice_dialog_service);

  SetUserSelectedDefaultSearchProvider(
      TemplateURLServiceFactory::GetForProfile(profile()),
      /*created_by_policy=*/true);
  EXPECT_FALSE(search_engine_choice_dialog_service->CanShowDialog(*browser()));
}

TEST_F(SearchEngineChoiceDialogServiceTest, DoNotCreateServiceIfPolicyIsSet) {
  SetUserSelectedDefaultSearchProvider(
      TemplateURLServiceFactory::GetForProfile(profile()),
      /*created_by_policy=*/true);

  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());
  EXPECT_FALSE(search_engine_choice_dialog_service);
}

#else
TEST_F(SearchEngineChoiceDialogServiceTest,
       ServiceNotInitializedInChromeForTesting) {
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile());
  ASSERT_EQ(search_engine_choice_dialog_service, nullptr);
}
#endif
