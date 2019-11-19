// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/overlay_user_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace content_settings {

class DeadlockCheckerThread : public base::PlatformThread::Delegate {
 public:
  explicit DeadlockCheckerThread(const ContentSettingsPref* pref)
      : pref_(pref) {}

  void ThreadMain() override {
    EXPECT_TRUE(pref_->TryLockForTesting());
  }
 private:
  const ContentSettingsPref* pref_;
  DISALLOW_COPY_AND_ASSIGN(DeadlockCheckerThread);
};

// A helper for observing an preference changes and testing whether
// |PrefProvider| holds a lock when the preferences change.
class DeadlockCheckerObserver {
 public:
  // |DeadlockCheckerObserver| doesn't take the ownership of |prefs| or
  // |provider|.
  DeadlockCheckerObserver(PrefService* prefs, PrefProvider* provider)
      : provider_(provider),
      notification_received_(false) {
    pref_change_registrar_.Init(prefs);
    WebsiteSettingsRegistry* registry = WebsiteSettingsRegistry::GetInstance();
    for (const auto& pair : provider_->content_settings_prefs_) {
      const ContentSettingsPref* pref = pair.second.get();
      pref_change_registrar_.Add(
          registry->Get(pair.first)->pref_name(),
          base::Bind(
              &DeadlockCheckerObserver::OnContentSettingsPatternPairsChanged,
              base::Unretained(this), base::Unretained(pref)));
    }
  }
  virtual ~DeadlockCheckerObserver() {}

  bool notification_received() const {
    return notification_received_;
  }

 private:
  void OnContentSettingsPatternPairsChanged(const ContentSettingsPref* pref) {
    // Check whether |provider_| holds its lock. For this, we need a
    // separate thread.
    DeadlockCheckerThread thread(pref);
    base::PlatformThreadHandle handle;
    ASSERT_TRUE(base::PlatformThread::Create(0, &thread, &handle));
    base::PlatformThread::Join(handle);
    notification_received_ = true;
  }

  PrefProvider* provider_;
  PrefChangeRegistrar pref_change_registrar_;
  bool notification_received_;
  DISALLOW_COPY_AND_ASSIGN(DeadlockCheckerObserver);
};

// Synthesizes a plugin content setting exception into |prefs|. Plugin settings
// are emphemeral as of Chrome M71; this method is used to simulate the scenario
// where we inherit these legacy values from Chrome versions M70-, when the
// exceptions were still stored in preferences.
bool SetLegacyPersistedPluginSetting(
    PrefService* prefs,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ResourceIdentifier& resource_identifier,
    std::unique_ptr<base::Value>&& in_value) {
  auto* registry = ContentSettingsRegistry::GetInstance();
  auto* content_setting_info = registry->Get(ContentSettingsType::PLUGINS);
  PrefChangeRegistrar pref_change_registrar;
  pref_change_registrar.Init(prefs);
  ContentSettingsPref content_settings_pref(
      ContentSettingsType::PLUGINS, prefs, &pref_change_registrar,
      content_setting_info->website_settings_info()->pref_name(),
      false /* is_incognito */, base::DoNothing());
  return content_settings_pref.SetWebsiteSetting(
      primary_pattern, secondary_pattern, resource_identifier,
      base::Time::Now(), std::move(in_value));
}

class PrefProviderTest : public testing::Test {
 public:
  PrefProviderTest() {
    // Ensure all content settings are initialized.
    ContentSettingsRegistry::GetInstance();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PrefProviderTest, Observer) {
  TestingProfile profile;
  PrefProvider pref_content_settings_provider(profile.GetPrefs(),
                                              false /* incognito */,
                                              true /* store_last_modified */);

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 pattern, ContentSettingsPattern::Wildcard(),
                                 ContentSettingsType::COOKIES, ""));

  pref_content_settings_provider.AddObserver(&mock_observer);

  pref_content_settings_provider.SetWebsiteSetting(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  pref_content_settings_provider.ShutdownOnUIThread();
}

