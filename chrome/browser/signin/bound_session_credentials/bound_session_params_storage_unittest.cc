// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"

#include <optional>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace {

const char kBoundSessionParamsPref[] =
    "bound_session_credentials_bound_session_params";

bound_session_credentials::BoundSessionParams CreateValidBoundSessionParams() {
  bound_session_credentials::BoundSessionParams params;
  params.set_session_id("123");
  params.set_site("https://google.com/");
  params.set_wrapped_key("456");
  params.set_refresh_url("https://google.com/rotate/");

  bound_session_credentials::CookieCredential* cookie =
      params.add_credentials()->mutable_cookie_credential();
  cookie->set_name("auth_cookie");
  cookie->set_domain(".google.com");
  cookie->set_path("/");
  return params;
}

void UpdateAllCookieCredentialsDomains(
    bound_session_credentials::BoundSessionParams& params,
    const std::string& domain) {
  for (auto& credential : *params.mutable_credentials()) {
    credential.mutable_cookie_credential()->set_domain(domain);
  }
}

void PopulateSameSiteParams(
    const std::string& site,
    std::vector<bound_session_credentials::BoundSessionParams>& params) {
  GURL site_url(site);
  for (size_t i = 0; i < 3; ++i) {
    bound_session_credentials::BoundSessionParams same_site_params =
        CreateValidBoundSessionParams();
    same_site_params.set_site(site_url.spec());
    same_site_params.set_session_id(
        base::StrCat({"session_", base::NumberToString(i)}));
    UpdateAllCookieCredentialsDomains(same_site_params, site_url.host());
    params.push_back(std::move(same_site_params));
  }
}

void PopulateSameSessionIdParams(
    const std::string& session_id,
    std::vector<bound_session_credentials::BoundSessionParams>& params) {
  for (size_t i = 0; i < 3; ++i) {
    bound_session_credentials::BoundSessionParams same_session_id_params =
        CreateValidBoundSessionParams();
    same_session_id_params.set_session_id(session_id);
    GURL site(base::StrCat(
        {"https://domain", base::NumberToString(i), ".google.com/"}));
    same_session_id_params.set_site(site.spec());
    UpdateAllCookieCredentialsDomains(same_session_id_params, site.host());
    params.push_back(std::move(same_session_id_params));
  }
}

bound_session_credentials::BoundSessionParams
CreateInvalidBoundSessionParams() {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  // Removes a required `wrapped_key` field.
  params.clear_wrapped_key();
  return params;
}

}  // namespace

class BoundSessionParamsStorageTest : public testing::TestWithParam<bool> {
 public:
  BoundSessionParamsStorageTest() : storage_(CreateStorage()) {}

  bool IsOffTheRecord() { return GetParam(); }

  BoundSessionParamsStorage& storage() { return *storage_; }

  void ResetStorage() { storage_ = CreateStorage(); }

 private:
  std::unique_ptr<BoundSessionParamsStorage> CreateStorage() {
    return BoundSessionParamsStorage::CreateForProfile(
        IsOffTheRecord()
            ? *profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)
            : profile_);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BoundSessionParamsStorage> storage_;
};

TEST_P(BoundSessionParamsStorageTest, InitiallyEmpty) {
  EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
              testing::IsEmpty());
}

TEST_P(BoundSessionParamsStorageTest, SaveAndRead) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(params));
  EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
              testing::Pointwise(base::test::EqualsProto(), {params}));
}

TEST_P(BoundSessionParamsStorageTest, SaveInvalidParams) {
  EXPECT_FALSE(storage().SaveParams(CreateInvalidBoundSessionParams()));
  EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
              testing::IsEmpty());
}

TEST_P(BoundSessionParamsStorageTest, OverwriteWithValidParams) {
  ASSERT_TRUE(storage().SaveParams(CreateValidBoundSessionParams()));
  bound_session_credentials::BoundSessionParams new_params =
      CreateValidBoundSessionParams();
  new_params.set_wrapped_key("new_wrapped_key");
  EXPECT_TRUE(storage().SaveParams(new_params));
  EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
              testing::Pointwise(base::test::EqualsProto(), {new_params}));
}

TEST_P(BoundSessionParamsStorageTest, OverwriteWithInvalidParams) {
  bound_session_credentials::BoundSessionParams valid_params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(valid_params));
  EXPECT_FALSE(storage().SaveParams(CreateInvalidBoundSessionParams()));
  EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
              testing::Pointwise(base::test::EqualsProto(), {valid_params}));
}

TEST_P(BoundSessionParamsStorageTest, SaveMultipleParamsSameSite) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params;
  PopulateSameSiteParams("https://google.com/", all_params);
  for (const auto& params : all_params) {
    EXPECT_TRUE(storage().SaveParams(params));
  }
  EXPECT_THAT(
      storage().ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), all_params));
}

TEST_P(BoundSessionParamsStorageTest, SaveMultipleParamsDifferentSites) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params;
  PopulateSameSessionIdParams("123", all_params);
  for (const auto& params : all_params) {
    EXPECT_TRUE(storage().SaveParams(params));
  }
  EXPECT_THAT(
      storage().ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), all_params));
}

