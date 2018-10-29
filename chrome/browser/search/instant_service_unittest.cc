// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/section_type.h"
#include "components/search/url_validity_checker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::StrictMock;
using testing::_;

using InstantServiceTest = InstantUnitTestBase;

namespace {

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  return std::move(std::get<k>(args)).Run(p0, p1);
}

class MockUrlValidityChecker : public UrlValidityChecker {
 public:
  MockUrlValidityChecker() {}
  ~MockUrlValidityChecker() override {}

  void DoesUrlResolve(const GURL& url,
                      net::NetworkTrafficAnnotationTag traffic_annotation,
                      UrlValidityCheckerCallback callback) override {
    DoDoesUrlResolve(url, traffic_annotation, callback);
  }
  MOCK_METHOD3(DoDoesUrlResolve,
               void(const GURL& url,
                    net::NetworkTrafficAnnotationTag traffic_annotation,
                    UrlValidityCheckerCallback& callback));
};

}  // namespace

class InstantServiceTestCustomLinksEnabled : public InstantServiceTest {
 public:
  InstantServiceTestCustomLinksEnabled() {
    scoped_feature_list_.InitAndEnableFeature(ntp_tiles::kNtpCustomLinks);
  }
  ~InstantServiceTestCustomLinksEnabled() override {}

  void SetUp() override {
    InstantServiceTest::SetUp();
    instant_service_->SetUrlValidityCheckerForTesting(&mock_url_checker_);
  }

  StrictMock<MockUrlValidityChecker> mock_url_checker_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(InstantServiceTestCustomLinksEnabled);
};

class InstantServiceTestCustomBackgroundsEnabled : public InstantServiceTest {
 public:
  InstantServiceTestCustomBackgroundsEnabled() {
    scoped_feature_list_.InitAndEnableFeature(features::kNtpBackgrounds);
  }
  ~InstantServiceTestCustomBackgroundsEnabled() override {}

  base::DictionaryValue GetBackgroundInfoAsDict(const GURL& background_url) {
    base::DictionaryValue background_info;
    background_info.SetKey("background_url",
                           base::Value(background_url.spec()));
    background_info.SetKey("attribution_line_1", base::Value(std::string()));
    background_info.SetKey("attribution_line_2", base::Value(std::string()));
    background_info.SetKey("attribution_action_url",
                           base::Value(std::string()));

    return background_info;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(InstantServiceTestCustomBackgroundsEnabled);
};

TEST_F(InstantServiceTest, GetNTPTileSuggestion) {
  ntp_tiles::NTPTile some_tile;
  some_tile.source = ntp_tiles::TileSource::TOP_SITES;
  some_tile.title_source = ntp_tiles::TileTitleSource::TITLE_TAG;
  ntp_tiles::NTPTilesVector suggestions{some_tile};

  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> suggestions_map;
  suggestions_map[ntp_tiles::SectionType::PERSONALIZED] = suggestions;

  instant_service_->OnURLsAvailable(suggestions_map);

  auto items = instant_service_->most_visited_items_;
  ASSERT_EQ(1, (int)items.size());
  EXPECT_EQ(ntp_tiles::TileSource::TOP_SITES, items[0].source);
  EXPECT_EQ(ntp_tiles::TileTitleSource::TITLE_TAG, items[0].title_source);
}

TEST_F(InstantServiceTestCustomLinksEnabled,
       DisableUndoCustomLinkActionForNonGoogleSearchProvider) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  EXPECT_TRUE(instant_service_->UndoCustomLinkAction());

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->UndoCustomLinkAction());
}

TEST_F(InstantServiceTestCustomLinksEnabled,
       DisableResetCustomLinksForNonGoogleSearchProvider) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  EXPECT_TRUE(instant_service_->ResetCustomLinks());

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  EXPECT_FALSE(instant_service_->ResetCustomLinks());
}

