// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/font_family_cache.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestingFontFamilyCache : public FontFamilyCache {
 public:
  explicit TestingFontFamilyCache(Profile* profile)
      : FontFamilyCache(profile), fetch_font_count_(0) {}

  TestingFontFamilyCache(const TestingFontFamilyCache&) = delete;
  TestingFontFamilyCache& operator=(const TestingFontFamilyCache&) = delete;

  ~TestingFontFamilyCache() override {}
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
  std::string map_name("webkit.webprefs.fonts.sansserif");
  std::string script("Zzyxca");
  std::string pref_name(map_name + '.' + script);
  std::string pref_name2(map_name + '.' + "adsf");

  // Registers 2 preferences, and sets the first one.
  prefs->registry()->RegisterStringPref(pref_name.c_str(), std::string());
  prefs->registry()->RegisterStringPref(pref_name2.c_str(), std::string());
  prefs->SetString(pref_name.c_str(), font1.c_str());

  // Check that the right preference is returned.
  std::string result = base::UTF16ToUTF8(
      cache.FetchAndCacheFont(script.c_str(), map_name.c_str()));
  EXPECT_EQ(font1, result);
  EXPECT_EQ(1, cache.fetch_font_count_);

  // Check that the second access uses the cache.
  result = base::UTF16ToUTF8(
      cache.FetchAndCacheFont(script.c_str(), map_name.c_str()));
  EXPECT_EQ(font1, result);
  EXPECT_EQ(1, cache.fetch_font_count_);

  // Changing another preference should have no effect.
  prefs->SetString(pref_name2.c_str(), "katy perry");
  result = base::UTF16ToUTF8(
      cache.FetchAndCacheFont(script.c_str(), map_name.c_str()));
  EXPECT_EQ(font1, result);
  EXPECT_EQ(1, cache.fetch_font_count_);

  // Changing the preferences invalidates the cache.
  prefs->SetString(pref_name.c_str(), font2.c_str());
  result = base::UTF16ToUTF8(
      cache.FetchAndCacheFont(script.c_str(), map_name.c_str()));
  EXPECT_EQ(font2, result);
  EXPECT_EQ(2, cache.fetch_font_count_);
}