TEST_P(BoundSessionParamsStorageTest, Clear) {
  const std::string kSite = "https://mydomain.google.com/";
  const std::string kSessionId = "my_session";
  bound_session_credentials::BoundSessionParams params_to_be_removed =
      CreateValidBoundSessionParams();
  params_to_be_removed.set_site(kSite);
  params_to_be_removed.set_session_id(kSessionId);
  UpdateAllCookieCredentialsDomains(params_to_be_removed,
                                    "mydomain.google.com");

  // Populate storage with params matching by either a site or a session_id.
  std::vector<bound_session_credentials::BoundSessionParams> expected_params;
  PopulateSameSiteParams(kSite, expected_params);
  PopulateSameSessionIdParams(kSessionId, expected_params);
  for (const auto& params : expected_params) {
    ASSERT_TRUE(storage().SaveParams(params));
  }
  // Add `params_to_be_removed` last to easily `pop_back()` later.
  ASSERT_TRUE(storage().SaveParams(params_to_be_removed));
  expected_params.push_back(params_to_be_removed);
  // Verify that the storage is populated as expected.
  ASSERT_THAT(
      storage().ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), expected_params));

  EXPECT_TRUE(storage().ClearParams(GURL(params_to_be_removed.site()),
                                    params_to_be_removed.session_id()));
  // Removes `params_to_be_removed`.
  expected_params.pop_back();

  EXPECT_THAT(
      storage().ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), expected_params));
}

TEST_P(BoundSessionParamsStorageTest, ClearNonExisting) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  params.set_session_id("my_session_id");
  ASSERT_TRUE(storage().SaveParams(params));
  bound_session_credentials::BoundSessionParams other_params = params;
  other_params.set_session_id("other_session_id");

  EXPECT_FALSE(storage().ClearParams(GURL(other_params.site()),
                                     other_params.session_id()));

  EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
              testing::Pointwise(base::test::EqualsProto(), {params}));
}

TEST_P(BoundSessionParamsStorageTest, ClearAll) {
  ASSERT_TRUE(storage().SaveParams(CreateValidBoundSessionParams()));
  storage().ClearAllParams();
  EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
              testing::IsEmpty());
}

TEST_P(BoundSessionParamsStorageTest, Persistence) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(params));
  EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
              testing::Not(testing::IsEmpty()));

  ResetStorage();

  if (IsOffTheRecord()) {
    EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
                testing::IsEmpty());
  } else {
    EXPECT_THAT(storage().ReadAllParamsAndCleanStorageIfNecessary(),
                testing::Pointwise(base::test::EqualsProto(), {params}));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         BoundSessionParamsStorageTest,
                         testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "OTR" : "Persistent";
                         });

class BoundSessionParamsStorageOTRTest : public testing::Test {
 public:
  TestingProfile& parent_profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Tests that an OTR profile storage isn't affected by the contents of the
// parent storage.
TEST_F(BoundSessionParamsStorageOTRTest, NoInheritance) {
  std::unique_ptr<BoundSessionParamsStorage> parent_storage =
      BoundSessionParamsStorage::CreateForProfile(parent_profile());
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(parent_storage->SaveParams(params));
  EXPECT_THAT(parent_storage->ReadAllParamsAndCleanStorageIfNecessary(),
              testing::Not(testing::IsEmpty()));

  std::unique_ptr<BoundSessionParamsStorage> otr_storage =
      BoundSessionParamsStorage::CreateForProfile(
          *parent_profile().GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_THAT(otr_storage->ReadAllParamsAndCleanStorageIfNecessary(),
              testing::IsEmpty());
  bound_session_credentials::BoundSessionParams params2 =
      CreateValidBoundSessionParams();
  params2.set_session_id("otr_session");
  ASSERT_TRUE(otr_storage->SaveParams(params2));
  EXPECT_THAT(otr_storage->ReadAllParamsAndCleanStorageIfNecessary(),
              testing::Pointwise(base::test::EqualsProto(), {params2}));

  // Parent storage hasn't changed.
  EXPECT_THAT(parent_storage->ReadAllParamsAndCleanStorageIfNecessary(),
              testing::Pointwise(base::test::EqualsProto(), {params}));
}

class BoundSessionParamsPrefsStorageTest : public testing::Test {
 public:
  BoundSessionParamsPrefsStorageTest() {
    BoundSessionParamsStorage::RegisterProfilePrefs(prefs_.registry());
    prefs_observer_.Init(&prefs_);
  }

  TestingPrefServiceSimple& prefs() { return prefs_; }

  PrefChangeRegistrar& prefs_observer() { return prefs_observer_; }

 private:
  TestingPrefServiceSimple prefs_;
  PrefChangeRegistrar prefs_observer_;
};

TEST_F(BoundSessionParamsPrefsStorageTest, CanonicalizeSiteInStorage) {
  std::unique_ptr<BoundSessionParamsStorage> storage =
      BoundSessionParamsStorage::CreatePrefsStorageForTesting(prefs());

  // Insert an old version entry into storage. Cannot use `SaveParams()` here
  // because it rejects invalid params.
  bound_session_credentials::BoundSessionParams prev_version_params =
      CreateValidBoundSessionParams();
  // No trailing "/".
  prev_version_params.set_site("https://google.com");
  {
    ScopedDictPrefUpdate root(&prefs(), kBoundSessionParamsPref);
    root->EnsureDict(prev_version_params.site())
        ->Set(prev_version_params.session_id(),
              base::Base64Encode(prev_version_params.SerializeAsString()));
  }

  bound_session_credentials::BoundSessionParams fixed_params =
      prev_version_params;
  fixed_params.set_site("https://google.com/");

  testing::StrictMock<base::MockCallback<base::RepeatingClosure>>
      pref_changed_callback;
  prefs_observer().Add(kBoundSessionParamsPref, pref_changed_callback.Get());

  // Site should be canonicalized both in the result and in the storage.
  EXPECT_CALL(pref_changed_callback, Run);
  ASSERT_THAT(
      storage->ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), {fixed_params}));

