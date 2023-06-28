// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

using blink::web_pref::WebPreferences;
using content::RenderViewHostTester;

TEST(ChromePrefServiceTest, UpdateCommandLinePrefStore) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kDisable3DAPIs, false);

  // Check to make sure the value is as expected.
  const PrefService::Preference* pref =
      prefs.FindPreference(prefs::kDisable3DAPIs);
  ASSERT_TRUE(pref);
  const base::Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::BOOLEAN, value->type());
  EXPECT_TRUE(value->is_bool());
  EXPECT_FALSE(value->GetBool());

  // Change the command line.
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(switches::kDisable3DAPIs);

  // Call UpdateCommandLinePrefStore and check to see if the value has changed.
  prefs.UpdateCommandLinePrefStore(new ChromeCommandLinePrefStore(&cmd_line));
  pref = prefs.FindPreference(prefs::kDisable3DAPIs);
  ASSERT_TRUE(pref);
  value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::BOOLEAN, value->type());
  EXPECT_TRUE(value->is_bool());
  EXPECT_TRUE(value->GetBool());
}

class ChromePrefServiceWebKitPrefs : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Supply our own profile so we use the correct profile data. The test
    // harness is not supposed to overwrite a profile if it's already created.

    // Set some (WebKit) user preferences.
    sync_preferences::TestingPrefServiceSyncable* pref_services =
        profile()->GetTestingPrefService();
    pref_services->SetUserPref(prefs::kDefaultCharset,
                               std::make_unique<base::Value>("utf8"));
    pref_services->SetUserPref(prefs::kWebKitDefaultFontSize,
                               std::make_unique<base::Value>(20));
    pref_services->SetUserPref(prefs::kWebKitTextAreasAreResizable,
                               std::make_unique<base::Value>(false));
    pref_services->SetUserPref("webkit.webprefs.foo",
                               std::make_unique<base::Value>("bar"));
  }
};

// Tests to see that webkit preferences are properly loaded and copied over
// to a WebPreferences object.
TEST_F(ChromePrefServiceWebKitPrefs, PrefsCopied) {
  WebPreferences webkit_prefs =
      RenderViewHostTester::For(rvh())->TestComputeWebPreferences();

  // These values have been overridden by the profile preferences.
  EXPECT_EQ("UTF-8", webkit_prefs.default_encoding);
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(20, webkit_prefs.default_font_size);
#else
  // This pref is not configurable on Android so the default of 16 is always
  // used.
  EXPECT_EQ(16, webkit_prefs.default_font_size);
#endif
  EXPECT_FALSE(webkit_prefs.text_areas_are_resizable);

  // These should still be the default values.
#if BUILDFLAG(IS_MAC)
  const char16_t kDefaultFont[] = u"Times";
#elif BUILDFLAG(IS_CHROMEOS)
  const char16_t kDefaultFont[] = u"Tinos";
#else
  const char16_t kDefaultFont[] = u"Times New Roman";
#endif
  EXPECT_EQ(kDefaultFont,
            webkit_prefs.standard_font_family_map[prefs::kWebKitCommonScript]);
  EXPECT_TRUE(webkit_prefs.javascript_enabled);

#if BUILDFLAG(IS_ANDROID)
  // Touch event enabled only on Android.
  EXPECT_TRUE(webkit_prefs.touch_event_feature_detection_enabled);
#else
  EXPECT_FALSE(webkit_prefs.touch_event_feature_detection_enabled);
#endif
}