// Tests that fullscreen and mouselock content settings are cleared.
TEST_F(PrefProviderTest, DiscardObsoleteFullscreenAndMouselockPreferences) {
  static const char kFullscreenPrefPath[] =
      "profile.content_settings.exceptions.fullscreen";
#if !defined(OS_ANDROID)
  static const char kMouselockPrefPath[] =
      "profile.content_settings.exceptions.mouselock";
#endif
  static const char kGeolocationPrefPath[] =
      "profile.content_settings.exceptions.geolocation";
  static const char kPattern[] = "[*.]example.com";

  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set some pref data. Each content setting type has the following value:
  // {"[*.]example.com": {"setting": 1}}
  base::DictionaryValue pref_data;
  auto data_for_pattern = std::make_unique<base::DictionaryValue>();
  data_for_pattern->SetInteger("setting", CONTENT_SETTING_ALLOW);
  pref_data.SetWithoutPathExpansion(kPattern, std::move(data_for_pattern));
  prefs->Set(kFullscreenPrefPath, pref_data);
#if !defined(OS_ANDROID)
  prefs->Set(kMouselockPrefPath, pref_data);
#endif
  prefs->Set(kGeolocationPrefPath, pref_data);

  // Instantiate a new PrefProvider here, because we want to test the
  // constructor's behavior after setting the above.
  PrefProvider provider(prefs, false /* incognito */,
                        true /* store_last_modified */);
  provider.ShutdownOnUIThread();

  // Check that fullscreen and mouselock have been deleted.
  EXPECT_FALSE(prefs->HasPrefPath(kFullscreenPrefPath));
#if !defined(OS_ANDROID)
  EXPECT_FALSE(prefs->HasPrefPath(kMouselockPrefPath));
#endif
  EXPECT_TRUE(prefs->HasPrefPath(kGeolocationPrefPath));
  GURL primary_url("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         std::string(), false));
}

// Test for regression in which the PrefProvider modified the user pref store
// of the OTR unintentionally: http://crbug.com/74466.
TEST_F(PrefProviderTest, Incognito) {
  PersistentPrefStore* user_prefs = new TestingPrefStore();
  OverlayUserPrefStore* otr_user_prefs =
      new OverlayUserPrefStore(user_prefs);

  sync_preferences::PrefServiceMockFactory factory;
  factory.set_user_prefs(base::WrapRefCounted(user_prefs));
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable);
  sync_preferences::PrefServiceSyncable* regular_prefs =
      factory.CreateSyncable(registry.get()).release();

  RegisterUserProfilePrefs(registry.get());

  sync_preferences::PrefServiceMockFactory otr_factory;
  otr_factory.set_user_prefs(base::WrapRefCounted(otr_user_prefs));
  scoped_refptr<user_prefs::PrefRegistrySyncable> otr_registry(
      new user_prefs::PrefRegistrySyncable);
  sync_preferences::PrefServiceSyncable* otr_prefs =
      otr_factory.CreateSyncable(otr_registry.get()).release();

  RegisterUserProfilePrefs(otr_registry.get());

  TestingProfile::Builder profile_builder;
  profile_builder.SetPrefService(base::WrapUnique(regular_prefs));
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();

  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.SetPrefService(base::WrapUnique(otr_prefs));
  otr_profile_builder.BuildIncognito(profile.get());

  PrefProvider pref_content_settings_provider(
      regular_prefs, false /* incognito */, true /* store_last_modified */);
  PrefProvider pref_content_settings_provider_incognito(
      otr_prefs, true /* incognito */, true /* store_last_modified */);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  pref_content_settings_provider.SetWebsiteSetting(
      pattern, pattern, ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  GURL host("http://example.com/");
  // The value should of course be visible in the regular PrefProvider.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host,
                                         host, ContentSettingsType::COOKIES,
                                         std::string(), false));
  // And also in the OTR version.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                &pref_content_settings_provider_incognito, host, host,
                ContentSettingsType::COOKIES, std::string(), false));
  const WebsiteSettingsInfo* info =
      WebsiteSettingsRegistry::GetInstance()->Get(ContentSettingsType::COOKIES);
  // But the value should not be overridden in the OTR user prefs accidentally.
  EXPECT_FALSE(otr_user_prefs->IsSetInOverlay(info->pref_name()));

  pref_content_settings_provider.ShutdownOnUIThread();
  pref_content_settings_provider_incognito.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, GetContentSettingsValue) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), false /* incognito */,
                        true /* store_last_modified */);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));

  EXPECT_EQ(NULL, TestUtils::GetContentSettingValue(
                      &provider, primary_url, primary_url,
                      ContentSettingsType::COOKIES, std::string(), false));

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::COOKIES,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
  std::unique_ptr<base::Value> value_ptr(TestUtils::GetContentSettingValue(
      &provider, primary_url, primary_url, ContentSettingsType::COOKIES,
      std::string(), false));
  int int_value = -1;
  value_ptr->GetAsInteger(&int_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, IntToContentSetting(int_value));

  provider.SetWebsiteSetting(primary_pattern, primary_pattern,
                             ContentSettingsType::COOKIES, std::string(),
                             nullptr);
  EXPECT_EQ(NULL, TestUtils::GetContentSettingValue(
                      &provider, primary_url, primary_url,
                      ContentSettingsType::COOKIES, std::string(), false));
  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, Patterns) {
  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(testing_profile.GetPrefs(),
                                              false /* incognito */,
                                              true /* store_last_modified */);

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  GURL host4("file:///tmp/test.html");
  ContentSettingsPattern pattern1 =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("example.org");
  ContentSettingsPattern pattern3 =
      ContentSettingsPattern::FromString("file:///tmp/test.html");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host1,
                                         host1, ContentSettingsType::COOKIES,
                                         std::string(), false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern1, pattern1, ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host1,
                                         host1, ContentSettingsType::COOKIES,
                                         std::string(), false));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host2,
                                         host2, ContentSettingsType::COOKIES,
                                         std::string(), false));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host3,
                                         host3, ContentSettingsType::COOKIES,
                                         std::string(), false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern2, pattern2, ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host3,
                                         host3, ContentSettingsType::COOKIES,
                                         std::string(), false));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host4,
                                         host4, ContentSettingsType::COOKIES,
                                         std::string(), false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern3, pattern3, ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host4,
                                         host4, ContentSettingsType::COOKIES,
                                         std::string(), false));

  pref_content_settings_provider.ShutdownOnUIThread();
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(PrefProviderTest, ResourceIdentifier) {
  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(testing_profile.GetPrefs(),
                                              false /* incognito */,
                                              true /* store_last_modified */);

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  std::string resource1("someplugin");
  std::string resource2("otherplugin");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host,
                                         host, ContentSettingsType::PLUGINS,
                                         resource1, false));
  std::unique_ptr<base::Value> value(new base::Value(CONTENT_SETTING_BLOCK));
  pref_content_settings_provider.SetWebsiteSetting(pattern, pattern,
                                                   ContentSettingsType::PLUGINS,
                                                   resource1, std::move(value));

  ASSERT_EQ(ContentSettingsInfo::EPHEMERAL,
            ContentSettingsRegistry::GetInstance()
                ->Get(ContentSettingsType::PLUGINS)
                ->storage_behavior());
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host,
                                         host, ContentSettingsType::PLUGINS,
                                         resource1, false));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&pref_content_settings_provider, host,
                                         host, ContentSettingsType::PLUGINS,
                                         resource2, false));

  pref_content_settings_provider.ShutdownOnUIThread();
}
#endif

