// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

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
  auto url_pref_list = std::make_unique<base::ListValue>();
  url_pref_list->Set(0, std::make_unique<base::Value>("google.com"));
  url_pref_list->Set(1, std::make_unique<base::Value>("chromium.org"));
  pref_service_->SetUserPref(prefs::kURLsToRestoreOnStartup,
                             std::move(url_pref_list));

  SessionStartupPref result =
      SessionStartupPref::GetStartupPref(pref_service_.get());
  EXPECT_EQ(2u, result.urls.size());
  EXPECT_EQ("http://google.com/", result.urls[0].spec());
  EXPECT_EQ("http://chromium.org/", result.urls[1].spec());
}

TEST_F(SessionStartupPrefTest, URLListManagedOverridesUser) {
  auto url_pref_list1 = std::make_unique<base::ListValue>();
  url_pref_list1->Set(0, std::make_unique<base::Value>("chromium.org"));
  pref_service_->SetUserPref(prefs::kURLsToRestoreOnStartup,
                             std::move(url_pref_list1));

  auto url_pref_list2 = std::make_unique<base::ListValue>();
  url_pref_list2->Set(0, std::make_unique<base::Value>("chromium.org"));
  url_pref_list2->Set(1, std::make_unique<base::Value>("chromium.org"));
  url_pref_list2->Set(2, std::make_unique<base::Value>("chromium.org"));
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
