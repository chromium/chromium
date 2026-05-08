// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_duplicate_permission_cleaner.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_permission_cleaning_service.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_permission_cleaning_service_impl.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_permission_service.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor_login {

namespace {
using testing::_;
using testing::AllOf;
using testing::Each;
using testing::Eq;
using testing::Field;
using testing::Not;
using testing::UnorderedElementsAre;
}  // namespace

class ActorLoginDuplicatePermissionCleanerTest : public testing::Test {
 public:
  ActorLoginDuplicatePermissionCleanerTest() {
    feature_list_.InitAndEnableFeature(features::kFedCmEmbedderInitiatedLogin);
    auto match_helper =
        std::make_unique<password_manager::AffiliatedMatchHelper>(
            &mock_affiliation_service_);
    match_helper_ = match_helper.get();
    store_->Init(std::move(match_helper));
    service_ = std::make_unique<ActorLoginPermissionCleaningServiceImpl>(
        &permission_service_, store_.get(), nullptr);

    ON_CALL(mock_affiliation_service(), GetAffiliationsAndBranding)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
            affiliations::AffiliatedFacets(), true));
    ON_CALL(mock_affiliation_service(), GetGroupingInfo)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
            std::vector<affiliations::GroupedFacets>(1)));
    ON_CALL(mock_affiliation_service(), GetPSLExtensions)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<0>(
            std::vector<std::string>()));
  }

  ~ActorLoginDuplicatePermissionCleanerTest() override {
    match_helper_ = nullptr;
    store_->ShutdownOnUIThread();
  }

  affiliations::MockAffiliationService& mock_affiliation_service() {
    return mock_affiliation_service_;
  }
  password_manager::AffiliatedMatchHelper* match_helper() {
    return match_helper_;
  }
  MockActorLoginPermissionService* permission_service() {
    return &permission_service_;
  }

  password_manager::TestPasswordStore* store() { return store_.get(); }
  ActorLoginPermissionCleaningService* cleaning_service() {
    return service_.get();
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<affiliations::MockAffiliationService>
      mock_affiliation_service_;
  raw_ptr<password_manager::AffiliatedMatchHelper> match_helper_ = nullptr;
  testing::NiceMock<MockActorLoginPermissionService> permission_service_;
  scoped_refptr<password_manager::TestPasswordStore> store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  std::unique_ptr<ActorLoginPermissionCleaningService> service_;
};

// Tests that when a new permission is saved for a password-based credential
// the other existing permissions are deleted, but the newly-granted one and
// the one corresponding to the matching federated credentials are preserved.
TEST_F(ActorLoginDuplicatePermissionCleanerTest,
       KeepsExcludedPasswordPermissionAndItsFederatedMatchIfPasswordSaved) {
  base::HistogramTester histogram_tester;
  const GURL kUrl("https://example.com/login");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  const std::u16string kExcludeUser = u"user1@gmail.com";
  const std::string kSignonRealm = kOrigin.GetURL().spec();

  password_manager::PasswordForm form1;
  form1.url = kUrl;
  form1.signon_realm = kSignonRealm;
  form1.username_value = kExcludeUser;
  form1.password_value = u"pass1";
  form1.actor_login_approved = true;
  form1.match_type = password_manager::PasswordForm::MatchType::kExact;
  store()->AddLogin(password_manager::FromPasswordForm(form1));

  password_manager::PasswordForm form2;
  form2.url = kUrl;
  form2.signon_realm = kSignonRealm;
  form2.username_value = u"user2";
  form2.password_value = u"pass2";
  form2.actor_login_approved = true;
  form2.match_type = password_manager::PasswordForm::MatchType::kExact;
  store()->AddLogin(password_manager::FromPasswordForm(form2));

  // Wait for store to add logins.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetAllLoginsSync(store()).count(kSignonRealm) > 0 &&
           GetAllLoginsSync(store()).at(kSignonRealm).size() == 2;
  }));

  EXPECT_CALL(*permission_service(), ListPermissions(kOrigin, _))
      .WillOnce([&](const url::Origin& origin, ListPermissionsResult callback) {
        std::vector<FederatedPermission> perms;
        FederatedPermission p1;
        p1.chosen_account_email = base::UTF16ToUTF8(kExcludeUser);
        p1.chosen_account_id = "old_id";
        p1.rp_embedder_origin = kOrigin;
        perms.push_back(p1);
        FederatedPermission p2;
        p2.chosen_account_email = "new@example.com";
        p2.chosen_account_id = "new_id";
        p2.rp_embedder_origin = kOrigin;
        perms.push_back(p2);
        std::move(callback).Run(std::move(perms));
      });

  Credential credential;
  credential.request_origin = kOrigin;
  credential.username = kExcludeUser;
  credential.type = CredentialType::kPassword;
  credential.signon_realm = kSignonRealm;

  EXPECT_CALL(*permission_service(),
              DeletePermission(kOrigin, Eq(base::UTF16ToUTF8(kExcludeUser)), _))
      .Times(0);
  EXPECT_CALL(*permission_service(),
              DeletePermission(kOrigin, "new@example.com", _))
      .WillOnce([](const url::Origin&, const std::string&,
                   DeletePermissionResult callback) {
        std::move(callback).Run(true);
      });

  base::test::TestFuture<void> future;

  cleaning_service()->ClearConflictingPermissions(credential,
                                                  future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // All updates are guaranteed to be finished here.
  EXPECT_THAT(
      GetAllLoginsSync(store()).at(kSignonRealm),
      UnorderedElementsAre(
          AllOf(Field(&password_manager::PasswordForm::username_value,
                      kExcludeUser),
                Field(&password_manager::PasswordForm::actor_login_approved,
                      true)),
          AllOf(Field(&password_manager::PasswordForm::username_value,
                      Not(kExcludeUser)),
                Field(&password_manager::PasswordForm::actor_login_approved,
                      false))));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.DuplicatePermissionCleaner.Invocations", true,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.DuplicatePermissionCleaner.PasswordsDeleted",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ActorLogin.DuplicatePermissionCleaner.FederatedDeleted",
      1, 1);
}

