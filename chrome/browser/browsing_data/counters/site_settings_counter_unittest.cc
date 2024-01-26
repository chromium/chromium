// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/site_settings_counter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#endif

using custom_handlers::ProtocolHandler;

namespace {

class SiteSettingsCounterTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    map_ = HostContentSettingsMapFactory::GetForProfile(profile());
#if !BUILDFLAG(IS_ANDROID)
    zoom_map_ = content::HostZoomMap::GetDefaultForBrowserContext(profile());
#else
    zoom_map_ = nullptr;
#endif
    handler_registry_ =
        std::make_unique<custom_handlers::ProtocolHandlerRegistry>(
            profile()->GetPrefs(),
            std::make_unique<
                custom_handlers::TestProtocolHandlerRegistryDelegate>());

    counter_ = std::make_unique<SiteSettingsCounter>(
        map(), zoom_map(), handler_registry(), profile_->GetPrefs());
    counter_->Init(profile()->GetPrefs(),
                   browsing_data::ClearBrowsingDataTab::ADVANCED,
                   base::BindRepeating(&SiteSettingsCounterTest::Callback,
                                       base::Unretained(this)));
#if BUILDFLAG(IS_ANDROID)
    ClearNotificationsChannels();
#endif
  }

  Profile* profile() { return profile_.get(); }

  HostContentSettingsMap* map() { return map_.get(); }

  content::HostZoomMap* zoom_map() { return zoom_map_; }

  custom_handlers::ProtocolHandlerRegistry* handler_registry() {
    return handler_registry_.get();
  }

  SiteSettingsCounter* counter() { return counter_.get(); }

  void SetSiteSettingsDeletionPref(bool value) {
    profile()->GetPrefs()->SetBoolean(browsing_data::prefs::kDeleteSiteSettings,
                                      value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    profile()->GetPrefs()->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                                      static_cast<int>(period));
  }

  browsing_data::BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  void Callback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    DCHECK(result->Finished());
    finished_ = result->Finished();

    result_ = static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
                  result.get())
                  ->Value();
  }

#if BUILDFLAG(IS_ANDROID)
  void ClearNotificationsChannels() {
    // Because notification channel settings aren't tied to the profile, they
    // will persist across tests. We need to make sure they're reset here.
    for (auto& setting :
         map_->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS)) {
      if (!setting.primary_pattern.MatchesAllHosts() ||
          !setting.secondary_pattern.MatchesAllHosts()) {
        map_->SetContentSettingCustomScope(
            setting.primary_pattern, setting.secondary_pattern,
            ContentSettingsType ::NOTIFICATIONS,
            ContentSetting::CONTENT_SETTING_DEFAULT);
      }
    }
  }
#endif

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  scoped_refptr<HostContentSettingsMap> map_;
  raw_ptr<content::HostZoomMap> zoom_map_;
  std::unique_ptr<custom_handlers::ProtocolHandlerRegistry> handler_registry_;
  std::unique_ptr<SiteSettingsCounter> counter_;
  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt result_;
};

// Tests that the counter correctly counts each setting.
TEST_F(SiteSettingsCounterTest, Count) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
  map()->SetContentSettingDefaultScope(
      GURL("http://maps.google.com"), GURL("http://maps.google.com"),
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  counter()->Restart();
  EXPECT_EQ(2, GetResult());
}

// Test that the counter counts correctly when using a time period.
TEST_F(SiteSettingsCounterTest, CountWithTimePeriod) {
  base::SimpleTestClock test_clock;
  map()->SetClockForTesting(&test_clock);

  // Create a setting at Now()-90min.
  test_clock.SetNow(base::Time::Now() - base::Minutes(90));
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  // Create a setting at Now()-30min.
  test_clock.SetNow(base::Time::Now() - base::Minutes(30));
  map()->SetContentSettingDefaultScope(
      GURL("http://maps.google.com"), GURL("http://maps.google.com"),
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  // Create a setting at Now()-31days.
  test_clock.SetNow(base::Time::Now() - base::Days(31));
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  test_clock.SetNow(base::Time::Now());
  // Only one of the settings was created in the last hour.
  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  EXPECT_EQ(1, GetResult());
  // Both settings were created during the last day.
  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  EXPECT_EQ(2, GetResult());
  // One of the settings was created 31days ago.
  SetDeletionPeriodPref(browsing_data::TimePeriod::OLDER_THAN_30_DAYS);
  EXPECT_EQ(1, GetResult());
}

// Tests that the counter doesn't count website settings
TEST_F(SiteSettingsCounterTest, OnlyCountContentSettings) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
  map()->SetWebsiteSettingDefaultScope(GURL("http://maps.google.com"), GURL(),
                                       ContentSettingsType::SITE_ENGAGEMENT,
                                       base::Value(base::Value::Type::DICT));

  counter()->Restart();
  EXPECT_EQ(1, GetResult());
}

