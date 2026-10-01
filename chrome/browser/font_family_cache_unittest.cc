// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/font_family_cache.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

// Font family preferences are not registered on non-desktop Android (see
// `PrefsTabHelper::RegisterProfilePrefs`), and `FontFamilyCache` is not used
// there (see `ChromeContentBrowserClient::OverrideWebPreferences`).
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

namespace {

class TestingFontFamilyCache : public FontFamilyCache {
 public:
  explicit TestingFontFamilyCache(Profile* profile)
      : FontFamilyCache(profile), fetch_font_count_(0) {}

  TestingFontFamilyCache(const TestingFontFamilyCache&) = delete;
  TestingFontFamilyCache& operator=(const TestingFontFamilyCache&) = delete;

  ~TestingFontFamilyCache() override = default;
  std::u16string FetchFont(const char* script, const char* map_name) override {
    ++fetch_font_count_;
    return FontFamilyCache::FetchFont(script, map_name);
  }

  int fetch_font_count_;
};

}  // namespace

// Tests that the cache is correctly set and cleared.
TEST(FontFamilyCacheTest, Caching) {
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  TestingFontFamilyCache cache(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  std::string font1("font 1");
  std::string font2("font 2");
  const char* map_name = prefs::kWebKitSansSerifFontFamilyMap;
  std::string script("Latn");
  std::string pref_name(std::string(map_name) + '.' + script);
  std::string pref_name2(std::string(map_name) + '.' + "adsf");

  // Sets the first preference and registers the second one.
  prefs->SetString(pref_name.c_str(), font1.c_str());
  prefs->registry()->RegisterStringPref(pref_name2.c_str(), std::string());

  // Check that the right preference is returned.
  blink::web_pref::ScriptFontFamilyMap map;
  cache.FillFontFamilyMap(map_name, &map);
  EXPECT_EQ(font1, base::UTF16ToUTF8(map[script]));
  const int expected_initial_fetches =
      static_cast<int>(prefs::kWebKitScriptsForFontFamilyMapsLength);
  EXPECT_EQ(expected_initial_fetches, cache.fetch_font_count_);

  // Check that the second access uses the cache.
  map.clear();
  cache.FillFontFamilyMap(map_name, &map);
  EXPECT_EQ(font1, base::UTF16ToUTF8(map[script]));
  EXPECT_EQ(expected_initial_fetches, cache.fetch_font_count_);

  // Changing another preference should have no effect.
  prefs->SetString(pref_name2.c_str(), "katy perry");
  map.clear();
  cache.FillFontFamilyMap(map_name, &map);
  EXPECT_EQ(font1, base::UTF16ToUTF8(map[script]));
  EXPECT_EQ(expected_initial_fetches, cache.fetch_font_count_);

  // Changing the preference updates the cache.
  prefs->SetString(pref_name.c_str(), font2.c_str());
  map.clear();
  cache.FillFontFamilyMap(map_name, &map);
  EXPECT_EQ(font2, base::UTF16ToUTF8(map[script]));
  EXPECT_EQ(expected_initial_fetches + 1, cache.fetch_font_count_);
}

#endif  // !BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
