// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"

#include <memory>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

// Unit tests for SessionStartupPref.
class SessionStartupPrefTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    SessionStartupPref::RegisterProfilePrefs(registry());
    registry()->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, true);
  }

  user_prefs::PrefRegistrySyncable* registry() {
    return pref_service_->registry();
  }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
};

TEST_F(SessionStartupPrefTest, DefaultStartupType) {
  SessionStartupPref result =
      SessionStartupPref::GetStartupPref(pref_service_.get());
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(), result.type);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(SessionStartupPref::LAST,
            SessionStartupPref::GetDefaultStartupType());
#elif BUILDFLAG(IS_ANDROID)
  base::android::device_info::set_is_desktop_for_testing(true);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        chrome::android::kSyncRestoreOnStartupPref);
    EXPECT_EQ(SessionStartupPref::LAST,
              SessionStartupPref::GetDefaultStartupType());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        chrome::android::kSyncRestoreOnStartupPref);
    EXPECT_EQ(SessionStartupPref::DEFAULT,
              SessionStartupPref::GetDefaultStartupType());
  }

  base::android::device_info::set_is_desktop_for_testing(false);
  EXPECT_EQ(SessionStartupPref::DEFAULT,
            SessionStartupPref::GetDefaultStartupType());

  base::android::device_info::reset_is_desktop_for_testing();
#else
  EXPECT_EQ(SessionStartupPref::DEFAULT,
            SessionStartupPref::GetDefaultStartupType());
#endif
}

TEST_F(SessionStartupPrefTest, URLListIsFixedUp) {
  base::ListValue url_pref_list;
  url_pref_list.Append("google.com");
  url_pref_list.Append("chromium.org");
  pref_service_->SetUserPref(prefs::kURLsToRestoreOnStartup,
                             std::move(url_pref_list));

  SessionStartupPref result =
      SessionStartupPref::GetStartupPref(pref_service_.get());
  EXPECT_EQ(2u, result.urls.size());
  EXPECT_EQ("http://google.com/", result.urls[0].spec());
  EXPECT_EQ("http://chromium.org/", result.urls[1].spec());
}

TEST_F(SessionStartupPrefTest, URLListManagedOverridesUser) {
  base::ListValue url_pref_list1;
  url_pref_list1.Append("chromium.org");
  pref_service_->SetUserPref(prefs::kURLsToRestoreOnStartup,
                             std::move(url_pref_list1));

  base::ListValue url_pref_list2;
  url_pref_list2.Append("chromium.org");
  url_pref_list2.Append("chromium.org");
  url_pref_list2.Append("chromium.org");
  pref_service_->SetManagedPref(prefs::kURLsToRestoreOnStartup,
                                std::move(url_pref_list2));

  SessionStartupPref result =
      SessionStartupPref::GetStartupPref(pref_service_.get());
  EXPECT_EQ(3u, result.urls.size());

  SessionStartupPref override_test =
      SessionStartupPref(SessionStartupPref::URLS);
  override_test.urls.push_back(GURL("dev.chromium.org"));
  SessionStartupPref::SetStartupPref(pref_service_.get(), override_test);

  result = SessionStartupPref::GetStartupPref(pref_service_.get());
  EXPECT_EQ(3u, result.urls.size());
}