  testing::Mock::VerifyAndClearExpectations(&pref_changed_callback);

  // Clean-up update shouldn't be needed afterwards.
  EXPECT_CALL(pref_changed_callback, Run).Times(0);
  ASSERT_THAT(
      storage->ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), {fixed_params}));
}

TEST_F(BoundSessionParamsPrefsStorageTest, PopulateRefreshUrl) {
  std::unique_ptr<BoundSessionParamsStorage> storage =
      BoundSessionParamsStorage::CreatePrefsStorageForTesting(prefs());

  // Insert an old version entry into storage. Cannot use `SaveParams()` here
  // because it rejects invalid params.
  bound_session_credentials::BoundSessionParams prev_version_params =
      CreateValidBoundSessionParams();
  prev_version_params.clear_refresh_url();
  {
    ScopedDictPrefUpdate root(&prefs(), kBoundSessionParamsPref);
    root->EnsureDict(prev_version_params.site())
        ->Set(prev_version_params.session_id(),
              base::Base64Encode(prev_version_params.SerializeAsString()));
  }

  bound_session_credentials::BoundSessionParams fixed_params =
      prev_version_params;
  const std::string default_refresh_url =
      "https://accounts.google.com/RotateBoundCookies";
  fixed_params.set_refresh_url(default_refresh_url);

  testing::StrictMock<base::MockCallback<base::RepeatingClosure>>
      pref_changed_callback;
  prefs_observer().Add(kBoundSessionParamsPref, pref_changed_callback.Get());

  // Refresh URL should be populated both in the result and in the storage.
  EXPECT_CALL(pref_changed_callback, Run);
  ASSERT_THAT(
      storage->ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), {fixed_params}));

  testing::Mock::VerifyAndClearExpectations(&pref_changed_callback);

  // Clean-up update shouldn't be needed afterwards.
  EXPECT_CALL(pref_changed_callback, Run).Times(0);
  ASSERT_THAT(
      storage->ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), {fixed_params}));
}

TEST_F(BoundSessionParamsPrefsStorageTest, CleanUpInvalidEntries) {
  std::unique_ptr<BoundSessionParamsStorage> storage =
      BoundSessionParamsStorage::CreatePrefsStorageForTesting(prefs());

  const std::string kSite = "https://google.com/";
  const std::string kSessionId = "my_session";

  // Populate storage with valid params to make sure that they aren't removed.
  std::vector<bound_session_credentials::BoundSessionParams> expected_params;
  PopulateSameSiteParams(kSite, expected_params);
  PopulateSameSessionIdParams(kSessionId, expected_params);
  for (const auto& params : expected_params) {
    ASSERT_TRUE(storage->SaveParams(params));
  }

  // Insert an invalid entry into storage. Cannot use `SaveParams()` here
  // because it rejects invalid params.
  bound_session_credentials::BoundSessionParams invalid_params =
      CreateInvalidBoundSessionParams();
  invalid_params.set_site(kSite);
  invalid_params.set_session_id(kSessionId);
  {
    ScopedDictPrefUpdate root(&prefs(), kBoundSessionParamsPref);
    root->EnsureDict(invalid_params.site())
        ->Set(invalid_params.session_id(),
              base::Base64Encode(invalid_params.SerializeAsString()));
  }

  testing::StrictMock<base::MockCallback<base::RepeatingClosure>>
      pref_changed_callback;
  prefs_observer().Add(kBoundSessionParamsPref, pref_changed_callback.Get());

  // Invalid params shouldn't be added to the result and the storage pref should
  // be updated to remove an invalid entry.
  EXPECT_CALL(pref_changed_callback, Run);
  ASSERT_THAT(
      storage->ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), expected_params));

  testing::Mock::VerifyAndClearExpectations(&pref_changed_callback);

  // Clean-up update shouldn't be needed afterwards.
  EXPECT_CALL(pref_changed_callback, Run).Times(0);
  ASSERT_THAT(
      storage->ReadAllParamsAndCleanStorageIfNecessary(),
      testing::UnorderedPointwise(base::test::EqualsProto(), expected_params));
}
