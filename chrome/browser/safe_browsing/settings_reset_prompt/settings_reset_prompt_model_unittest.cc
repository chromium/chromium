// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_test_utils.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

using testing::_;
using testing::Bool;
using testing::Combine;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::UnorderedElementsAre;

const char kHomepage[] = "http://myhomepage.com";
const char kDefaultSearch[] = "http://testsearch.com/?q={searchTerms}";
const char kStartupUrl1[] = "http://start1.com";
const char kStartupUrl2[] = "http://start2.com";
const char kStartupUrl3[] = "http://start3.com";

bool ListValueContainsUrl(const base::ListValue* list, const GURL& url) {
  if (!list)
    return false;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string url_text;
    if (list->GetString(i, &url_text) && url == url_text)
      return true;
  }
  return false;
}

class SettingsResetPromptModelTest
    : public extensions::ExtensionServiceTestBase {
 public:
  virtual void OnResetDone() { ++reset_callbacks_; }

 protected:
  using ModelPointer = std::unique_ptr<SettingsResetPromptModel>;

  SettingsResetPromptModelTest() : startup_pref_(SessionStartupPref::DEFAULT) {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();

    // By not specifying a pref_file filepath, we get a
    // sync_preferences::TestingPrefServiceSyncable, which in turn provides us
    // with a convient way of registring preferences.
    ExtensionServiceInitParams init_params = CreateDefaultInitParams();
    init_params.pref_file.clear();
    InitializeExtensionService(init_params);

#if !defined(OS_WIN)
    // In production code, the settings reset prompt profile preferences are
    // registered on Windows only. We explicitly register the prefs on
    // non-Windows systems so that we can continue testing the model on more
    // than just Windows.
    SettingsResetPromptPrefsManager::RegisterProfilePrefs(
        testing_pref_service()->registry());
#endif  // !defined(OS_WIN)

    profile_->CreateWebDataService();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&CreateTemplateURLServiceForTesting));

    SessionStartupPref::SetStartupPref(profile(), startup_pref_);

    prefs_ = profile()->GetPrefs();
    ASSERT_TRUE(prefs_);
  }

  void SetShowHomeButton(bool show_home_button) {
    prefs_->SetBoolean(prefs::kShowHomeButton, show_home_button);
  }

  void SetHomepageIsNTP(bool homepage_is_ntp) {
    prefs_->SetBoolean(prefs::kHomePageIsNewTabPage, homepage_is_ntp);
  }

  void SetHomepage(const std::string& homepage) {
    prefs_->SetString(prefs::kHomePage, homepage);
  }

  void SetDefaultSearch(const std::string& default_search) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(template_url_service);

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("TestEngine"));
    data.SetKeyword(base::ASCIIToUTF16("TestEngine"));
    data.SetURL(default_search);
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void SetStartupType(SessionStartupPref::Type startup_type) {
    startup_pref_.type = startup_type;
    SessionStartupPref::SetStartupPref(profile(), startup_pref_);
    ASSERT_EQ(SessionStartupPref::PrefValueToType(
                  prefs_->GetInteger(prefs::kRestoreOnStartup)),
              startup_pref_.type);
  }

  void AddStartupUrl(const std::string& startup_url) {
    GURL startup_gurl(startup_url);
    ASSERT_TRUE(startup_gurl.is_valid());

    startup_pref_.urls.push_back(startup_gurl);
    SessionStartupPref::SetStartupPref(profile(), startup_pref_);
    ASSERT_EQ(SessionStartupPref::PrefValueToType(
                  prefs_->GetInteger(prefs::kRestoreOnStartup)),
              startup_pref_.type);

    // Also make sure that the |startup_url| is now in the list of URLs in the
    // preferences.
    ASSERT_TRUE(ListValueContainsUrl(
        prefs_->GetList(prefs::kURLsToRestoreOnStartup), startup_gurl));
  }

  // Returns a model with a mock config that will return negative IDs for every
  // URL.
  ModelPointer CreateModel() {
    return CreateModelForTesting(profile(), std::unordered_set<std::string>(),
                                 nullptr);
  }

  // Returns a model with a mock config that will return positive IDs for each
  // URL in |reset_urls|.
  ModelPointer CreateModel(std::unordered_set<std::string> reset_urls) {
    return CreateModelForTesting(profile(), reset_urls, nullptr);
  }

  // Returns a model with a mock config that will return positive IDs for each
  // URL in |reset_urls|.
  ModelPointer CreateModel(std::unordered_set<std::string> reset_urls,
                           std::unique_ptr<ProfileResetter> profile_resetter) {
    return CreateModelForTesting(profile(), reset_urls,
                                 std::move(profile_resetter));
  }

  PrefService* prefs_;
  SessionStartupPref startup_pref_;
  int reset_callbacks_ = 0;
};