// http://crosbug.com/17760
TEST_F(PrefProviderTest, Deadlock) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  // Chain of events: a preference changes, |PrefProvider| notices it, and reads
  // and writes the preference. When the preference is written, a notification
  // is sent, and this used to happen when |PrefProvider| was still holding its
  // lock.

  const WebsiteSettingsInfo* info =
      WebsiteSettingsRegistry::GetInstance()->Get(ContentSettingsType::COOKIES);
  PrefProvider provider(&prefs, false /* incognito */,
                        true /* store_last_modified */);
  DeadlockCheckerObserver observer(&prefs, &provider);
  {
    DictionaryPrefUpdate update(&prefs, info->pref_name());
    base::DictionaryValue* mutable_settings = update.Get();
    mutable_settings->SetWithoutPathExpansion(
        "www.example.com,*", std::make_unique<base::DictionaryValue>());
  }
  EXPECT_TRUE(observer.notification_received());

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, IncognitoInheritsValueMap) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  ContentSettingsPattern pattern_1 =
      ContentSettingsPattern::FromString("google.com");
  ContentSettingsPattern pattern_2 =
      ContentSettingsPattern::FromString("www.google.com");
  ContentSettingsPattern wildcard =
      ContentSettingsPattern::FromString("*");
  std::unique_ptr<base::Value> value(new base::Value(CONTENT_SETTING_ALLOW));

  // Create a normal provider and set a setting.
  PrefProvider normal_provider(&prefs, false /* incognito */,
                               true /* store_last_modified */);
  normal_provider.SetWebsiteSetting(
      pattern_1, wildcard, ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(value->Clone()));

  // Non-OTR provider, Non-OTR iterator has one setting (pattern 1).
  {
    std::unique_ptr<RuleIterator> it(normal_provider.GetRuleIterator(
        ContentSettingsType::COOKIES, std::string(), false));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_1, it->Next().primary_pattern);
    EXPECT_FALSE(it->HasNext());
  }

  // Non-OTR provider, OTR iterator has no settings.
  {
    std::unique_ptr<RuleIterator> it(normal_provider.GetRuleIterator(
        ContentSettingsType::COOKIES, std::string(), true));
    EXPECT_FALSE(it);
  }

  // Create an incognito provider and set a setting.
  PrefProvider incognito_provider(&prefs, true /* incognito */,
                                  true /* store_last_modified */);
  incognito_provider.SetWebsiteSetting(
      pattern_2, wildcard, ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(value->Clone()));

  // OTR provider, non-OTR iterator has one setting (pattern 1).
  {
    std::unique_ptr<RuleIterator> it(incognito_provider.GetRuleIterator(
        ContentSettingsType::COOKIES, std::string(), false));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_1, it->Next().primary_pattern);
    EXPECT_FALSE(it->HasNext());
  }

  // OTR provider, OTR iterator has one setting (pattern 2).
  {
    std::unique_ptr<RuleIterator> it(incognito_provider.GetRuleIterator(
        ContentSettingsType::COOKIES, std::string(), true));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_2, it->Next().primary_pattern);
    EXPECT_FALSE(it->HasNext());
  }

  incognito_provider.ShutdownOnUIThread();
  normal_provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, ClearAllContentSettingsRules) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("google.com");
  ContentSettingsPattern wildcard =
      ContentSettingsPattern::FromString("*");
  std::unique_ptr<base::Value> value(new base::Value(CONTENT_SETTING_ALLOW));

  PrefProvider provider(&prefs, false /* incognito */,
                        true /* store_last_modified */);

  // Non-empty pattern, syncable, empty resource identifier.
  provider.SetWebsiteSetting(pattern, wildcard, ContentSettingsType::JAVASCRIPT,
                             ResourceIdentifier(),
                             std::make_unique<base::Value>(value->Clone()));

  // Non-empty pattern, non-syncable, empty resource identifier.
  provider.SetWebsiteSetting(
      pattern, wildcard, ContentSettingsType::GEOLOCATION, ResourceIdentifier(),
      std::make_unique<base::Value>(value->Clone()));

