// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/live_translate_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"

namespace captions {

class LiveTranslateControllerTest : public InProcessBrowserTest {
 public:
  LiveTranslateControllerTest() = default;
  LiveTranslateControllerTest(const LiveTranslateControllerTest&) = delete;
  LiveTranslateControllerTest& operator=(const LiveTranslateControllerTest&) =
      delete;
  ~LiveTranslateControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media::kLiveTranslate, media::kFeatureManagementLiveTranslateCrOS},
        {});
    InProcessBrowserTest::SetUp();
  }

  LiveTranslateController* GetControllerForProfile(Profile* profile) {
    return LiveTranslateControllerFactory::GetForProfile(profile);
  }

  void SetLiveCaptionEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                                 enabled);
  }

  bool GetLiveCaptionEnabled() {
    return browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kLiveCaptionEnabled);
  }

  void SetLiveTranslateEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                 enabled);
  }

  bool GetLiveTranslateEnabled() {
    return browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kLiveTranslateEnabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LiveTranslateControllerTest,
                       LiveTranslateEnableChanged) {
  EXPECT_FALSE(GetLiveCaptionEnabled());
  EXPECT_FALSE(GetLiveTranslateEnabled());

  SetLiveCaptionEnabled(true);
  EXPECT_TRUE(GetLiveCaptionEnabled());
  EXPECT_FALSE(GetLiveTranslateEnabled());

  SetLiveTranslateEnabled(true);
  EXPECT_TRUE(GetLiveCaptionEnabled());
  EXPECT_TRUE(GetLiveTranslateEnabled());

  // Turning off Live Caption should not modify live translate.
  SetLiveCaptionEnabled(false);
  EXPECT_FALSE(GetLiveCaptionEnabled());
  EXPECT_TRUE(GetLiveTranslateEnabled());

  SetLiveTranslateEnabled(false);
  // Turning on Live Translate should automatically turn on Live Caption.
  SetLiveTranslateEnabled(true);
  EXPECT_TRUE(GetLiveCaptionEnabled());
  EXPECT_TRUE(GetLiveTranslateEnabled());

  // Turning off Live Translate should not turn off Live Caption.
  SetLiveTranslateEnabled(false);
  EXPECT_TRUE(GetLiveCaptionEnabled());
  EXPECT_FALSE(GetLiveTranslateEnabled());
}

}  // namespace captions