class ResetStatesTest
    : public SettingsResetPromptModelTest,
      public testing::WithParamInterface<testing::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override {
    SettingsResetPromptModelTest::SetUp();
    homepage_matches_config_ = testing::get<0>(GetParam());
    default_search_matches_config_ = testing::get<1>(GetParam());
    startup_urls_matches_config_ = testing::get<2>(GetParam());
    should_prompt_ = homepage_matches_config_ ||
                     default_search_matches_config_ ||
                     startup_urls_matches_config_;

    if (homepage_matches_config_) {
      SetShowHomeButton(true);
      SetHomepageIsNTP(false);
      SetHomepage(kHomepage);
    }

    if (default_search_matches_config_)
      SetDefaultSearch(kDefaultSearch);

    if (startup_urls_matches_config_) {
      SetStartupType(SessionStartupPref::URLS);
      AddStartupUrl(kStartupUrl1);
      AddStartupUrl(kStartupUrl2);
      AddStartupUrl(kStartupUrl3);
    }
  }

  bool homepage_matches_config_;
  bool default_search_matches_config_;
  bool startup_urls_matches_config_;
  bool should_prompt_;
};

TEST_F(SettingsResetPromptModelTest, Homepage) {
  SetHomepage(kHomepage);
  ModelPointer model = CreateModel();
  EXPECT_EQ(model->homepage(), GURL(kHomepage));
}

TEST_F(SettingsResetPromptModelTest, HomepageResetState) {
  SetHomepage(kHomepage);

  for (bool homepage_is_ntp : {false, true}) {
    for (bool show_home_button : {false, true}) {
      SetShowHomeButton(show_home_button);
      SetHomepageIsNTP(homepage_is_ntp);
      // Should return |NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED| when
      // |UrlToResetDomainId()| returns a negative integer.
      {
        ModelPointer model = CreateModel();
        EXPECT_EQ(model->homepage_reset_state(),
                  SettingsResetPromptModel::
                      NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
      }

      // When neither default search nor startup URLs need to be reset,
      // |homepage_reset_state()| should return |RESET_REQUIRED| when
      // |UrlToResetDomainId()| returns a positive integer and the home button
      // is visible and homepage is not set to the New Tab page, and
      // |NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED| otherwise.
      {
        ModelPointer model = CreateModel({kHomepage});
        EXPECT_EQ(model->homepage_reset_state(),
                  show_home_button && !homepage_is_ntp
                      ? SettingsResetPromptModel::RESET_REQUIRED
                      : SettingsResetPromptModel::
                            NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
      }
    }
  }
}

TEST_F(SettingsResetPromptModelTest, DefaultSearch) {
  SetDefaultSearch(kDefaultSearch);
  ModelPointer model = CreateModel();
  EXPECT_EQ(model->default_search(), GURL(kDefaultSearch));
}

TEST_F(SettingsResetPromptModelTest, StartupUrls) {
  // Should return empty list of startup URLs if startup type is not set to
  // |SessionStartupPref::URLS|.
  SetStartupType(SessionStartupPref::DEFAULT);
  {
    ModelPointer model = CreateModel();
    EXPECT_THAT(model->startup_urls(), IsEmpty());
  }

  AddStartupUrl(kStartupUrl1);
  {
    ModelPointer model = CreateModel();
    EXPECT_THAT(model->startup_urls(), IsEmpty());
  }

  SetStartupType(SessionStartupPref::LAST);
  {
    ModelPointer model = CreateModel();
    EXPECT_THAT(model->startup_urls(), IsEmpty());
  }

  // Should return the list of startup URLs if startup type is set to
  // |SessionStartupPref::URLS|.
  SetStartupType(SessionStartupPref::URLS);
  {
    ModelPointer model = CreateModel();
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl1)));
  }

  AddStartupUrl(kStartupUrl2);
  {
    ModelPointer model = CreateModel();
    EXPECT_THAT(model->startup_urls(),
                UnorderedElementsAre(GURL(kStartupUrl1), GURL(kStartupUrl2)));
  }
}

