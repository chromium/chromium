// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/site_settings_counter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#else
#include "base/android/build_info.h"
#endif

namespace {

class SiteSettingsCounterTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    map_ = HostContentSettingsMapFactory::GetForProfile(profile());
#if !defined(OS_ANDROID)
    zoom_map_ = content::HostZoomMap::GetDefaultForBrowserContext(profile());
#else
    zoom_map_ = nullptr;
#endif
    handler_registry_ = std::make_unique<ProtocolHandlerRegistry>(
        profile(), std::make_unique<TestProtocolHandlerRegistryDelegate>());

    counter_ = std::make_unique<SiteSettingsCounter>(
        map(), zoom_map(), handler_registry(), profile_->GetPrefs());
    counter_->Init(profile()->GetPrefs(),
                   browsing_data::ClearBrowsingDataTab::ADVANCED,
                   base::BindRepeating(&SiteSettingsCounterTest::Callback,
                                       base::Unretained(this)));
  }

  Profile* profile() { return profile_.get(); }

  HostContentSettingsMap* map() { return map_.get(); }

  content::HostZoomMap* zoom_map() { return zoom_map_; }

  ProtocolHandlerRegistry* handler_registry() {
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

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  scoped_refptr<HostContentSettingsMap> map_;
  content::HostZoomMap* zoom_map_;
  std::unique_ptr<ProtocolHandlerRegistry> handler_registry_;
  std::unique_ptr<SiteSettingsCounter> counter_;
  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt result_;
};

// Tests that the counter correctly counts each setting.
TEST_F(SiteSettingsCounterTest, Count) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);
  map()->SetContentSettingDefaultScope(
      GURL("http://maps.google.com"), GURL("http://maps.google.com"),
      ContentSettingsType::GEOLOCATION, std::string(), CONTENT_SETTING_ALLOW);

  counter()->Restart();
  EXPECT_EQ(2, GetResult());
}

// Test that the counter counts correctly when using a time period.
TEST_F(SiteSettingsCounterTest, CountWithTimePeriod) {
#if defined(OS_ANDROID)
  // TODO(crbug.com/981972)
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_OREO) {
    return;
  }
#endif

  base::SimpleTestClock test_clock;
  map()->SetClockForTesting(&test_clock);

  // Create a setting at Now()-90min.
  test_clock.SetNow(base::Time::Now() - base::TimeDelta::FromMinutes(90));
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);

  // Create a setting at Now()-30min.
  test_clock.SetNow(base::Time::Now() - base::TimeDelta::FromMinutes(30));
  map()->SetContentSettingDefaultScope(
      GURL("http://maps.google.com"), GURL("http://maps.google.com"),
      ContentSettingsType::GEOLOCATION, std::string(), CONTENT_SETTING_ALLOW);

  // Create a setting at Now()-31days.
  test_clock.SetNow(base::Time::Now() - base::TimeDelta::FromDays(31));
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::NOTIFICATIONS, std::string(), CONTENT_SETTING_ALLOW);

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
      ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);
  map()->SetWebsiteSettingDefaultScope(
      GURL("http://maps.google.com"), GURL(),
      ContentSettingsType::SITE_ENGAGEMENT, std::string(),
      std::make_unique<base::DictionaryValue>());

  counter()->Restart();
  EXPECT_EQ(1, GetResult());
}

// Tests that the counter counts WebUSB settings
TEST_F(SiteSettingsCounterTest, CountWebUsbSettings) {
  map()->SetWebsiteSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::USB_CHOOSER_DATA, std::string(),
      std::make_unique<base::DictionaryValue>());

  counter()->Restart();
  EXPECT_EQ(1, GetResult());
}

// Tests that the counter counts settings with the same pattern only
// once.
TEST_F(SiteSettingsCounterTest, OnlyCountPatternOnce) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::GEOLOCATION, std::string(), CONTENT_SETTING_ALLOW);

  counter()->Restart();
  EXPECT_EQ(1, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
TEST_F(SiteSettingsCounterTest, PrefChanged) {
  SetSiteSettingsDeletionPref(false);
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);

  SetSiteSettingsDeletionPref(true);
  EXPECT_EQ(1, GetResult());
}

// Tests that changing the deletion period restarts the counting.
TEST_F(SiteSettingsCounterTest, PeriodChanged) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  EXPECT_EQ(1, GetResult());
}

#if !defined(OS_ANDROID)
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
      ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);
  map()->SetContentSettingDefaultScope(
      GURL("https://maps.google.com"), GURL("https://maps.google.com"),
      ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);

  base::Time now = base::Time::Now();
  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("news", GURL("http://www.google.com"), now));
  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("news", GURL("http://docs.google.com"), now));
  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("news", GURL("http://slides.google.com"), now));

  auto translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(profile()->GetPrefs());
  translate_prefs->BlacklistSite("www.google.com");
  translate_prefs->BlacklistSite("docs.google.com");
  translate_prefs->BlacklistSite("photos.google.com");
  counter()->Restart();
  EXPECT_EQ(6, GetResult());
}
#endif

TEST_F(SiteSettingsCounterTest, ProtocolHandlerCounting) {
  base::Time now = base::Time::Now();

  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("news", GURL("http://www.google.com"), now));
  handler_registry()->OnAcceptRegisterProtocolHandler(
      ProtocolHandler("mailto", GURL("http://maps.google.com"),
                      now - base::TimeDelta::FromMinutes(90)));
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
  translate_prefs->BlacklistSite("www.google.com");
  translate_prefs->BlacklistSite("maps.google.com");

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  EXPECT_EQ(2, GetResult());
}

}  // namespace