#if BUILDFLAG(ENABLE_PLUGINS)
  // Plugin settings became emphemeral as of Chrome M71 and are no longer
  // persisted into preferences. Here we simulate the scenario where we inherit
  // legacy, persisted values coming from Chrome versions M70-, to verify the
  // ability of PrefProvider to delete the legacy plugin settings although those
  // are no longer handled by it (as opposed to the first section of the test,
  // which verifies the deletion of regular settings which are still handled by
  // PrefProvider).

  // Legacy, persisted exception for ContentSettingsType::PLUGINS with a
  // non-empty pattern, and non-empty resource identifier.
  ASSERT_TRUE(SetLegacyPersistedPluginSetting(
      &prefs, pattern, wildcard, ResourceIdentifier(),
      std::make_unique<base::Value>(value->Clone())));

  // Non-empty pattern, plugins, empty resource identifier.
  provider.SetWebsiteSetting(pattern, wildcard, ContentSettingsType::PLUGINS,
                             ResourceIdentifier(),
                             std::make_unique<base::Value>(value->Clone()));
#endif

  // Non-empty pattern, syncable, empty resource identifier.
  provider.SetWebsiteSetting(pattern, wildcard, ContentSettingsType::COOKIES,
                             ResourceIdentifier(),
                             std::make_unique<base::Value>(value->Clone()));

  // Non-empty pattern, non-syncable, empty resource identifier.
  provider.SetWebsiteSetting(
      pattern, wildcard, ContentSettingsType::NOTIFICATIONS,
      ResourceIdentifier(), std::make_unique<base::Value>(value->Clone()));

  // Test that the preferences for images, geolocation and plugins get cleared.
  WebsiteSettingsRegistry* registry = WebsiteSettingsRegistry::GetInstance();
  const char* cleared_prefs[] = {
    registry->Get(ContentSettingsType::JAVASCRIPT)->pref_name().c_str(),
    registry->Get(ContentSettingsType::GEOLOCATION)->pref_name().c_str(),
#if BUILDFLAG(ENABLE_PLUGINS)
    registry->Get(ContentSettingsType::PLUGINS)->pref_name().c_str(),
#endif
  };

  // Expect the prefs are not empty before we trigger clearing them.
  for (const char* pref : cleared_prefs) {
    DictionaryPrefUpdate update(&prefs, pref);
    const base::DictionaryValue* dictionary = update.Get();
    ASSERT_FALSE(dictionary->empty());
  }

  provider.ClearAllContentSettingsRules(ContentSettingsType::JAVASCRIPT);
  provider.ClearAllContentSettingsRules(ContentSettingsType::GEOLOCATION);
#if BUILDFLAG(ENABLE_PLUGINS)
  provider.ClearAllContentSettingsRules(ContentSettingsType::PLUGINS);