TEST_F(SettingsResetPromptModelTest, StartupUrlsToReset) {
  AddStartupUrl(kStartupUrl1);
  AddStartupUrl(kStartupUrl2);
  AddStartupUrl(kStartupUrl3);

  // Should return no URLs as long as startup type is not set to
  // |SessionStartupPref::URLS|.
  SetStartupType(SessionStartupPref::DEFAULT);
  {
    ModelPointer model = CreateModel({kStartupUrl2});
    EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
  }

  SetStartupType(SessionStartupPref::LAST);
  {
    ModelPointer model = CreateModel({kStartupUrl2});
    EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
  }

  // Should return the URLs that have a match in the config when startup type is
  // set to |SessionStartupPref::URLS|.
  SetStartupType(SessionStartupPref::URLS);
  {
    ModelPointer model = CreateModel({kStartupUrl2});
    EXPECT_THAT(model->startup_urls_to_reset(),
                ElementsAre(GURL(kStartupUrl2)));
  }

  {
    ModelPointer model = CreateModel({kStartupUrl1, kStartupUrl2});
    EXPECT_THAT(model->startup_urls_to_reset(),
                UnorderedElementsAre(GURL(kStartupUrl1), GURL(kStartupUrl2)));
  }
}

TEST_P(ResetStatesTest, SettingsResetStates) {
  std::unordered_set<std::string> reset_urls;
  if (homepage_matches_config_)
    reset_urls.insert(kHomepage);
  if (default_search_matches_config_)
    reset_urls.insert(kDefaultSearch);
  if (startup_urls_matches_config_)
    reset_urls.insert(kStartupUrl2);

  ModelPointer model = CreateModel(reset_urls);

  SettingsResetPromptModel::ResetState expected_search_reset_state =
      default_search_matches_config_
          ? SettingsResetPromptModel::RESET_REQUIRED
          : SettingsResetPromptModel::
                NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED;

  SettingsResetPromptModel::ResetState expected_startup_urls_reset_state =
      SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED;
  if (startup_urls_matches_config_) {
    expected_startup_urls_reset_state =
        default_search_matches_config_
            ? SettingsResetPromptModel::
                  NO_RESET_REQUIRED_DUE_TO_OTHER_SETTING_REQUIRING_RESET
            : SettingsResetPromptModel::RESET_REQUIRED;
  }

  SettingsResetPromptModel::ResetState expected_homepage_reset_state =
      SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED;
  if (homepage_matches_config_) {
    expected_homepage_reset_state =
        (default_search_matches_config_ || startup_urls_matches_config_)
            ? SettingsResetPromptModel::
                  NO_RESET_REQUIRED_DUE_TO_OTHER_SETTING_REQUIRING_RESET
            : SettingsResetPromptModel::RESET_REQUIRED;
  }

  EXPECT_EQ(model->default_search_reset_state(), expected_search_reset_state);
  EXPECT_EQ(model->startup_urls_reset_state(),
            expected_startup_urls_reset_state);
  EXPECT_EQ(model->homepage_reset_state(), expected_homepage_reset_state);
}

TEST_P(ResetStatesTest, ShouldPromptForReset) {
  std::unordered_set<std::string> reset_urls;
  if (homepage_matches_config_)
    reset_urls.insert(kHomepage);
  if (default_search_matches_config_)
    reset_urls.insert(kDefaultSearch);
  if (startup_urls_matches_config_)
    reset_urls.insert(kStartupUrl2);

  ModelPointer model = CreateModel(reset_urls);
  EXPECT_EQ(model->ShouldPromptForReset(), should_prompt_);
}

TEST_P(ResetStatesTest, PerformReset) {
  ProfileResetter::ResettableFlags expected_reset_flags = 0U;
  std::unordered_set<std::string> reset_urls;
  if (default_search_matches_config_)
    reset_urls.insert(kDefaultSearch);
  if (startup_urls_matches_config_)
    reset_urls.insert(kStartupUrl1);
  if (homepage_matches_config_)
    reset_urls.insert(kHomepage);

  if (default_search_matches_config_)
    expected_reset_flags = ProfileResetter::DEFAULT_SEARCH_ENGINE;
  else if (startup_urls_matches_config_)
    expected_reset_flags = ProfileResetter::STARTUP_PAGES;
  else if (homepage_matches_config_)
    expected_reset_flags = ProfileResetter::HOMEPAGE;

  auto profile_resetter =
      std::make_unique<NiceMock<MockProfileResetter>>(profile());
  EXPECT_CALL(*profile_resetter, MockReset(expected_reset_flags, _, _))
      .Times(1);

  ModelPointer model = CreateModel(reset_urls, std::move(profile_resetter));
  model->PerformReset(std::make_unique<BrandcodedDefaultSettings>(),
                      base::Bind(&SettingsResetPromptModelTest::OnResetDone,
                                 base::Unretained(this)));
  EXPECT_EQ(reset_callbacks_, 1);
}

INSTANTIATE_TEST_CASE_P(SettingsResetPromptModel,
                        ResetStatesTest,
                        Combine(Bool(), Bool(), Bool()));

}  // namespace
}  // namespace safe_browsing
