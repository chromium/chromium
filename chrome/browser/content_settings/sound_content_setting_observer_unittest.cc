// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/sound_content_setting_observer.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/tab_utils.h"
#endif

namespace {

constexpr char kURL1[] = "http://google.com/";
constexpr char kURL2[] = "http://youtube.com/";
constexpr char kSiteMutedEvent[] = "Media.SiteMuted";
constexpr char kSiteMutedReason[] = "MuteReason";
#if !BUILDFLAG(IS_ANDROID)
constexpr char kChromeURL[] = "chrome://dino";
constexpr char kExtensionId[] = "extensionid";
#endif

}  // anonymous namespace

class SoundContentSettingObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  SoundContentSettingObserverTest() {
    scoped_feature_list_.InitWithFeatures({media::kEnableTabMuting}, {});
  }

  SoundContentSettingObserverTest(const SoundContentSettingObserverTest&) =
      delete;
  SoundContentSettingObserverTest& operator=(
      const SoundContentSettingObserverTest&) = delete;

  ~SoundContentSettingObserverTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    RecentlyAudibleHelper::CreateForWebContents(web_contents());
    SoundContentSettingObserver::CreateForWebContents(web_contents());
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
    host_content_settings_map_ = HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    NavigateAndCommit(GURL(kURL1));
  }

 protected:
  void ChangeSoundContentSettingTo(ContentSetting setting) {
    GURL url = web_contents()->GetLastCommittedURL();
    host_content_settings_map_->SetContentSettingDefaultScope(
        url, url, ContentSettingsType::SOUND, setting);
  }

  void ChangeDefaultSoundContentSettingTo(ContentSetting setting) {
    host_content_settings_map_->SetDefaultContentSetting(
        ContentSettingsType::SOUND, setting);
  }

  void SimulateAudioStarting() {
    SoundContentSettingObserver::FromWebContents(web_contents())
        ->OnAudioStateChanged(true);
  }

  void SimulateAudioPlaying() {
    content::WebContentsTester::For(web_contents())
        ->SetIsCurrentlyAudible(true);
  }

  bool RecordedSiteMuted() {
    auto entries = test_ukm_recorder_->GetEntriesByName(kSiteMutedEvent);
    return !entries.empty();
  }

  void ExpectRecordedForReason(SoundContentSettingObserver::MuteReason reason) {
    auto entries = test_ukm_recorder_->GetEntriesByName(kSiteMutedEvent);
    EXPECT_EQ(1u, entries.size());
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kURL1));
      test_ukm_recorder_->ExpectEntryMetric(entry, kSiteMutedReason, reason);
    }
  }

// TabMutedReason does not exist on Android.
#if !BUILDFLAG(IS_ANDROID)
  void SetMuteStateForReason(bool state, TabMutedReason reason) {
    SetTabAudioMuted(web_contents(), state, reason, kExtensionId);
  }