// Tests that the counter counts WebUSB settings
TEST_F(SiteSettingsCounterTest, CountWebUsbSettings) {
  map()->SetWebsiteSettingDefaultScope(GURL("http://www.google.com"),
                                       GURL("http://www.google.com"),
                                       ContentSettingsType::USB_CHOOSER_DATA,
                                       base::Value(base::Value::Type::DICT));

  counter()->Restart();
  EXPECT_EQ(1, GetResult());
}

// Tests that the counter counts settings with the same pattern only
// once.
TEST_F(SiteSettingsCounterTest, OnlyCountPatternOnce) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  counter()->Restart();
  EXPECT_EQ(1, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
TEST_F(SiteSettingsCounterTest, PrefChanged) {
  SetSiteSettingsDeletionPref(false);
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  SetSiteSettingsDeletionPref(true);
  EXPECT_EQ(1, GetResult());
}

// Tests that changing the deletion period restarts the counting.
TEST_F(SiteSettingsCounterTest, PeriodChanged) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  EXPECT_EQ(1, GetResult());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(SiteSettingsCounterTest, ZoomLevel) {
  zoom_map()->SetZoomLevelForHost("google.com", 1.5);
  zoom_map()->SetZoomLevelForHost("www.google.com", 1.5);

  counter()->Restart();
  EXPECT_EQ(2, GetResult());
}

TEST_F(SiteSettingsCounterTest, AllSiteSettingsMixed) {
  zoom_map()->SetZoomLevelForHost("google.com", 1.5);
  zoom_map()->SetZoomLevelForHost("www.google.com", 1.5);

  map()->SetContentSettingDefaultScope(
      GURL("https://www.google.com"), GURL("https://www.google.com"),
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
  map()->SetContentSettingDefaultScope(
      GURL("https://maps.google.com"), GURL("https://maps.google.com"),
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  base::Time now = base::Time::Now();
  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("news", GURL("https://www.google.com"), now,
                      blink::ProtocolHandlerSecurityLevel::kStrict));
  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("news", GURL("https://docs.google.com"), now,
                      blink::ProtocolHandlerSecurityLevel::kStrict));
  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("news", GURL("https://slides.google.com"), now,
                      blink::ProtocolHandlerSecurityLevel::kStrict));

  auto translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(profile()->GetPrefs());
  translate_prefs->AddSiteToNeverPromptList("www.google.com");
  translate_prefs->AddSiteToNeverPromptList("docs.google.com");
  translate_prefs->AddSiteToNeverPromptList("photos.google.com");
  counter()->Restart();
  EXPECT_EQ(6, GetResult());
}
#endif

TEST_F(SiteSettingsCounterTest, ProtocolHandlerCounting) {
  base::Time now = base::Time::Now();

  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("news", GURL("https://www.google.com"), now,
                      blink::ProtocolHandlerSecurityLevel::kStrict));
  handler_registry()->OnAcceptRegisterProtocolHandler(ProtocolHandler(
      "mailto", GURL("https://maps.google.com"), now - base::Minutes(90),
      blink::ProtocolHandlerSecurityLevel::kStrict));
  EXPECT_TRUE(handler_registry()->IsHandledProtocol("news"));
  EXPECT_TRUE(handler_registry()->IsHandledProtocol("mailto"));

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  EXPECT_EQ(2, GetResult());
  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  EXPECT_EQ(1, GetResult());
}

TEST_F(SiteSettingsCounterTest, TranslatedSitesCounting) {
  auto translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(profile()->GetPrefs());
  translate_prefs->AddSiteToNeverPromptList("www.google.com");
  translate_prefs->AddSiteToNeverPromptList("maps.google.com");

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  EXPECT_EQ(2, GetResult());
}

TEST_F(SiteSettingsCounterTest, DiscardingExceptionsCounting) {
  base::Value::Dict exclusion_map;
  exclusion_map.Set("a.com", base::TimeToValue(base::Time::Now()));
  exclusion_map.Set("a.com", base::TimeToValue(base::Time::Now()));
  exclusion_map.Set("b.com",
                    base::TimeToValue(base::Time::Now() - base::Minutes(30)));
  exclusion_map.Set("c.com",
                    base::TimeToValue(base::Time::Now() - base::Hours(2)));
  exclusion_map.Set("d.com",
                    base::TimeToValue(base::Time::Now() - base::Hours(30)));
  profile()->GetPrefs()->SetDict(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptionsWithTime,
      std::move(exclusion_map));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  EXPECT_EQ(2, GetResult());
  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  EXPECT_EQ(4, GetResult());
}

}  // namespace
