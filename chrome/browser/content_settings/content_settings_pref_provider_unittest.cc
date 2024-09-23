// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
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

  DeadlockCheckerThread(const DeadlockCheckerThread&) = delete;
  DeadlockCheckerThread& operator=(const DeadlockCheckerThread&) = delete;

  void ThreadMain() override { EXPECT_TRUE(pref_->TryLockForTesting()); }

 private:
  raw_ptr<const ContentSettingsPref> pref_;
};

// A helper for observing an preference changes and testing whether
// |PrefProvider| holds a lock when the preferences change.
class DeadlockCheckerObserver {
 public:
  // |DeadlockCheckerObserver| doesn't take the ownership of |prefs| or
  // |provider|.
  DeadlockCheckerObserver(PrefService* prefs, PrefProvider* provider)
      : provider_(provider), notification_received_(false) {
    pref_change_registrar_.Init(prefs);
    WebsiteSettingsRegistry* registry = WebsiteSettingsRegistry::GetInstance();
    for (const auto& pair : provider_->content_settings_prefs_) {
      const ContentSettingsPref* pref = pair.second.get();
      pref_change_registrar_.Add(
          registry->Get(pair.first)->pref_name(),
          base::BindRepeating(
              &DeadlockCheckerObserver::OnContentSettingsPatternPairsChanged,
              base::Unretained(this), base::Unretained(pref)));
    }
  }

  DeadlockCheckerObserver(const DeadlockCheckerObserver&) = delete;
  DeadlockCheckerObserver& operator=(const DeadlockCheckerObserver&) = delete;

  virtual ~DeadlockCheckerObserver() = default;

  bool notification_received() const { return notification_received_; }

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

  raw_ptr<PrefProvider> provider_;
  PrefChangeRegistrar pref_change_registrar_;
  bool notification_received_;
};

class PrefProviderTest : public testing::Test {
 public:
  PrefProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Ensure all content settings are initialized.
    ContentSettingsRegistry::GetInstance();
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PrefProviderTest, Observer) {
  TestingProfile profile;
  PrefProvider pref_content_settings_provider(profile.GetPrefs(),
                                              /*off_the_record=*/false,
                                              /*store_last_modified=*/true,
                                              /*restore_session=*/false);

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 pattern, ContentSettingsPattern::Wildcard(),
                                 ContentSettingsType::COOKIES));

  pref_content_settings_provider.AddObserver(&mock_observer);

  pref_content_settings_provider.SetWebsiteSetting(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_ALLOW), {},
      content_settings::PartitionKey::GetDefaultForTesting());

  pref_content_settings_provider.ShutdownOnUIThread();
}

// Tests that obsolete content settings are cleared.
TEST_F(PrefProviderTest, DiscardObsoletePreferences) {
#if !BUILDFLAG(IS_ANDROID)
  const char kObsoleteInstalledWebAppMetadataExceptionsPref[] =
      "profile.content_settings.exceptions.installed_web_app_metadata";
#endif
  static const char kGeolocationPrefPath[] =
      "profile.content_settings.exceptions.geolocation";
  static const char kGetDisplayMediaSetSelectAllScreensAllowedForUrlsPref[] =
      "profile.content_settings.exceptions.get_display_media_set_select_all_"
      "screens";
  static const char kPattern[] = "[*.]example.com";

  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set some pref data. Each content setting type has the following value:
  // {"[*.]example.com": {"setting": 1}}
  base::Value::Dict plugins_data_pref;
  constexpr char kFlagKey[] = "flashPreviouslyChanged";
  plugins_data_pref.Set(kFlagKey, base::Value::Dict());

  base::Value::Dict data_for_pattern;
  data_for_pattern.Set("setting", static_cast<int>(CONTENT_SETTING_ALLOW));
  base::Value::Dict pref_data;
  base::Value::List pref_list;
  pref_data.Set(kPattern, std::move(data_for_pattern));
#if !BUILDFLAG(IS_ANDROID)
  prefs->SetDict(kObsoleteInstalledWebAppMetadataExceptionsPref,
                 pref_data.Clone());
#endif
  prefs->SetDict(kGeolocationPrefPath, std::move(pref_data));
  prefs->SetList(kGetDisplayMediaSetSelectAllScreensAllowedForUrlsPref,
                 std::move(pref_list));

  // Instantiate a new PrefProvider here, because we want to test the
  // constructor's behavior after setting the above.
  PrefProvider provider(prefs, /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);
  provider.ShutdownOnUIThread();

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(
      prefs->HasPrefPath(kObsoleteInstalledWebAppMetadataExceptionsPref));
#endif
  EXPECT_FALSE(prefs->HasPrefPath(
      kGetDisplayMediaSetSelectAllScreensAllowedForUrlsPref));
  EXPECT_TRUE(prefs->HasPrefPath(kGeolocationPrefPath));
  GURL primary_url("http://example.com/");
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::GEOLOCATION, false));
}

