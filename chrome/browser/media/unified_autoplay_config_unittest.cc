// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/unified_autoplay_config.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"

// Unit tests for the unified autoplay policy with the unified sound settings
// UI enabled.

class UnifiedAutoplaySoundSettingsTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ~UnifiedAutoplaySoundSettingsTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media::kAutoplayDisableSettings, media::kAutoplayWhitelistSettings},
        {});
    ChromeRenderViewHostTestHarness::SetUp();
    test_service_manager_context_ =
        std::make_unique<content::TestServiceManagerContext>();

    SoundContentSettingObserver::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    // Must be reset before browser thread teardown.
    test_service_manager_context_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetSoundContentSettingDefault(ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile());
    content_settings->SetDefaultContentSetting(ContentSettingsType::SOUND,
                                               value);
  }

  void SetAutoplayPrefValue(bool value) {
    GetPrefs()->SetBoolean(prefs::kBlockAutoplayEnabled, value);
    EXPECT_EQ(value, GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled));
  }

  bool ShouldBlockAutoplay() {
    return UnifiedAutoplayConfig::ShouldBlockAutoplay(profile());
  }

  content::AutoplayPolicy GetAppliedAutoplayPolicy() {
    return web_contents()
        ->GetRenderViewHost()
        ->GetWebkitPreferences()
        .autoplay_policy;
  }

  void NavigateToTestPage() {
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("https://first.example.com"));
  }

 private:
  PrefService* GetPrefs() { return profile()->GetPrefs(); }

  base::test::ScopedFeatureList scoped_feature_list_;

  // WebContentsImpl accesses the system Connector, so the Service Manager must
  // be initialized.
  std::unique_ptr<content::TestServiceManagerContext>
      test_service_manager_context_;
};

TEST_F(UnifiedAutoplaySoundSettingsTest, ContentSetting_Allow) {
  SetSoundContentSettingDefault(CONTENT_SETTING_ALLOW);
  SetAutoplayPrefValue(false);

  EXPECT_FALSE(ShouldBlockAutoplay());

  NavigateToTestPage();
  EXPECT_EQ(content::AutoplayPolicy::kNoUserGestureRequired,
            GetAppliedAutoplayPolicy());
}

TEST_F(UnifiedAutoplaySoundSettingsTest, ContentSetting_Block) {
  SetSoundContentSettingDefault(CONTENT_SETTING_BLOCK);

  SetAutoplayPrefValue(false);
  EXPECT_TRUE(ShouldBlockAutoplay());

  NavigateToTestPage();
  EXPECT_EQ(content::AutoplayPolicy::kDocumentUserActivationRequired,
            GetAppliedAutoplayPolicy());

  // Set back to ALLOW to ensure that the policy is updated on the next
  // navigation.
  SetSoundContentSettingDefault(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldBlockAutoplay());

  NavigateToTestPage();
  EXPECT_EQ(content::AutoplayPolicy::kNoUserGestureRequired,
            GetAppliedAutoplayPolicy());
}

TEST_F(UnifiedAutoplaySoundSettingsTest, Feature_DisabledNoop) {
  // Explicitly disable the feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {media::kAutoplayDisableSettings, media::kAutoplayWhitelistSettings});

  SetAutoplayPrefValue(false);
  EXPECT_FALSE(ShouldBlockAutoplay());

  NavigateToTestPage();
  EXPECT_EQ(content::AutoplayPolicy::kDocumentUserActivationRequired,
            GetAppliedAutoplayPolicy());
}

TEST_F(UnifiedAutoplaySoundSettingsTest, Pref_DefaultEnabled) {
  EXPECT_TRUE(ShouldBlockAutoplay());

  NavigateToTestPage();
  EXPECT_EQ(content::AutoplayPolicy::kDocumentUserActivationRequired,
            GetAppliedAutoplayPolicy());
}

TEST_F(UnifiedAutoplaySoundSettingsTest, Pref_Disabled) {
  SetAutoplayPrefValue(false);
  EXPECT_FALSE(ShouldBlockAutoplay());

  NavigateToTestPage();
  EXPECT_EQ(content::AutoplayPolicy::kNoUserGestureRequired,
            GetAppliedAutoplayPolicy());

  // Now update the pref and make sure we apply it on the next navigation.
  SetAutoplayPrefValue(true);
  EXPECT_TRUE(ShouldBlockAutoplay());

  NavigateToTestPage();
  EXPECT_EQ(content::AutoplayPolicy::kDocumentUserActivationRequired,
            GetAppliedAutoplayPolicy());
}

// Unit tests for the unified autoplay policy with the unified sound settings
// UI enabled and a custom autoplay policy command line switch.

class UnifiedAutoplaySoundSettingsOverrideTest
    : public UnifiedAutoplaySoundSettingsTest {
 public:
  ~UnifiedAutoplaySoundSettingsOverrideTest() override = default;

  void SetUp() override {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kUserGestureRequiredPolicy);

    UnifiedAutoplaySoundSettingsTest::SetUp();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(UnifiedAutoplaySoundSettingsOverrideTest, CommandLineOverride) {
  EXPECT_TRUE(ShouldBlockAutoplay());

  NavigateToTestPage();
  EXPECT_EQ(content::AutoplayPolicy::kUserGestureRequired,
            GetAppliedAutoplayPolicy());
}
