// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

TEST_F(SessionStartupPrefTest, URLListIsFixedUp) {
  base::Value::List url_pref_list;
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
  base::Value::List url_pref_list1;
  url_pref_list1.Append("chromium.org");
  pref_service_->SetUserPref(prefs::kURLsToRestoreOnStartup,
                             std::move(url_pref_list1));

  base::Value::List url_pref_list2;
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