// Test for regression in which the PrefProvider modified the user pref store
// of the OTR unintentionally: http://crbug.com/74466.
TEST_F(PrefProviderTest, Incognito) {
  PersistentPrefStore* user_prefs = new TestingPrefStore();
  OverlayUserPrefStore* otr_user_prefs = new OverlayUserPrefStore(user_prefs);

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
      regular_prefs, /*off_the_record=*/false, /*store_last_modified=*/true,
      /*restore_session=*/false);
  PrefProvider pref_content_settings_provider_incognito(
      otr_prefs, true /*off_the_record=*/, /*store_last_modified=*/true,
      /*restore_session=*/false);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  pref_content_settings_provider.SetWebsiteSetting(
      pattern, pattern, ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_ALLOW), {},
      content_settings::PartitionKey::GetDefaultForTesting());

  GURL host("http://example.com/");
  // The value should of course be visible in the regular PrefProvider.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      TestUtils::GetContentSetting(&pref_content_settings_provider, host, host,
                                   ContentSettingsType::COOKIES, false));
  // And also in the OTR version.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                &pref_content_settings_provider_incognito, host, host,
                ContentSettingsType::COOKIES, false));
  const WebsiteSettingsInfo* info =
      WebsiteSettingsRegistry::GetInstance()->Get(ContentSettingsType::COOKIES);
  // But the value should not be overridden in the OTR user prefs accidentally.
  EXPECT_FALSE(otr_user_prefs->IsSetInOverlay(info->pref_name()));

  pref_content_settings_provider.ShutdownOnUIThread();
  pref_content_settings_provider_incognito.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, GetContentSettingsValue) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::COOKIES, false));

  EXPECT_EQ(base::Value(), TestUtils::GetContentSettingValue(
                               &provider, primary_url, primary_url,
                               ContentSettingsType::COOKIES, false));

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_BLOCK), {},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::COOKIES, false));
  base::Value value = TestUtils::GetContentSettingValue(
      &provider, primary_url, primary_url, ContentSettingsType::COOKIES, false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::COOKIES,
      base::Value(), {},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(base::Value(), TestUtils::GetContentSettingValue(
                               &provider, primary_url, primary_url,
                               ContentSettingsType::COOKIES, false));
  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, Patterns) {
  TestingProfile testing_profile;
  PrefProvider pref_content_settings_provider(testing_profile.GetPrefs(),
                                              /*off_the_record=*/false,
                                              /*store_last_modified=*/true,
                                              /*restore_session=*/false);

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

  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&pref_content_settings_provider, host1,
                                   host1, ContentSettingsType::COOKIES, false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern1, pattern1, ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_BLOCK), {},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&pref_content_settings_provider, host1,
                                   host1, ContentSettingsType::COOKIES, false));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&pref_content_settings_provider, host2,
                                   host2, ContentSettingsType::COOKIES, false));

  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&pref_content_settings_provider, host3,
                                   host3, ContentSettingsType::COOKIES, false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern2, pattern2, ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_BLOCK), {},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&pref_content_settings_provider, host3,
                                   host3, ContentSettingsType::COOKIES, false));

  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&pref_content_settings_provider, host4,
                                   host4, ContentSettingsType::COOKIES, false));
  pref_content_settings_provider.SetWebsiteSetting(
      pattern3, pattern3, ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_BLOCK), {},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&pref_content_settings_provider, host4,
                                   host4, ContentSettingsType::COOKIES, false));

  pref_content_settings_provider.ShutdownOnUIThread();
}

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
  PrefProvider provider(&prefs, /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  DeadlockCheckerObserver observer(&prefs, &provider);
  {
    ScopedDictPrefUpdate update(&prefs, info->pref_name());
    base::Value::Dict& mutable_settings = update.Get();
    mutable_settings.Set("www.example.com,*",
                         base::Value(base::Value::Type::DICT));
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
  ContentSettingsPattern pattern_3 =
      ContentSettingsPattern::FromString("example.com");
  ContentSettingsPattern pattern_4 =
      ContentSettingsPattern::FromString("foo.com");
  ContentSettingsPattern pattern_5 =
      ContentSettingsPattern::FromString("bar.com");

  ContentSettingsPattern wildcard = ContentSettingsPattern::FromString("*");

  // Create a normal provider and set a setting.
  PrefProvider normal_provider(&prefs, /*off_the_record=*/false,
                               /*store_last_modified=*/true,
                               /*restore_session=*/false);

  {
    ContentSettingConstraints constraints;
    constraints.set_session_model(mojom::SessionModel::USER_SESSION);

    normal_provider.SetWebsiteSetting(
        pattern_1, wildcard, ContentSettingsType::COOKIES,
        base::Value(CONTENT_SETTING_ALLOW), {},
        content_settings::PartitionKey::GetDefaultForTesting());
    normal_provider.SetWebsiteSetting(
        pattern_3, pattern_3, ContentSettingsType::COOKIES,
        base::Value(CONTENT_SETTING_BLOCK), constraints,
        content_settings::PartitionKey::GetDefaultForTesting());
  }
  {
    // Durable and not expired
    ContentSettingConstraints constraints;
    constraints.set_lifetime(base::Days(1));
    constraints.set_session_model(mojom::SessionModel::DURABLE);
    normal_provider.SetWebsiteSetting(
        pattern_4, pattern_4, ContentSettingsType::COOKIES,
        base::Value(CONTENT_SETTING_BLOCK), constraints,
        content_settings::PartitionKey::GetDefaultForTesting());
  }
  {
    // Durable but expired
    ContentSettingConstraints constraints(base::Time::Now() - base::Days(2));
    constraints.set_lifetime(base::Days(1));
    constraints.set_session_model(mojom::SessionModel::DURABLE);
    normal_provider.SetWebsiteSetting(
        pattern_5, pattern_5, ContentSettingsType::COOKIES,
        base::Value(CONTENT_SETTING_BLOCK), constraints,
        content_settings::PartitionKey::GetDefaultForTesting());
  }
  // Non-OTR provider, Non-OTR iterator has one setting (pattern 1) using
  // default params and one scoped to a UserSession lifetime model.
  {
    std::unique_ptr<RuleIterator> it(normal_provider.GetRuleIterator(
        ContentSettingsType::COOKIES, false,
        content_settings::PartitionKey::GetDefaultForTesting()));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_5, it->Next()->primary_pattern);
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_3, it->Next()->primary_pattern);
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_4, it->Next()->primary_pattern);
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_1, it->Next()->primary_pattern);
    EXPECT_FALSE(it->HasNext());
  }

  // Non-OTR provider, OTR iterator has no settings.
  {
    std::unique_ptr<RuleIterator> it(normal_provider.GetRuleIterator(
        ContentSettingsType::COOKIES, true,
        content_settings::PartitionKey::GetDefaultForTesting()));
    EXPECT_FALSE(it);
  }

  // Create an incognito provider and set a setting.
  PrefProvider incognito_provider(&prefs, true /* incognito */,
                                  /*store_last_modified=*/true,
                                  /*restore_session=*/false);

  incognito_provider.SetWebsiteSetting(
      pattern_2, wildcard, ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_ALLOW), {},
      content_settings::PartitionKey::GetDefaultForTesting());

  // OTR provider, non-OTR iterator has two settings (pattern 1/3).
  {
    std::unique_ptr<RuleIterator> it(incognito_provider.GetRuleIterator(
        ContentSettingsType::COOKIES, false,
        content_settings::PartitionKey::GetDefaultForTesting()));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_3, it->Next()->primary_pattern);
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_4, it->Next()->primary_pattern);
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_1, it->Next()->primary_pattern);
    EXPECT_FALSE(it->HasNext());
  }

  // OTR provider, OTR iterator has one setting (pattern 2).
  {
    std::unique_ptr<RuleIterator> it(incognito_provider.GetRuleIterator(
        ContentSettingsType::COOKIES, true,
        content_settings::PartitionKey::GetDefaultForTesting()));
    EXPECT_TRUE(it->HasNext());
    EXPECT_EQ(pattern_2, it->Next()->primary_pattern);
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
  ContentSettingsPattern wildcard = ContentSettingsPattern::FromString("*");
  base::Value value(CONTENT_SETTING_ALLOW);

  PrefProvider provider(&prefs, /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  // Non-empty pattern, syncable, empty resource identifier.
  provider.SetWebsiteSetting(
      pattern, wildcard, ContentSettingsType::JAVASCRIPT,

      value.Clone(), {},
      content_settings::PartitionKey::GetDefaultForTesting());

  // Non-empty pattern, non-syncable, empty resource identifier.
  provider.SetWebsiteSetting(
      pattern, wildcard, ContentSettingsType::GEOLOCATION, value.Clone(), {},
      content_settings::PartitionKey::GetDefaultForTesting());

  // Non-empty pattern, syncable, empty resource identifier.
  provider.SetWebsiteSetting(
      pattern, wildcard, ContentSettingsType::COOKIES,

      value.Clone(), {},
      content_settings::PartitionKey::GetDefaultForTesting());

  // Non-empty pattern, non-syncable, empty resource identifier.
  provider.SetWebsiteSetting(
      pattern, wildcard, ContentSettingsType::NOTIFICATIONS, value.Clone(), {},
      content_settings::PartitionKey::GetDefaultForTesting());

  // Test that the preferences for images, geolocation and plugins get cleared.
  WebsiteSettingsRegistry* registry = WebsiteSettingsRegistry::GetInstance();
  const char* cleared_prefs[] = {
    registry->Get(ContentSettingsType::JAVASCRIPT)->pref_name().c_str(),
    registry->Get(ContentSettingsType::GEOLOCATION)->pref_name().c_str(),
  };

  // Expect the prefs are not empty before we trigger clearing them.
  for (const char* pref : cleared_prefs) {
    const base::Value::Dict& dictionary = prefs.GetDict(pref);
    ASSERT_FALSE(dictionary.empty());
  }

  provider.ClearAllContentSettingsRules(
      ContentSettingsType::JAVASCRIPT,
      content_settings::PartitionKey::GetDefaultForTesting());
  provider.ClearAllContentSettingsRules(
      ContentSettingsType::GEOLOCATION,
      content_settings::PartitionKey::GetDefaultForTesting());

  // Ensure they become empty afterwards.
  for (const char* pref : cleared_prefs) {
    const base::Value::Dict& dictionary = prefs.GetDict(pref);
    EXPECT_TRUE(dictionary.empty());
  }

  // Test that the preferences for cookies and notifications are not empty.
  const char* nonempty_prefs[] = {
      registry->Get(ContentSettingsType::COOKIES)->pref_name().c_str(),
      registry->Get(ContentSettingsType::NOTIFICATIONS)->pref_name().c_str(),
  };

  for (const char* pref : nonempty_prefs) {
    const base::Value::Dict& dictionary = prefs.GetDict(pref);
    EXPECT_EQ(1u, dictionary.size());
  }

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, LastModified) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  GURL url1("https://google.com");
  GURL url2("https://www.google.com");
  ContentSettingsPattern pattern_1 =
      ContentSettingsPattern::FromString("google.com");
  ContentSettingsPattern pattern_2 =
      ContentSettingsPattern::FromString("www.google.com");
  // Create a  provider and set a few settings.
  PrefProvider provider(&prefs, /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  provider.SetClockForTesting(&test_clock);

  base::Time t1 = test_clock.Now();

  provider.SetWebsiteSetting(
      pattern_1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_ALLOW), {},
      content_settings::PartitionKey::GetDefaultForTesting());
  provider.SetWebsiteSetting(
      pattern_2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_ALLOW), {},
      content_settings::PartitionKey::GetDefaultForTesting());
  // Make sure that the timestamps for pattern_1 and patter_2 are before |t2|.
  test_clock.Advance(base::Seconds(1));
  base::Time t2 = test_clock.Now();

  base::Time last_modified = TestUtils::GetLastModified(
      &provider, url1, url1, ContentSettingsType::COOKIES);
  EXPECT_EQ(t1, last_modified);
  last_modified = TestUtils::GetLastModified(&provider, url2, url2,
                                             ContentSettingsType::COOKIES);
  EXPECT_EQ(t1, last_modified);

  // A change for pattern_1, which will update the last_modified timestamp.
  ;
  provider.SetWebsiteSetting(
      pattern_1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_BLOCK), {},
      content_settings::PartitionKey::GetDefaultForTesting());

  last_modified = TestUtils::GetLastModified(&provider, url1, url1,
                                             ContentSettingsType::COOKIES);
  EXPECT_EQ(t2, last_modified);

  // The timestamp of pattern_2 shouldn't change.
  last_modified = TestUtils::GetLastModified(&provider, url2, url2,
                                             ContentSettingsType::COOKIES);
  EXPECT_EQ(t1, last_modified);

  provider.ShutdownOnUIThread();

  // Ensure the timestamps survive roundtrip through a provider reload properly.
  PrefProvider provider2(&prefs, /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/false);

  last_modified = TestUtils::GetLastModified(&provider, url1, url1,
                                             ContentSettingsType::COOKIES);
  EXPECT_EQ(t2, last_modified);

  // The timestamp of pattern_2 shouldn't change.
  last_modified = TestUtils::GetLastModified(&provider, url2, url2,
                                             ContentSettingsType::COOKIES);
  EXPECT_EQ(t1, last_modified);
  provider2.ShutdownOnUIThread();
}

