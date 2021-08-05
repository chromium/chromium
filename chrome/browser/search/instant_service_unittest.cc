// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/section_type.h"
#include "components/search/ntp_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/test_native_theme.h"
#include "url/gurl.h"

namespace {

class MockInstantServiceObserver : public InstantServiceObserver {
 public:
  MOCK_METHOD1(NtpThemeChanged, void(const NtpTheme&));
  MOCK_METHOD1(MostVisitedInfoChanged, void(const InstantMostVisitedInfo&));
};

class MockInstantService : public InstantService {
 public:
  explicit MockInstantService(Profile* profile) : InstantService(profile) {}
  ~MockInstantService() override = default;

  MOCK_METHOD0(ResetCustomBackgroundNtpTheme, void());
};
}  // namespace

using InstantServiceTest = InstantUnitTestBase;

TEST_F(InstantServiceTest, GetNTPTileSuggestion) {
  ntp_tiles::NTPTile some_tile;
  some_tile.source = ntp_tiles::TileSource::TOP_SITES;
  some_tile.title_source = ntp_tiles::TileTitleSource::TITLE_TAG;
  ntp_tiles::NTPTilesVector suggestions{some_tile};

  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> suggestions_map;
  suggestions_map[ntp_tiles::SectionType::PERSONALIZED] = suggestions;

  instant_service_->OnURLsAvailable(suggestions_map);

  auto items = instant_service_->most_visited_info_->items;
  ASSERT_EQ(1, (int)items.size());
  EXPECT_EQ(ntp_tiles::TileSource::TOP_SITES, items[0].source);
  EXPECT_EQ(ntp_tiles::TileTitleSource::TITLE_TAG, items[0].title_source);
}

TEST_F(InstantServiceTest, ChangingSearchProviderClearsNtpThemeAndPref) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundInfo(kUrl, kAttributionLine1,
                                            kAttributionLine2, kActionUrl, "");

  NtpTheme* theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_EQ(kUrl, theme->custom_background_url);
  EXPECT_EQ(kAttributionLine1, theme->custom_background_attribution_line_1);
  EXPECT_EQ(kAttributionLine2, theme->custom_background_attribution_line_2);
  EXPECT_EQ(kActionUrl, theme->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  instant_service_->UpdateNtpTheme();

  theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_EQ(GURL(), theme->custom_background_url);
  EXPECT_EQ("", theme->custom_background_attribution_line_1);
  EXPECT_EQ("", theme->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme->custom_background_attribution_action_url);
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->UpdateNtpTheme();

  theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_EQ(GURL(), theme->custom_background_url);
  EXPECT_EQ("", theme->custom_background_attribution_line_1);
  EXPECT_EQ("", theme->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(), theme->custom_background_attribution_action_url);
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest,
       ChangingSearchProviderRemovesLocalBackgroundImageCopy) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  base::FilePath profile_path = profile()->GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));

  base::WriteFile(path, "background_image", 16);

  SetUserSelectedDefaultSearchProvider("https://www.search.com");
  instant_service_->UpdateNtpTheme();

  task_environment()->RunUntilIdle();

  bool file_exists = base::PathExists(path);

  EXPECT_EQ(false, file_exists);
  EXPECT_EQ(false, profile()->GetTestingPrefService()->GetBoolean(
                       prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_FALSE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, CustomBackgroundAttributionActionUrlReset) {
  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kHttpsActionUrl("https://www.bar.com");
  const GURL kHttpActionUrl("http://www.bar.com");

  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundInfo(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpsActionUrl, "");

  NtpTheme* theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_EQ(kHttpsActionUrl, theme->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundInfo(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpActionUrl, "");

  theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_EQ(GURL(), theme->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundInfo(
      kUrl, kAttributionLine1, kAttributionLine2, kHttpsActionUrl, "");

  theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_EQ(kHttpsActionUrl, theme->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());

  instant_service_->SetCustomBackgroundInfo(kUrl, kAttributionLine1,
                                            kAttributionLine2, GURL(), "");

  theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_EQ(GURL(), theme->custom_background_attribution_action_url);
  EXPECT_TRUE(instant_service_->IsCustomBackgroundSet());
}

TEST_F(InstantServiceTest, TestNoNtpTheme) {
  instant_service_->theme_ = nullptr;
  EXPECT_NE(nullptr, instant_service_->GetInitializedNtpTheme());

  instant_service_->theme_ = nullptr;
  // As |FallbackToDefaultNtpTheme| uses |theme_| it should initialize it
  // otherwise the test should crash.
  instant_service_->FallbackToDefaultNtpTheme();
  EXPECT_NE(nullptr, instant_service_->theme_);
}

class InstantServiceThemeTest : public InstantServiceTest {
 public:
  InstantServiceThemeTest() {}
  ~InstantServiceThemeTest() override {}

  ui::TestNativeTheme* theme() { return &theme_; }

 private:
  ui::TestNativeTheme theme_;

  DISALLOW_COPY_AND_ASSIGN(InstantServiceThemeTest);
};

TEST_F(InstantServiceTest, SetNTPElementsNtpTheme) {
  const auto& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile());
  SkColor default_text_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT);

  ASSERT_FALSE(instant_service_->IsCustomBackgroundSet());

  // Check defaults when no theme and no custom backgrounds is set.
  NtpTheme* theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_EQ(default_text_color, theme->text_color);
  EXPECT_FALSE(theme->logo_alternate);

  // Install colors, theme update should trigger SetNTPElementsNtpTheme() and
  // update NTP themed elements info.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile());
  test::ThemeServiceChangedWaiter waiter(theme_service);
  theme_service->BuildAutogeneratedThemeFromColor(SK_ColorRED);
  waiter.WaitForThemeChanged();

  theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_NE(default_text_color, theme->text_color);
  EXPECT_TRUE(theme->logo_alternate);

  // Setting a custom background should call SetNTPElementsNtpTheme() and
  // update NTP themed elements info.
  const GURL kUrl("https://www.foo.com");
  instant_service_->AddValidBackdropUrlForTesting(kUrl);
  instant_service_->SetCustomBackgroundInfo(kUrl, "", "", GURL(), "");
  ASSERT_TRUE(instant_service_->IsCustomBackgroundSet());

  theme = instant_service_->GetInitializedNtpTheme();
  EXPECT_NE(default_text_color, theme->text_color);
  EXPECT_TRUE(theme->logo_alternate);
}
