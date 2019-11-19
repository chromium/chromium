// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"

namespace {

bool FilterGoogle(const GURL& url) {
  return url == "https://www.google.com/";
}

bool FilterAll(const GURL& url) {
  return true;
}

}  // namespace

class PermissionDecisionAutoBlockerUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    autoblocker_ = PermissionDecisionAutoBlocker::GetForProfile(profile());
    feature_list_.InitWithFeatures({features::kBlockPromptsIfDismissedOften,
                                    features::kBlockPromptsIfIgnoredOften},
                                   {});
    last_embargoed_status_ = false;
    autoblocker_->SetClockForTesting(&clock_);
    callback_was_run_ = false;
  }

  PermissionDecisionAutoBlocker* autoblocker() { return autoblocker_; }

  void SetLastEmbargoStatus(base::Closure quit_closure, bool status) {
    callback_was_run_ = true;
    last_embargoed_status_ = status;
    if (quit_closure) {
      quit_closure.Run();
      quit_closure.Reset();
    }
  }

  bool last_embargoed_status() { return last_embargoed_status_; }

  bool callback_was_run() { return callback_was_run_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  PermissionDecisionAutoBlocker* autoblocker_;
  base::test::ScopedFeatureList feature_list_;
  base::SimpleTestClock clock_;
  bool last_embargoed_status_;
  bool callback_was_run_;
};

// Check removing the the embargo for a single permission on a site works, and
// that it doesn't interfere with other embargoed permissions or the same
// permission embargoed on other sites.
TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveEmbargoByUrl) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  // Record dismissals for location and notifications in |url1|.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS));
  // Record dismissals for location in |url2|.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION));

  // Verify all dismissals recorded above resulted in embargo.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url1, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  result =
      autoblocker()->GetEmbargoResult(url1, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  result =
      autoblocker()->GetEmbargoResult(url2, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Remove the embargo on notifications. Verify it is no longer under embargo,
  // but location still is.
  autoblocker()->RemoveEmbargoByUrl(url1, ContentSettingsType::NOTIFICATIONS);
  result =
      autoblocker()->GetEmbargoResult(url1, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  result =
      autoblocker()->GetEmbargoResult(url1, ContentSettingsType::NOTIFICATIONS);
  // If not under embargo, GetEmbargoResult() returns a setting of ASK.
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  // Verify |url2|'s embargo is still intact as well.
  result =
      autoblocker()->GetEmbargoResult(url2, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
}

// Test it still only takes one more dismissal to re-trigger embargo after
// removing the embargo status for a site.
TEST_F(PermissionDecisionAutoBlockerUnitTest,
       DismissAfterRemovingEmbargoByURL) {
  GURL url("https://www.example.com");

  // Record dismissals for location.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));

  // Verify location is under embargo.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Remove embargo and verify this is true.
  autoblocker()->RemoveEmbargoByUrl(url, ContentSettingsType::GEOLOCATION);
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismissal and verify location is under embargo again.
  autoblocker()->RecordDismissAndEmbargo(url, ContentSettingsType::GEOLOCATION);
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
}

TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveCountsByUrl) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  // Record some dismissals.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));

  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(3, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url2, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::NOTIFICATIONS));

  // Record some ignores.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::MIDI_SYSEX));
  EXPECT_EQ(
      1, autoblocker()->GetIgnoreCount(url1, ContentSettingsType::MIDI_SYSEX));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::DURABLE_STORAGE));
  EXPECT_EQ(1, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::DURABLE_STORAGE));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      2, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::GEOLOCATION));

  autoblocker()->RemoveCountsByUrl(base::Bind(&FilterGoogle));

  // Expect that url1's actions are gone, but url2's remain.
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url1, ContentSettingsType::MIDI_SYSEX));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::DURABLE_STORAGE));

  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url2, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      2, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::GEOLOCATION));

  // Add some more actions.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::NOTIFICATIONS));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   url2, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      1, autoblocker()->GetIgnoreCount(url1, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::DURABLE_STORAGE));
  EXPECT_EQ(1, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::DURABLE_STORAGE));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url2, ContentSettingsType::MIDI_SYSEX));
  EXPECT_EQ(
      1, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::MIDI_SYSEX));

  // Remove everything and expect that it's all gone.
  autoblocker()->RemoveCountsByUrl(base::Bind(&FilterAll));

  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url2, ContentSettingsType::GEOLOCATION));

  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url2, ContentSettingsType::DURABLE_STORAGE));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::MIDI_SYSEX));
}