// If a setting is constrained to a session scope it should only persist in
// memory.
TEST_F(PrefProviderTest, SessionScopeSettingsDontPersist) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));

  ContentSettingConstraints constraints;
  constraints.set_session_model(mojom::SessionModel::USER_SESSION);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  base::Value value = TestUtils::GetContentSettingValue(
      &provider, primary_url, primary_url, ContentSettingsType::STORAGE_ACCESS,
      false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  // Now if we create a new provider, it should not be able to read our setting
  // back.
  provider.ShutdownOnUIThread();

  PrefProvider provider2(testing_profile.GetPrefs(),
                         /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/false);
  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&provider2, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  provider2.ShutdownOnUIThread();
}

// If a setting is constrained to a session scope and a provider is made with
// the `restore_Session` flag, the setting should not be cleared.
TEST_F(PrefProviderTest, SessionScopeSettingsRestoreSession) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  PrefProvider::RegisterProfilePrefs(prefs.registry());

  // Create a normal provider and set a setting.
  PrefProvider provider(&prefs, /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));

  ContentSettingConstraints constraints;
  constraints.set_session_model(mojom::SessionModel::USER_SESSION);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  base::Value value(TestUtils::GetContentSettingValue(
      &provider, primary_url, primary_url, ContentSettingsType::STORAGE_ACCESS,
      false));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  // Now if we create a new provider, it should be able to read our setting
  // back.
  provider.ShutdownOnUIThread();

  PrefProvider provider2(&prefs, /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/true);

  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider2, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  provider2.ShutdownOnUIThread();
}