#endif

  // Ensure they become empty afterwards.
  for (const char* pref : cleared_prefs) {
    DictionaryPrefUpdate update(&prefs, pref);
    const base::DictionaryValue* dictionary = update.Get();
    EXPECT_TRUE(dictionary->empty());
  }

  // Test that the preferences for cookies and notifications are not empty.
  const char* nonempty_prefs[] = {
      registry->Get(ContentSettingsType::COOKIES)->pref_name().c_str(),
      registry->Get(ContentSettingsType::NOTIFICATIONS)->pref_name().c_str(),
  };

  for (const char* pref : nonempty_prefs) {
    DictionaryPrefUpdate update(&prefs, pref);
    const base::DictionaryValue* dictionary = update.Get();
    EXPECT_EQ(1u, dictionary->size());
  }

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, LastModified) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  ContentSettingsPattern pattern_1 =
      ContentSettingsPattern::FromString("google.com");
  ContentSettingsPattern pattern_2 =
      ContentSettingsPattern::FromString("www.google.com");
  auto value = std::make_unique<base::Value>(CONTENT_SETTING_ALLOW);

  // Create a  provider and set a few settings.
  PrefProvider provider(&prefs, false /* incognito */,
                        true /* store_last_modified */);
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  provider.SetClockForTesting(&test_clock);

  base::Time t1 = test_clock.Now();

  provider.SetWebsiteSetting(pattern_1, ContentSettingsPattern::Wildcard(),
                             ContentSettingsType::COOKIES, std::string(),
                             std::make_unique<base::Value>(value->Clone()));
  provider.SetWebsiteSetting(pattern_2, ContentSettingsPattern::Wildcard(),
                             ContentSettingsType::COOKIES, std::string(),
                             std::make_unique<base::Value>(value->Clone()));
  // Make sure that the timestamps for pattern_1 and patter_2 are before |t2|.
  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  base::Time t2 = test_clock.Now();

  base::Time last_modified = provider.GetWebsiteSettingLastModified(
      pattern_1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string());
  EXPECT_EQ(last_modified, t1);
  last_modified = provider.GetWebsiteSettingLastModified(
      pattern_2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string());
  EXPECT_EQ(last_modified, t1);

  // A change for pattern_1, which will update the last_modified timestamp.
  auto value2 = std::make_unique<base::Value>(CONTENT_SETTING_BLOCK);
  provider.SetWebsiteSetting(pattern_1, ContentSettingsPattern::Wildcard(),
                             ContentSettingsType::COOKIES, std::string(),
                             std::make_unique<base::Value>(value2->Clone()));

  last_modified = provider.GetWebsiteSettingLastModified(
      pattern_1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string());
  EXPECT_EQ(last_modified, t2);

  // The timestamp of pattern_2 shouldn't change.
  last_modified = provider.GetWebsiteSettingLastModified(
      pattern_2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string());
  EXPECT_EQ(last_modified, t1);

  provider.ShutdownOnUIThread();
}

// Tests if PrefProvider rejects storing ephemeral types.
TEST_F(PrefProviderTest, RejectEphemeralStorage) {
  // Find an ephemeral type.
  ContentSettingsType ephemeral_type = ContentSettingsType::NUM_TYPES;
  ContentSettingsRegistry* registry = ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* item : *registry) {
    if (item->storage_behavior() == ContentSettingsInfo::EPHEMERAL) {
      ephemeral_type = item->website_settings_info()->type();
      break;
    }
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  // At the very least, ContentSettingsType::PLUGINS is ephemeral.
  ASSERT_NE(ContentSettingsType::NUM_TYPES, ephemeral_type);
#else
  // There might be no ephemeral setting if plugins are not supported.
  if (ephemeral_type == ContentSettingsType::NUM_TYPES)
    return;
#endif

  sync_preferences::TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());
  PrefProvider provider(&prefs, false /* regular */,
                        true /* store_last_modified */);
  ContentSettingsPattern site_pattern =
      ContentSettingsPattern::FromString("https://example.com");

  std::unique_ptr<base::Value> value(new base::Value(CONTENT_SETTING_ALLOW));
  EXPECT_FALSE(provider.SetWebsiteSetting(site_pattern, site_pattern,
                                          ephemeral_type, std::string(),
                                          std::move(value)));
  std::unique_ptr<RuleIterator> rule_iterator =
      provider.GetRuleIterator(ephemeral_type, std::string(), false);
  EXPECT_EQ(nullptr, rule_iterator);

  provider.ShutdownOnUIThread();
}


}  // namespace content_settings