TEST_F(InstantServiceTestCustomLinksEnabled, DoesUrlResolve_OnSuccess) {
  const GURL kUrl("https://www.foo.com");
  base::TimeDelta expected_duration;

  EXPECT_CALL(mock_url_checker_, DoDoesUrlResolve(kUrl, _, _))
      .WillOnce(InvokeCallbackArgument<2>(true, expected_duration));

  base::MockCallback<base::OnceCallback<void(bool, bool)>> callback;
  EXPECT_CALL(callback, Run(true, false));
  instant_service_->DoesUrlResolve(kUrl, callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(InstantServiceTestCustomLinksEnabled, DoesUrlResolve_OnFailure) {
  const GURL kUrl("https://www.foo.com");
  const int kTimeAdvance = 20;
  base::TimeDelta expected_duration;
  base::TimeDelta expected_duration_timeout =
      base::TimeDelta::FromSeconds(kTimeAdvance);

  EXPECT_CALL(mock_url_checker_, DoDoesUrlResolve(kUrl, _, _))
      .WillOnce(InvokeCallbackArgument<2>(false, expected_duration));

  base::MockCallback<base::OnceCallback<void(bool, bool)>> callback;
  EXPECT_CALL(callback, Run(false, false));
  instant_service_->DoesUrlResolve(kUrl, callback.Get());
  base::RunLoop().RunUntilIdle();

  // Fake a timeout. Now InstantService should internally update the link and
  // return true.
  EXPECT_CALL(mock_url_checker_, DoDoesUrlResolve(kUrl, _, _))
      .WillOnce(InvokeCallbackArgument<2>(false, expected_duration_timeout));

  base::MockCallback<base::OnceCallback<void(bool, bool)>> callback_timeout;
  EXPECT_CALL(callback_timeout, Run(false, true));
  instant_service_->DoesUrlResolve(kUrl, callback_timeout.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled, SetCustomBackgroundURL) {
  const GURL kUrl("https://www.foo.com");

  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURL(kUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
}

TEST_F(InstantServiceTest, SetCustomBackgroundURL) {
  const GURL kUrl("https://www.foo.com");

  instant_service_->UpdateThemeInfo();
  instant_service_->SetCustomBackgroundURL(kUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       SetCustomBackgroundURLInvalidURL) {
  const GURL kInvalidUrl("foo");
  const GURL kValidUrl("https://www.foo.com");
  instant_service_->AddValidBackdropUrlForTesting(kValidUrl);
  instant_service_->SetCustomBackgroundURL(kValidUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kValidUrl.spec(), theme_info->custom_background_url.spec());

  instant_service_->SetCustomBackgroundURL(kInvalidUrl);

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(std::string(), theme_info->custom_background_url.spec());
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       SetCustomBackgroundURLWithAttributions) {
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_EQ(kAttributionLine1,
            theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(kAttributionLine2,
            theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(kActionUrl, theme_info->custom_background_attribution_action_url);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       ChangingSearchProviderClearsThemeInfoAndPref) {
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_EQ(kAttributionLine1,
            theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(kAttributionLine2,
            theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(kActionUrl, theme_info->custom_background_attribution_action_url);

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  instant_service_->UpdateThemeInfo();

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->UpdateThemeInfo();

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_1);
  EXPECT_EQ(std::string(), theme_info->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       LocalBackgroundImageCopyCreated) {
  const GURL kUrl("chrome-search://local-ntp/background.jpg");

  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII("test_file"));
  base::FilePath copy_path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));

  base::WriteFile(path, "background_image", 16);

  instant_service_->SelectLocalBackgroundImage(path);

  thread_bundle()->RunUntilIdle();

  bool file_exists = base::PathExists(copy_path);

  EXPECT_EQ(true, file_exists);
  EXPECT_EQ(true, profile()->GetTestingPrefService()->GetBoolean(
                      prefs::kNtpCustomBackgroundLocalToDevice));
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       ChangingSearchProviderRemovesLocalBackgroundImageCopy) {
  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));

  base::WriteFile(path, "background_image", 16);

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  instant_service_->UpdateThemeInfo();

  thread_bundle()->RunUntilIdle();

  bool file_exists = base::PathExists(path);

  EXPECT_EQ(false, file_exists);
  EXPECT_EQ(false, profile()->GetTestingPrefService()->GetBoolean(
                       prefs::kNtpCustomBackgroundLocalToDevice));
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       SettingUrlRemovesLocalBackgroundImageCopy) {
  const GURL kUrl("https://www.foo.com");

  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));

  base::WriteFile(path, "background_image", 16);

  instant_service_->SetCustomBackgroundURL(kUrl);
  instant_service_->UpdateThemeInfo();

  thread_bundle()->RunUntilIdle();

  bool file_exists = base::PathExists(path);

  EXPECT_EQ(false, file_exists);
  EXPECT_EQ(false, profile()->GetTestingPrefService()->GetBoolean(
                       prefs::kNtpCustomBackgroundLocalToDevice));
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       CustomBackgroundAttributionActionUrlReset) {
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kHttpsActionUrl("https://www.bar.com");
  const GURL kHttpActionUrl("http://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpsActionUrl);

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kHttpsActionUrl,
            theme_info->custom_background_attribution_action_url);

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpActionUrl);

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpsActionUrl);

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kHttpsActionUrl,
            theme_info->custom_background_attribution_action_url);

  instant_service_->SetCustomBackgroundURLWithAttributions(
      kUrl, kAttributionLine1, kAttributionLine2, GURL());

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_attribution_action_url);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       UpdatingPrefUpdatesThemeInfo) {
  const GURL kUrlFoo("https://www.foo.com");
  const GURL kUrlBar("https://www.bar.com");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();
  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrlFoo)));

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrlFoo, theme_info->custom_background_url);

  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrlBar)));

  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrlBar, theme_info->custom_background_url);
  EXPECT_EQ(false,
            pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled, NoLocalFileExists) {
  const GURL kUrl("chrome-search://local-ntp/background.jpg?123456789");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrl)));
  thread_bundle()->RunUntilIdle();

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
  EXPECT_EQ(false,
            pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled, LocalFileExists) {
  const GURL kUrl("chrome-search://local-ntp/background.jpg?123456789");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));
  base::WriteFile(path, "background_image", 16);
  base::TaskScheduler::GetInstance()->FlushForTesting();

  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrl)));
  thread_bundle()->RunUntilIdle();

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
  EXPECT_EQ(true,
            pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled, LocalFilePrefSet) {
  const GURL kUrl("chrome-search://local-ntp/background.jpg?123456789");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  pref_service->SetUserPref(prefs::kNtpCustomBackgroundLocalToDevice,
                            std::make_unique<base::Value>(true));
  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrl)));
  thread_bundle()->RunUntilIdle();

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled,
       LocalFileCopiedToProfileDirectory) {
  const GURL kUrl("chrome-search://local-ntp/background.jpg?123456789");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath old_path(user_data_dir.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));
  base::FilePath new_path(profile()->GetPath().AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));
  base::WriteFile(old_path, "background_image", 16);
  base::TaskScheduler::GetInstance()->FlushForTesting();

  pref_service->SetUserPref(
      prefs::kNtpCustomBackgroundDict,
      std::make_unique<base::Value>(GetBackgroundInfoAsDict(kUrl)));
  thread_bundle()->RunUntilIdle();

  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kUrl, theme_info->custom_background_url);

  EXPECT_FALSE(base::PathExists(old_path));
  EXPECT_TRUE(base::PathExists(new_path));
}

TEST_F(InstantServiceTestCustomBackgroundsEnabled, ValidateBackdropUrls) {
  const GURL kBackdropUrl1("https://www.foo.com");
  const GURL kBackdropUrl2("https://www.bar.com");
  const GURL kNonBackdropUrl1("https://www.test.com");
  const GURL kNonBackdropUrl2("https://www.foo.com/path");

  instant_service_->AddValidBackdropUrlForTesting(kBackdropUrl1);
  instant_service_->AddValidBackdropUrlForTesting(kBackdropUrl2);

  instant_service_->SetCustomBackgroundURL(kBackdropUrl1);
  ThemeBackgroundInfo* theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kBackdropUrl1, theme_info->custom_background_url);

  instant_service_->SetCustomBackgroundURL(kNonBackdropUrl1);
  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);

  instant_service_->SetCustomBackgroundURL(kBackdropUrl2);
  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(kBackdropUrl2, theme_info->custom_background_url);

  instant_service_->SetCustomBackgroundURL(kNonBackdropUrl2);
  theme_info = instant_service_->GetThemeInfoForTesting();
  EXPECT_EQ(GURL(), theme_info->custom_background_url);
}