// If a setting is constrained to a non-restorable session scope and a provider
// is made with the `restore_Session` flag, the setting should be restored.
// TODO(b/344678400): Non-restorable grants are temporarily restored as part of
// b/338367663 to migrate them to DURABLE SessionModel. This test needs to be
// deleted once NON_RESTORABLE_USER_SESSION is removed.
TEST_F(PrefProviderTest, SessionScopeSettingsRestoreSessionNonRestorable) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));

  ContentSettingConstraints constraints;
  constraints.set_session_model(
      mojom::SessionModel::NON_RESTORABLE_USER_SESSION);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  base::Value value(TestUtils::GetContentSettingValue(
      &provider, primary_url, primary_url, ContentSettingsType::STORAGE_ACCESS,
      false));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  // Now if we create a new provider, it should be able to read our setting
  // back.
  provider.ShutdownOnUIThread();

  PrefProvider provider2(testing_profile.GetPrefs(), /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/true);

  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider2, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  provider2.ShutdownOnUIThread();
}

// Validate our settings will properly store our expiry time if specified.
TEST_F(PrefProviderTest, GetContentSettingsExpiry) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Seconds(123));
  constraints.set_session_model(mojom::SessionModel::DURABLE);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  base::Value value = TestUtils::GetContentSettingValue(
      &provider, primary_url, primary_url, ContentSettingsType::STORAGE_ACCESS,
      false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  // Now if we skip ahead our time our setting should be expired and no longer
  // valid.
  FastForwardTime(base::Seconds(200));

  EXPECT_EQ(base::Value(), TestUtils::GetContentSettingValue(
                               &provider, primary_url, primary_url,
                               ContentSettingsType::STORAGE_ACCESS, false));
  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  provider.ShutdownOnUIThread();
}

