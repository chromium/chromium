// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"

#include <stdint.h>

#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/push_messaging/app_identifier_test_support.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

base::Time kExpirationTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(1));

}  // namespace

class PushMessagingAppIdentifierTest
    : public push_messaging::AppIdentifierTestSupport,
      public testing::Test {
 protected:
  Profile* profile() { return &profile_; }

  const push_messaging::AppIdentifier original_ =
      push_messaging::AppIdentifier::Generate(GURL("https://www.example.com/"),
                                              1);
  const push_messaging::AppIdentifier same_origin_and_sw_ =
      push_messaging::AppIdentifier::Generate(GURL("https://www.example.com"),
                                              1);
  const push_messaging::AppIdentifier different_origin_ =
      push_messaging::AppIdentifier::Generate(
          GURL("https://foobar.example.com/"),
          1);
  const push_messaging::AppIdentifier different_sw_ =
      push_messaging::AppIdentifier::Generate(GURL("https://www.example.com/"),
                                              42);
  const push_messaging::AppIdentifier different_et_ =
      push_messaging::AppIdentifier::Generate(
          GURL("https://www.example.com/"),
          1,
          kExpirationTime + base::Seconds(100));
  const push_messaging::AppIdentifier with_et_ =
      push_messaging::AppIdentifier::Generate(GURL("https://www.example.com/"),
                                              1,
                                              kExpirationTime);

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(PushMessagingAppIdentifierTest, FindInvalidAppId) {
  // These calls to FindByAppId should not DCHECK.
  EXPECT_TRUE(PushMessagingAppIdentifier::FindByAppId(profile(), "").is_null());
  EXPECT_TRUE(PushMessagingAppIdentifier::FindByAppId(
                  profile(), "amhfneadkjmnlefnpidcijoldiibcdnd")
                  .is_null());
}

TEST_F(PushMessagingAppIdentifierTest, PersistAndFind) {
  ASSERT_TRUE(
      PushMessagingAppIdentifier::FindByAppId(profile(), original_.app_id())
          .is_null());

  const auto identifier = PushMessagingAppIdentifier::FindByServiceWorker(
      profile(), original_.origin(),
      original_.service_worker_registration_id());

  ASSERT_TRUE(identifier.is_null());

  // Test basic PersistToPrefs round trips.
  PushMessagingAppIdentifier::PersistToPrefs(original_, profile());
  {
    push_messaging::AppIdentifier found_by_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(), original_.app_id());
    EXPECT_FALSE(found_by_app_id.is_null());
    ExpectAppIdentifiersEqual(original_, found_by_app_id);
  }
  {
    push_messaging::AppIdentifier found_by_origin_and_swr_id =
        PushMessagingAppIdentifier::FindByServiceWorker(
            profile(), original_.origin(),
            original_.service_worker_registration_id());
    EXPECT_FALSE(found_by_origin_and_swr_id.is_null());
    ExpectAppIdentifiersEqual(original_, found_by_origin_and_swr_id);
  }
}

TEST_F(PushMessagingAppIdentifierTest, FindLegacy) {
  const std::string legacy_app_id("wp:9CC55CCE-B8F9-4092-A364-3B0F73A3AB5F");
  ASSERT_TRUE(PushMessagingAppIdentifier::FindByAppId(profile(), legacy_app_id)
                  .is_null());

  const auto identifier = PushMessagingAppIdentifier::FindByServiceWorker(
      profile(), original_.origin(),
      original_.service_worker_registration_id());

  ASSERT_TRUE(identifier.is_null());

  // Create a legacy preferences entry (the test happens to use PersistToPrefs
  // since that currently works, but it's ok to change the behavior of
  // PersistToPrefs; if so, this test can just do a raw ScopedDictPrefUpdate).
  const auto legacy_original = ReplaceAppId(original_, legacy_app_id);
  PushMessagingAppIdentifier::PersistToPrefs(legacy_original, profile());

  // Test that legacy entries can be read back from prefs.
  {
    push_messaging::AppIdentifier found_by_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(),
                                                legacy_original.app_id());
    EXPECT_FALSE(found_by_app_id.is_null());
    ExpectAppIdentifiersEqual(legacy_original, found_by_app_id);
  }
  {
    push_messaging::AppIdentifier found_by_origin_and_swr_id =
        PushMessagingAppIdentifier::FindByServiceWorker(
            profile(), legacy_original.origin(),
            original_.service_worker_registration_id());
    EXPECT_FALSE(found_by_origin_and_swr_id.is_null());
    ExpectAppIdentifiersEqual(legacy_original, found_by_origin_and_swr_id);
  }
}

TEST_F(PushMessagingAppIdentifierTest, PersistOverwritesSameOriginAndSW) {
  PushMessagingAppIdentifier::PersistToPrefs(original_, profile());

  // Test that PersistToPrefs overwrites when same origin and Service Worker.
  ASSERT_NE(original_.app_id(), same_origin_and_sw_.app_id());
  ASSERT_EQ(original_.origin(), same_origin_and_sw_.origin());
  ASSERT_EQ(original_.service_worker_registration_id(),
            same_origin_and_sw_.service_worker_registration_id());
  PushMessagingAppIdentifier::PersistToPrefs(same_origin_and_sw_, profile());
  {
    push_messaging::AppIdentifier found_by_original_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(), original_.app_id());
    EXPECT_TRUE(found_by_original_app_id.is_null());
  }
  {
    push_messaging::AppIdentifier found_by_soas_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(),
                                                same_origin_and_sw_.app_id());
    EXPECT_FALSE(found_by_soas_app_id.is_null());
    ExpectAppIdentifiersEqual(same_origin_and_sw_, found_by_soas_app_id);
  }
  {
    push_messaging::AppIdentifier found_by_original_origin_and_swr_id =
        PushMessagingAppIdentifier::FindByServiceWorker(
            profile(), original_.origin(),
            original_.service_worker_registration_id());
    EXPECT_FALSE(found_by_original_origin_and_swr_id.is_null());
    ExpectAppIdentifiersEqual(same_origin_and_sw_,
                              found_by_original_origin_and_swr_id);
  }
}