// Check that we do not apply embargo to the plugins content type, as prompts
// should be triggered only when necessary by Html5ByDefault.
TEST_F(PermissionDecisionAutoBlockerUnitTest,
       PluginsNotEmbargoedByMultipleDismissesOrIgnores) {
  GURL url("https://www.google.com");

  // Check dismisses first.
  autoblocker()->RecordDismissAndEmbargo(url, ContentSettingsType::PLUGINS);
  autoblocker()->RecordDismissAndEmbargo(url, ContentSettingsType::PLUGINS);
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(2,
            autoblocker()->GetDismissCount(url, ContentSettingsType::PLUGINS));

  // The third dismiss would normally embargo, but this shouldn't happen for
  // plugins.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::PLUGINS));
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(3,
            autoblocker()->GetDismissCount(url, ContentSettingsType::PLUGINS));

  // Extra one for sanity checking.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::PLUGINS));
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(4,
            autoblocker()->GetDismissCount(url, ContentSettingsType::PLUGINS));

  // Check ignores.
  autoblocker()->RecordIgnoreAndEmbargo(url, ContentSettingsType::PLUGINS);
  autoblocker()->RecordIgnoreAndEmbargo(url, ContentSettingsType::PLUGINS);
  autoblocker()->RecordIgnoreAndEmbargo(url, ContentSettingsType::PLUGINS);
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(3,
            autoblocker()->GetIgnoreCount(url, ContentSettingsType::PLUGINS));

  // The fourth ignore would normally embargo, but this shouldn't happen for
  // plugins.
  EXPECT_FALSE(
      autoblocker()->RecordIgnoreAndEmbargo(url, ContentSettingsType::PLUGINS));
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(4,
            autoblocker()->GetIgnoreCount(url, ContentSettingsType::PLUGINS));

  // Extra one for sanity checking.
  EXPECT_FALSE(
      autoblocker()->RecordIgnoreAndEmbargo(url, ContentSettingsType::PLUGINS));
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(5,
            autoblocker()->GetIgnoreCount(url, ContentSettingsType::PLUGINS));
}

// Check that GetEmbargoResult returns the correct value when the embargo is set
// and expires.
TEST_F(PermissionDecisionAutoBlockerUnitTest, CheckEmbargoStatus) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  // Check the default state.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Place under embargo and verify.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Check that the origin is not under embargo for a different permission.
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Confirm embargo status during the embargo period.
  clock()->Advance(base::TimeDelta::FromDays(5));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Check embargo is lifted on expiry day. A small offset after the exact
  // embargo expiration date has been added to account for any precision errors
  // when removing the date stored as a double from the permission dictionary.
  clock()->Advance(base::TimeDelta::FromHours(3 * 24 + 1));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Check embargo is lifted well after the expiry day.
  clock()->Advance(base::TimeDelta::FromDays(1));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Place under embargo again and verify the embargo status.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS));
  clock()->Advance(base::TimeDelta::FromDays(1));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
}

// Tests the alternating pattern of the block on multiple dismiss behaviour. On
// N dismissals, the origin to be embargoed for the requested permission and
// automatically blocked. Each time the embargo is lifted, the site gets another
// chance to request the permission, but if it is again dismissed it is placed
// under embargo again and its permission requests blocked.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestDismissEmbargoBackoff) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());
  base::HistogramTester histograms;

  // Record some dismisses.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));

  // A request with < 3 prior dismisses should not be automatically blocked.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // After the 3rd dismiss subsequent permission requests should be autoblocked.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Accelerate time forward, check that the embargo status is lifted and the
  // request won't be automatically blocked.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Accelerate time again, check embargo is lifted and another permission
  // request is let through.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
}

// Tests the alternating pattern of the block on multiple ignores behaviour.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestIgnoreEmbargoBackoff) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());
  base::HistogramTester histograms;

  // Record some ignores.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX));

  // A request with < 4 prior ignores should not be automatically blocked.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // After the 4th ignore subsequent permission requests should be autoblocked.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX));
  EXPECT_TRUE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_IGNORES, result.source);

  // Accelerate time forward, check that the embargo status is lifted and the
  // request won't be automatically blocked.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_IGNORES, result.source);

  // Accelerate time again, check embargo is lifted and another permission
  // request is let through.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_IGNORES, result.source);
}