// Any specified expiry time should persist in our prefs and outlive a restart.
TEST_F(PrefProviderTest, GetContentSettingsExpiryPersists) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Seconds(123));
  constraints.set_session_model(mojom::SessionModel::DURABLE);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  base::Value value = TestUtils::GetContentSettingValue(
      &provider, primary_url, primary_url, ContentSettingsType::STORAGE_ACCESS,
      false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  // Shutdown our provider and we should still have a setting present.
  provider.ShutdownOnUIThread();
  PrefProvider provider2(testing_profile.GetPrefs(), /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/false);

  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider2, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));

  // Now if we skip ahead our time our setting should be expired and no longer
  // valid.
  FastForwardTime(base::Seconds(200));

  EXPECT_EQ(base::Value(), TestUtils::GetContentSettingValue(
                               &provider2, primary_url, primary_url,
                               ContentSettingsType::STORAGE_ACCESS, false));
  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&provider2, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  provider2.ShutdownOnUIThread();
}

// Any specified expiry time should persist in our prefs and outlive a restart.
TEST_F(PrefProviderTest, GetContentSettingsExpiryAfterRestore) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Seconds(123));
  constraints.set_session_model(mojom::SessionModel::DURABLE);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  base::Value value = TestUtils::GetContentSettingValue(
      &provider, primary_url, primary_url, ContentSettingsType::STORAGE_ACCESS,
      false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  provider.ShutdownOnUIThread();
  PrefProvider provider2(testing_profile.GetPrefs(), /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/true);

  // Now if we skip ahead our time our setting should be expired and no longer
  // valid.
  FastForwardTime(base::Seconds(200));

  EXPECT_EQ(base::Value(), TestUtils::GetContentSettingValue(
                               &provider2, primary_url, primary_url,
                               ContentSettingsType::STORAGE_ACCESS, false));
  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&provider2, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  provider2.ShutdownOnUIThread();
}