TEST_F(PushMessagingAppIdentifierTest, PersistDoesNotOverwriteDifferent) {
  PushMessagingAppIdentifier::PersistToPrefs(original_, profile());

  // Test that PersistToPrefs doesn't overwrite when different origin or SW.
  ASSERT_NE(original_.app_id(), different_origin_.app_id());
  ASSERT_NE(original_.app_id(), different_sw_.app_id());
  PushMessagingAppIdentifier::PersistToPrefs(different_origin_, profile());
  PushMessagingAppIdentifier::PersistToPrefs(different_sw_, profile());
  {
    push_messaging::AppIdentifier found_by_original_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(), original_.app_id());
    EXPECT_FALSE(found_by_original_app_id.is_null());
    ExpectAppIdentifiersEqual(original_, found_by_original_app_id);
  }
  {
    push_messaging::AppIdentifier found_by_original_origin_and_swr_id =
        PushMessagingAppIdentifier::FindByServiceWorker(
            profile(), original_.origin(),
            original_.service_worker_registration_id());
    EXPECT_FALSE(found_by_original_origin_and_swr_id.is_null());
    ExpectAppIdentifiersEqual(original_, found_by_original_origin_and_swr_id);
  }
}

TEST_F(PushMessagingAppIdentifierTest, DeleteFromPrefs) {
  PushMessagingAppIdentifier::PersistToPrefs(original_, profile());
  PushMessagingAppIdentifier::PersistToPrefs(different_origin_, profile());
  PushMessagingAppIdentifier::PersistToPrefs(different_sw_, profile());

  // Test DeleteFromPrefs. Deleted app identifier should be deleted.
  PushMessagingAppIdentifier::DeleteFromPrefs(original_, profile());
  {
    push_messaging::AppIdentifier found_by_original_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(), original_.app_id());
    EXPECT_TRUE(found_by_original_app_id.is_null());
  }
  {
    push_messaging::AppIdentifier found_by_original_origin_and_swr_id =
        PushMessagingAppIdentifier::FindByServiceWorker(
            profile(), original_.origin(),
            original_.service_worker_registration_id());
    EXPECT_TRUE(found_by_original_origin_and_swr_id.is_null());
  }
}

TEST_F(PushMessagingAppIdentifierTest, GetAll) {
  PushMessagingAppIdentifier::PersistToPrefs(original_, profile());
  PushMessagingAppIdentifier::PersistToPrefs(different_origin_, profile());
  PushMessagingAppIdentifier::PersistToPrefs(different_sw_, profile());

  PushMessagingAppIdentifier::DeleteFromPrefs(original_, profile());

  // Test GetAll. Non-deleted app identifiers should all be listed.
  std::vector<push_messaging::AppIdentifier> all_app_identifiers =
      PushMessagingAppIdentifier::GetAll(profile());
  EXPECT_EQ(2u, all_app_identifiers.size());
  // Order is unspecified.
  bool contained_different_origin = false;
  bool contained_different_sw = false;
  for (const push_messaging::AppIdentifier& app_identifier :
       all_app_identifiers) {
    if (app_identifier.app_id() == different_origin_.app_id()) {
      ExpectAppIdentifiersEqual(different_origin_, app_identifier);
      contained_different_origin = true;
    } else {
      ExpectAppIdentifiersEqual(different_sw_, app_identifier);
      contained_different_sw = true;
    }
  }
  EXPECT_TRUE(contained_different_origin);
  EXPECT_TRUE(contained_different_sw);
}

TEST_F(PushMessagingAppIdentifierTest, PersistWithExpirationTime) {
  ASSERT_TRUE(with_et_.expiration_time());
  ASSERT_TRUE(different_et_.expiration_time());
  ASSERT_EQ(with_et_.origin(), different_et_.origin());
  ASSERT_EQ(with_et_.service_worker_registration_id(),
            different_et_.service_worker_registration_id());
  ASSERT_FALSE(kExpirationTime.is_null());

  PushMessagingAppIdentifier::PersistToPrefs(different_et_, profile());

  // Test PersistToPrefs and FindByAppId, whether expiration time is saved
  // properly
  std::vector<push_messaging::AppIdentifier> all_app_identifiers =
      PushMessagingAppIdentifier::GetAll(profile());
  EXPECT_EQ(1u, all_app_identifiers.size());
  {
    push_messaging::AppIdentifier found_by_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(),
                                                different_et_.app_id());
    // Check whether expiration time was saved
    ExpectAppIdentifiersEqual(found_by_app_id, different_et_);
  }
  PushMessagingAppIdentifier::PersistToPrefs(with_et_, profile());
  {
    all_app_identifiers = PushMessagingAppIdentifier::GetAll(profile());
    EXPECT_EQ(1u, all_app_identifiers.size());
  }
  {
    push_messaging::AppIdentifier found_by_with_et_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(), with_et_.app_id());
    EXPECT_FALSE(found_by_with_et_app_id.is_null());
    EXPECT_EQ(found_by_with_et_app_id.expiration_time(), kExpirationTime);
    ExpectAppIdentifiersEqual(found_by_with_et_app_id, with_et_);
  }
  {
    push_messaging::AppIdentifier found_by_different_et_app_id =
        PushMessagingAppIdentifier::FindByAppId(profile(),
                                                different_et_.app_id());
    EXPECT_TRUE(found_by_different_et_app_id.is_null());
  }
}