// Tests that when a new permission is saved for a federated credential
// the other existing permissions are deleted, with the exception of the
// newly-granted one and the one corresponding to the matching password
// credential.
TEST_F(ActorLoginDuplicatePermissionCleanerTest,
       KeepsFederatedPermissionAndPasswordExactMatchIfFederatedSaved) {
  const GURL kUrl("https://example.com/login");
  const std::u16string kExcludedUser = u"user1@gmail.com";
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  const std::string kSignonRealm = kOrigin.GetURL().spec();
  const std::string kAffiliatedRealm = "https://affiliated.com/";
  const GURL kAffiliatedUrl("https://affiliated.com/login");

  password_manager::PasswordForm form1;
  form1.url = kUrl;
  form1.signon_realm = kSignonRealm;
  form1.username_value = kExcludedUser;
  form1.password_value = u"pass1";
  form1.actor_login_approved = true;
  form1.match_type = password_manager::PasswordForm::MatchType::kExact;
  store()->AddLogin(password_manager::FromPasswordForm(form1));

  password_manager::PasswordForm form2;
  form2.url = kAffiliatedUrl;
  form2.signon_realm = kAffiliatedRealm;
  form2.username_value = kExcludedUser;
  form2.password_value = u"pass2";
  form2.actor_login_approved = true;
  form2.match_type = password_manager::PasswordForm::MatchType::kAffiliated;
  store()->AddLogin(password_manager::FromPasswordForm(form2));

  // Wait for store to add login.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetAllLoginsSync(store()).count(kSignonRealm) > 0 &&
           GetAllLoginsSync(store()).count(kAffiliatedRealm) > 0;
  }));

  EXPECT_CALL(mock_affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(base::test::RunOnceCallback<1>(
          affiliations::AffiliatedFacets{
              affiliations::Facet(affiliations::FacetURI::FromCanonicalSpec(
                  "https://affiliated.com"))},
          true));

  Credential credential;
  credential.request_origin = kOrigin;
  credential.type = CredentialType::kFederated;
  credential.username = kExcludedUser;
  FederationDetail detail;
  detail.account_id = "excluded_id";
  credential.federation_detail = detail;

  EXPECT_CALL(*permission_service(), ListPermissions(kOrigin, _))
      .WillOnce([&](const url::Origin& origin, ListPermissionsResult callback) {
        std::vector<FederatedPermission> perms;
        FederatedPermission p1;
        p1.chosen_account_email = base::UTF16ToUTF8(kExcludedUser);
        p1.chosen_account_id = "excluded_id";
        p1.rp_embedder_origin = kOrigin;
        perms.push_back(p1);
        FederatedPermission p2;
        p2.chosen_account_email = "other@example.com";
        p2.chosen_account_id = "other_id";
        p2.rp_embedder_origin = kOrigin;
        perms.push_back(p2);
        std::move(callback).Run(std::move(perms));
      });

  EXPECT_CALL(
      *permission_service(),
      DeletePermission(kOrigin, Eq(base::UTF16ToUTF8(kExcludedUser)), _))
      .Times(0);
  EXPECT_CALL(*permission_service(),
              DeletePermission(kOrigin, "other@example.com", _))
      .WillOnce([](const url::Origin&, const std::string&,
                   DeletePermissionResult callback) {
        std::move(callback).Run(true);
      });

  base::test::TestFuture<void> future;
  cleaning_service()->ClearConflictingPermissions(credential,
                                                  future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // All updates are guaranteed to be finished here.
  EXPECT_TRUE(
      GetAllLoginsSync(store()).at(kSignonRealm)[0].actor_login_approved);
#if !BUILDFLAG(IS_ANDROID)
  // On Android, the `FakePasswordStoreBackend` ignores web affiliations due to
  // the C++ filter in `AffiliatedMatchHelper` (which is dead code in prod but
  // active in tests). So the affiliated credential is not cleared in the test.
  // TODO(crbug.com/504896739): Update the test once the fake backend supports
  // affiliation without the helper on Android.
  EXPECT_FALSE(
      GetAllLoginsSync(store()).at(kAffiliatedRealm)[0].actor_login_approved);
#endif
}

// Tests that when a new permission is saved for a federated credential
// the other existing permissions are deleted, with the exception of the
// newly-granted one and the one corresponding to the matching affiliated
// password credential.
TEST_F(ActorLoginDuplicatePermissionCleanerTest,
       KeepsFederatedPermissionAndPasswordAffiliatedMatchIfFederatedSaved) {
  const GURL kUrl("https://example.com/login");
  const std::u16string kExcludedUser = u"user1@gmail.com";
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  const std::string kSignonRealm = kOrigin.GetURL().spec();
  const std::string kAffiliatedRealm = "https://affiliated.com";

  password_manager::PasswordForm form1;
  form1.url = kUrl;
  form1.signon_realm = kAffiliatedRealm;
  form1.username_value = kExcludedUser;
  form1.password_value = u"pass1";
  form1.actor_login_approved = true;
  form1.match_type = password_manager::PasswordForm::MatchType::kAffiliated;
  store()->AddLogin(password_manager::FromPasswordForm(form1));

  // Wait for store to add login.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetAllLoginsSync(store()).count(kAffiliatedRealm) > 0; }));
  EXPECT_CALL(mock_affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(base::test::RunOnceCallback<1>(
          affiliations::AffiliatedFacets{affiliations::Facet(
              affiliations::FacetURI::FromCanonicalSpec(kAffiliatedRealm))},
          true));

  Credential credential;
  credential.request_origin = kOrigin;
  credential.type = CredentialType::kFederated;
  credential.username = kExcludedUser;
  FederationDetail detail;
  detail.account_id = "excluded_id";
  credential.federation_detail = detail;

  EXPECT_CALL(*permission_service(), ListPermissions(kOrigin, _))
      .WillOnce([&](const url::Origin& origin, ListPermissionsResult callback) {
        std::vector<FederatedPermission> perms;
        FederatedPermission p1;
        p1.chosen_account_email = base::UTF16ToUTF8(kExcludedUser);
        p1.chosen_account_id = "excluded_id";
        p1.rp_embedder_origin = kOrigin;
        perms.push_back(p1);
        FederatedPermission p2;
        p2.chosen_account_email = "other@example.com";
        p2.chosen_account_id = "other_id";
        p2.rp_embedder_origin = kOrigin;
        perms.push_back(p2);
        std::move(callback).Run(std::move(perms));
      });

  EXPECT_CALL(
      *permission_service(),
      DeletePermission(kOrigin, Eq(base::UTF16ToUTF8(kExcludedUser)), _))
      .Times(0);
  EXPECT_CALL(*permission_service(),
              DeletePermission(kOrigin, "other@example.com", _))
      .WillOnce([](const url::Origin&, const std::string&,
                   DeletePermissionResult callback) {
        std::move(callback).Run(true);
      });

  base::test::TestFuture<void> future;
  cleaning_service()->ClearConflictingPermissions(credential,
                                                  future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // All updates are guaranteed to be finished here.
  EXPECT_THAT(
      GetAllLoginsSync(store()).at(kAffiliatedRealm),
      UnorderedElementsAre(AllOf(
          Field(&password_manager::PasswordForm::username_value, kExcludedUser),
          Field(&password_manager::PasswordForm::actor_login_approved, true))));
}

TEST_F(ActorLoginDuplicatePermissionCleanerTest,
       ClearsSameUsernameDifferentSignonRealmPasswordCredential) {
  const GURL kUrl("https://example.com/login");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  const std::u16string kExcludeUser = u"user1";
  const std::string kExcludedSignonRealm = kOrigin.GetURL().spec();
  const std::string kOtherSignonRealm = "https://affiliated.com/";

  password_manager::PasswordForm form1;
  form1.url = kUrl;
  form1.signon_realm = kExcludedSignonRealm;
  form1.username_value = kExcludeUser;
  form1.actor_login_approved = true;
  form1.match_type = password_manager::PasswordForm::MatchType::kExact;
  store()->AddLogin(password_manager::FromPasswordForm(form1));

  password_manager::PasswordForm form2;
  form2.url = GURL("https://affiliated.com/login");
  form2.signon_realm = kOtherSignonRealm;
  form2.username_value = kExcludeUser;
  form2.password_value = u"pass2";
  form2.actor_login_approved = true;
  form2.match_type = password_manager::PasswordForm::MatchType::kAffiliated;
  store()->AddLogin(password_manager::FromPasswordForm(form2));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetAllLoginsSync(store()).count(kExcludedSignonRealm) > 0 &&
           GetAllLoginsSync(store()).count(kOtherSignonRealm) > 0;
  }));

  EXPECT_CALL(mock_affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(base::test::RunOnceCallback<1>(
          affiliations::AffiliatedFacets{
              affiliations::Facet(affiliations::FacetURI::FromCanonicalSpec(
                  "https://affiliated.com"))},
          true));

  EXPECT_CALL(*permission_service(), ListPermissions(kOrigin, _))
      .WillOnce([&](const url::Origin& origin, ListPermissionsResult callback) {
        std::move(callback).Run({});
      });

  Credential credential;
  credential.request_origin = kOrigin;
  credential.username = kExcludeUser;
  credential.type = CredentialType::kPassword;
  credential.signon_realm = kExcludedSignonRealm;

  base::test::TestFuture<void> future;
  cleaning_service()->ClearConflictingPermissions(credential,
                                                  future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_TRUE(GetAllLoginsSync(store())
                  .at(kExcludedSignonRealm)[0]
                  .actor_login_approved);
#if !BUILDFLAG(IS_ANDROID)
  // On Android, the `FakePasswordStoreBackend` ignores web affiliations due to
  // the C++ filter in `AffiliatedMatchHelper` (which is dead code in prod but
  // active in tests). So the affiliated credential is not cleared in the test.
  // TODO(crbug.com/504896739): Update the test once the fake backend supports
  // affiliation without the helper on Android.
  EXPECT_FALSE(
      GetAllLoginsSync(store()).at(kOtherSignonRealm)[0].actor_login_approved);
#endif
}

TEST_F(ActorLoginDuplicatePermissionCleanerTest,
       DoesNotTouchPslAndGroupedMatches) {
  const GURL kUrl("https://example.com/login");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  const std::string kSignonRealm = kOrigin.GetURL().spec();

  password_manager::PasswordForm psl_form;
  psl_form.url = GURL("https://psl.example.com/login");
  psl_form.signon_realm = "https://psl.example.com/";
  psl_form.username_value = u"user2";
  psl_form.actor_login_approved = true;
  psl_form.match_type = password_manager::PasswordForm::MatchType::kPSL;
  store()->AddLogin(password_manager::FromPasswordForm(psl_form));

  password_manager::PasswordForm grouped_form;
  grouped_form.url = GURL("https://grouped.com/login");
  grouped_form.signon_realm = "https://grouped.com/";
  grouped_form.username_value = u"user2";
  grouped_form.actor_login_approved = true;
  grouped_form.match_type = password_manager::PasswordForm::MatchType::kGrouped;
  store()->AddLogin(password_manager::FromPasswordForm(grouped_form));

  // Wait for store to add logins.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetAllLoginsSync(store()).count("https://psl.example.com/") > 0 &&
           GetAllLoginsSync(store()).count("https://grouped.com/") > 0;
  }));

  EXPECT_CALL(mock_affiliation_service(), GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>([]() {
        affiliations::GroupedFacets group;
        group.facets.emplace_back(
            affiliations::FacetURI::FromCanonicalSpec("https://grouped.com"));
        return std::vector<affiliations::GroupedFacets>{group};
      }()));

  ON_CALL(*permission_service(), ListPermissions(kOrigin, _))
      .WillByDefault(
          [&](const url::Origin& origin, ListPermissionsResult callback) {
            std::move(callback).Run({});
          });

  Credential credential;
  credential.request_origin = kOrigin;
  credential.username = u"user1";
  credential.type = CredentialType::kPassword;
  credential.signon_realm = kSignonRealm;

  base::test::TestFuture<void> future;
  cleaning_service()->ClearConflictingPermissions(credential,
                                                  future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // PSL and grouped matches should NOT be touched.
  EXPECT_TRUE(GetAllLoginsSync(store())
                  .at("https://psl.example.com/")[0]
                  .actor_login_approved);
  EXPECT_TRUE(GetAllLoginsSync(store())
                  .at("https://grouped.com/")[0]
                  .actor_login_approved);
}

}  // namespace actor_login