// If we update a setting and change the scope from Session to Durable it
// should persist in the same way as an original Durable scoped setting.
TEST_F(PrefProviderTest, ScopeSessionToDurablePersists) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingConstraints constraints;
  constraints.set_session_model(mojom::SessionModel::USER_SESSION);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));

  // Update to Durable and expect that the setting is still there.
  constraints.set_session_model(mojom::SessionModel::DURABLE);
  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));

  // Shutdown our provider and we should still have a setting present.
  provider.ShutdownOnUIThread();
  PrefProvider provider2(testing_profile.GetPrefs(), /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/false);

  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider2, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  provider2.ShutdownOnUIThread();
}

// If we update a setting and change the scope from Durable to Session it
// should drop in the same way as an original Session scoped setting would.
TEST_F(PrefProviderTest, ScopeDurableToSessionDrops) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingConstraints constraints;
  constraints.set_session_model(mojom::SessionModel::DURABLE);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));

  // Update to Durable and expect that the setting is still there.
  constraints.set_session_model(mojom::SessionModel::USER_SESSION);
  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_BLOCK), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));

  // Shutdown our provider and we should still have a setting present.
  provider.ShutdownOnUIThread();
  PrefProvider provider2(testing_profile.GetPrefs(), /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/false);

  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      TestUtils::GetContentSetting(&provider2, primary_url, primary_url,
                                   ContentSettingsType::STORAGE_ACCESS, false));
  provider2.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, LastVisitedTimeIsTracked) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  provider.SetClockForTesting(&clock);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  ContentSettingConstraints constraints;
  constraints.set_track_last_visit_for_autoexpiration(false);

  // Set one setting with track_last_visit_for_autoexpiration enabled and one
  // disabled.
  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::MEDIASTREAM_CAMERA,
      base::Value(CONTENT_SETTING_ALLOW), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());

  constraints.set_track_last_visit_for_autoexpiration(true);
  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::GEOLOCATION,
      base::Value(CONTENT_SETTING_ALLOW), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  RuleMetaData metadata;
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                &provider, primary_url, primary_url,
                ContentSettingsType::MEDIASTREAM_CAMERA, false, &metadata));
  EXPECT_EQ(metadata.last_visited(), base::Time());

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         false, &metadata));
  EXPECT_NE(metadata.last_visited(), base::Time());
  EXPECT_GE(metadata.last_visited(), clock.Now() - base::Days(7));
  EXPECT_LE(metadata.last_visited(), clock.Now());

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, RenewContentSetting) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  provider.SetClockForTesting(&clock);

  GURL primary_url("https://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("https://[*.]example.com");

  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Days(2));

  ASSERT_TRUE(provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::STORAGE_ACCESS,
      base::Value(CONTENT_SETTING_ALLOW), constraints,
      content_settings::PartitionKey::GetDefaultForTesting()));

  RuleMetaData metadata;
  EXPECT_EQ(CONTENT_SETTING_ALLOW, TestUtils::GetContentSetting(
                                       &provider, primary_url, primary_url,
                                       ContentSettingsType::STORAGE_ACCESS,
                                       /*include_incognito=*/false, &metadata));
  EXPECT_EQ(metadata.lifetime(), base::Days(2));
  EXPECT_EQ(metadata.expiration(), clock.Now() + base::Days(2));

  clock.Advance(base::Days(1));

  EXPECT_EQ(CONTENT_SETTING_ALLOW, TestUtils::GetContentSetting(
                                       &provider, primary_url, primary_url,
                                       ContentSettingsType::STORAGE_ACCESS,
                                       /*include_incognito=*/false, &metadata));
  EXPECT_EQ(metadata.lifetime(), base::Days(2));
  EXPECT_EQ(metadata.expiration(), clock.Now() + base::Days(1));

  // Wrong ContentSetting, doesn't match.
  EXPECT_FALSE(provider.RenewContentSetting(
      primary_url, primary_url, ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_BLOCK, PartitionKey::GetDefaultForTesting()));

  EXPECT_TRUE(provider.RenewContentSetting(
      primary_url, primary_url, ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW, PartitionKey::GetDefaultForTesting()));

  EXPECT_EQ(CONTENT_SETTING_ALLOW, TestUtils::GetContentSetting(
                                       &provider, primary_url, primary_url,
                                       ContentSettingsType::STORAGE_ACCESS,
                                       /*include_incognito=*/false, &metadata));
  EXPECT_EQ(metadata.lifetime(), base::Days(2));
  EXPECT_EQ(metadata.expiration(), clock.Now() + base::Days(2));

  provider.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, LastVisitedTimeStoredOnDisk) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);
  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingConstraints constraints;
  constraints.set_track_last_visit_for_autoexpiration(true);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::GEOLOCATION,
      base::Value(CONTENT_SETTING_ALLOW), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  RuleMetaData metadata;
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         false, &metadata));
  EXPECT_NE(metadata.last_visited(), base::Time());

  // Shutdown our provider and we should still have a setting present.
  provider.ShutdownOnUIThread();
  PrefProvider provider2(testing_profile.GetPrefs(), /*off_the_record=*/false,
                         /*store_last_modified=*/true,
                         /*restore_session=*/false);

  RuleMetaData metadata_from_disk;
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         false, &metadata_from_disk));
  EXPECT_EQ(metadata.last_visited(), metadata_from_disk.last_visited());

  provider2.ShutdownOnUIThread();
}