#endif

 private:
  raw_ptr<HostContentSettingsMap, DanglingUntriaged> host_content_settings_map_;
  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SoundContentSettingObserverTest, AudioMutingUpdatesWithContentSetting) {
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Block site.
  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Allow site.
  ChangeSoundContentSettingTo(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Set site to default.
  ChangeSoundContentSettingTo(CONTENT_SETTING_DEFAULT);
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Block by default.
  ChangeDefaultSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Should not affect site if explicitly allowed.
  ChangeSoundContentSettingTo(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Change site back to default.
  ChangeSoundContentSettingTo(CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Allow by default.
  ChangeDefaultSoundContentSettingTo(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(web_contents()->IsAudioMuted());
}

TEST_F(SoundContentSettingObserverTest, AudioMutingUpdatesWithNavigation) {
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Block for kURL1.
  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Should not be muted for kURL2.
  NavigateAndCommit(GURL(kURL2));
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Should be muted for kURL1.
  NavigateAndCommit(GURL(kURL1));
  EXPECT_TRUE(web_contents()->IsAudioMuted());
}

// TabMutedReason does not exist on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(SoundContentSettingObserverTest, DontMuteWhenUnmutedByExtension) {
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Mute kURL1 via content setting.
  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Unmute by extension.
  SetMuteStateForReason(false, TabMutedReason::EXTENSION);
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Navigating to a new URL and back to kURL1 should not mute the tab unmuted
  // by an extension.
  NavigateAndCommit(GURL(kURL2));
  EXPECT_FALSE(web_contents()->IsAudioMuted());
  NavigateAndCommit(GURL(kURL1));
  EXPECT_FALSE(web_contents()->IsAudioMuted());
}

TEST_F(SoundContentSettingObserverTest, DontUnmuteWhenMutedByExtension) {
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  SetMuteStateForReason(true, TabMutedReason::EXTENSION);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Navigating to a new URL should not unmute the tab muted by an extension.
  NavigateAndCommit(GURL(kURL2));
  EXPECT_TRUE(web_contents()->IsAudioMuted());
}

TEST_F(SoundContentSettingObserverTest, DontUnmuteWhenMutedByAudioIndicator) {
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  SetMuteStateForReason(true, TabMutedReason::AUDIO_INDICATOR);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Navigating to a new URL should not unmute the tab muted by audio indicator.
  NavigateAndCommit(GURL(kURL2));
  EXPECT_TRUE(web_contents()->IsAudioMuted());
}

TEST_F(SoundContentSettingObserverTest, DontUnmuteChromeTabWhenMuted) {
  NavigateAndCommit(GURL(kChromeURL));
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  SetMuteStateForReason(true, TabMutedReason::CONTENT_SETTING_CHROME);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  NavigateAndCommit(GURL(kChromeURL));
  EXPECT_TRUE(web_contents()->IsAudioMuted());
}

TEST_F(SoundContentSettingObserverTest,
       UnmuteChromeTabWhenNavigatingToNonChromeUrl) {
  NavigateAndCommit(GURL(kChromeURL));
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  SetMuteStateForReason(true, TabMutedReason::CONTENT_SETTING_CHROME);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  NavigateAndCommit(GURL(kURL1));
  EXPECT_FALSE(web_contents()->IsAudioMuted());
}

TEST_F(SoundContentSettingObserverTest,
       UnmuteNonChromeTabWhenNavigatingToChromeUrl) {
  NavigateAndCommit(GURL(kURL1));
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  NavigateAndCommit(GURL(kChromeURL));
  EXPECT_FALSE(web_contents()->IsAudioMuted());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(SoundContentSettingObserverTest,
       UnmutedAudioPlayingDoesNotRecordSiteMuted) {
  // Play audio while sound content setting is allowed.
  ChangeSoundContentSettingTo(CONTENT_SETTING_ALLOW);
  SimulateAudioStarting();
  EXPECT_FALSE(RecordedSiteMuted());
}

TEST_F(SoundContentSettingObserverTest, MutedAudioBlockedBySiteException) {
  // Play audio while sound content setting is blocked.
  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  SimulateAudioStarting();
  EXPECT_TRUE(RecordedSiteMuted());
  ExpectRecordedForReason(
      SoundContentSettingObserver::MuteReason::kSiteException);
}

TEST_F(SoundContentSettingObserverTest,
       MutingAudioWhileSoundIsPlayingBlocksSound) {
  // Unmuted audio starts playing.
  SimulateAudioPlaying();
  // Sound is not blocked.
  EXPECT_FALSE(RecordedSiteMuted());
  // User mutes the site.
  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  // Sound is blocked.
  EXPECT_TRUE(RecordedSiteMuted());
  ExpectRecordedForReason(
      SoundContentSettingObserver::MuteReason::kSiteException);
}

TEST_F(SoundContentSettingObserverTest, MuteByDefaultRecordsCorrectly) {
  // Blocking audio via default content setting records properly.
  ChangeDefaultSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  SimulateAudioStarting();
  EXPECT_TRUE(RecordedSiteMuted());
  ExpectRecordedForReason(
      SoundContentSettingObserver::MuteReason::kMuteByDefault);
}

TEST_F(SoundContentSettingObserverTest,
       MuteByDefaultAndExceptionRecordsException) {
  // Block audio via default and exception.
  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  ChangeDefaultSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  SimulateAudioStarting();
  EXPECT_TRUE(RecordedSiteMuted());
  ExpectRecordedForReason(
      SoundContentSettingObserver::MuteReason::kSiteException);
}