TEST_F(PrefProviderTest, LastVisitedTimeUpdating) {
  TestingProfile testing_profile;
  PrefProvider provider(testing_profile.GetPrefs(), /*off_the_record=*/false,
                        /*store_last_modified=*/true,
                        /*restore_session=*/false);
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  provider.SetClockForTesting(&clock);

  GURL primary_url("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingConstraints constraints;
  constraints.set_track_last_visit_for_autoexpiration(true);

  provider.SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::GEOLOCATION,
      base::Value(CONTENT_SETTING_ALLOW), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());
  RuleMetaData metadata;
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         false, &metadata));
  EXPECT_GE(metadata.last_visited(), clock.Now() - base::Days(7));
  EXPECT_LE(metadata.last_visited(), clock.Now());

  clock.Advance(base::Days(20));
  provider.UpdateLastVisitTime(primary_pattern, primary_pattern,
                               ContentSettingsType::GEOLOCATION,
                               PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         false, &metadata));
  EXPECT_GE(metadata.last_visited(), clock.Now() - base::Days(7));
  EXPECT_LE(metadata.last_visited(), clock.Now());

  // Test resetting the last_visited time.
  provider.ResetLastVisitTime(primary_pattern, primary_pattern,
                              ContentSettingsType::GEOLOCATION,
                              PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider, primary_url, primary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         false, &metadata));
  EXPECT_EQ(metadata.last_visited(), base::Time());
  provider.ShutdownOnUIThread();
}

}  // namespace content_settings
