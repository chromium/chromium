// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "content/public/test/browser_test.h"

using fake_server::FakeServer;
using sync_pb::SyncEntity;
using sync_pb::UserConsentSpecifics;
using sync_pb::UserConsentTypes;
using SyncConsent = sync_pb::UserConsentTypes::SyncConsent;

namespace {

CoreAccountId GetAccountId() {
  return CoreAccountId::FromGaiaId(
      signin::GetTestGaiaIdForEmail(SyncTest::kDefaultUserEmail));
}

class UserConsentEqualityChecker : public SingleClientStatusChangeChecker {
 public:
  UserConsentEqualityChecker(
      syncer::SyncServiceImpl* service,
      FakeServer* fake_server,
      std::vector<UserConsentSpecifics> expected_specifics)
      : SingleClientStatusChangeChecker(service), fake_server_(fake_server) {
    for (const UserConsentSpecifics& specifics : expected_specifics) {
      expected_specifics_.insert(std::pair<int64_t, UserConsentSpecifics>(
          specifics.consent_case(), specifics));
    }
  }

  UserConsentEqualityChecker(const UserConsentEqualityChecker&) = delete;
  UserConsentEqualityChecker& operator=(const UserConsentEqualityChecker&) =
      delete;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting server side USER_CONSENTS to match expected.";
    std::vector<SyncEntity> entities =
        fake_server_->GetSyncEntitiesByDataType(syncer::USER_CONSENTS);

    // |entities.size()| is only going to grow, if |entities.size()| ever
    // becomes bigger then all hope is lost of passing, stop now.
    EXPECT_GE(expected_specifics_.size(), entities.size());

    if (expected_specifics_.size() > entities.size()) {
      return false;
    }

    // Number of events on server matches expected, exit condition can be
    // satisfied. Let's verify that content matches as well. It is safe to
    // modify |expected_specifics_|.
    for (const SyncEntity& entity : entities) {
      UserConsentSpecifics server_specifics = entity.specifics().user_consent();
      auto iter = expected_specifics_.find(server_specifics.consent_case());
      EXPECT_NE(expected_specifics_.end(), iter);
      if (expected_specifics_.end() == iter) {
        return false;
      }
      EXPECT_EQ(iter->second.account_id(), server_specifics.account_id());
      expected_specifics_.erase(iter);
    }

    return true;
  }

 private:
  const raw_ptr<FakeServer> fake_server_;
  // TODO(markusheintz): User a string with the serialized proto instead of an
  // int. The requires creating better expectations with a proper creation
  // time.
  std::multimap<int64_t, UserConsentSpecifics> expected_specifics_;
};

class SingleClientUserConsentsSyncTest : public SyncTest {
 public:
  SingleClientUserConsentsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientUserConsentsSyncTest() override = default;

  bool ExpectUserConsents(
      std::vector<UserConsentSpecifics> expected_specifics) {
    return UserConsentEqualityChecker(GetSyncService(0), GetFakeServer(),
                                      expected_specifics)
        .Wait();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientUserConsentsSyncTest, ShouldSubmit) {
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(
      0u,
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::USER_CONSENTS).size());
  consent_auditor::ConsentAuditor* consent_service =
      ConsentAuditorFactory::GetForProfile(GetProfile(0));
  UserConsentSpecifics specifics;
  specifics.mutable_sync_consent()->set_confirmation_grd_id(1);
  specifics.set_account_id(GetAccountId().ToString());

  SyncConsent sync_consent;
  sync_consent.set_confirmation_grd_id(1);
  sync_consent.set_status(UserConsentTypes::GIVEN);

  consent_service->RecordSyncConsent(GetAccountId(), sync_consent);
  EXPECT_TRUE(ExpectUserConsents({specifics}));
}

// ChromeOS does not support signing out of a primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(
    SingleClientUserConsentsSyncTest,
    ShouldPreserveConsentsOnSignoutAndResubmitWhenReenabled) {
  UserConsentSpecifics specifics;
  specifics.mutable_sync_consent()->set_confirmation_grd_id(1);
  // Account id may be compared to the synced account, thus, we need them to
  // match.
  specifics.set_account_id(GetAccountId().ToString());

  ASSERT_TRUE(SetupSync());
  consent_auditor::ConsentAuditor* consent_service =
      ConsentAuditorFactory::GetForProfile(GetProfile(0));

  SyncConsent sync_consent;
  sync_consent.set_confirmation_grd_id(1);
  sync_consent.set_status(UserConsentTypes::GIVEN);
  consent_service->RecordSyncConsent(GetAccountId(), sync_consent);

  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_TRUE(GetClient(0)->SetupSync());

  EXPECT_TRUE(ExpectUserConsents({specifics}));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientUserConsentsSyncTest,
                       ShouldPreserveConsentsLoggedBeforeSyncSetup) {
  SyncConsent consent1;
  consent1.set_confirmation_grd_id(1);
  consent1.set_status(UserConsentTypes::GIVEN);
  SyncConsent consent2;
  consent2.set_confirmation_grd_id(2);
  consent2.set_status(UserConsentTypes::GIVEN);

  UserConsentSpecifics specifics1;
  *specifics1.mutable_sync_consent() = consent1;
  specifics1.set_account_id(GetAccountId().ToString());
  UserConsentSpecifics specifics2;
  *specifics2.mutable_sync_consent() = consent2;
  specifics2.set_account_id(GetAccountId().ToString());

  // Set up the clients (profiles), but do *not* set up Sync yet.
  ASSERT_TRUE(SetupClients());

  // Now we can already record a consent, but of course it won't make it to the
  // server yet.
  consent_auditor::ConsentAuditor* consent_service =
      ConsentAuditorFactory::GetForProfile(GetProfile(0));
  consent_service->RecordSyncConsent(GetAccountId(), consent1);
  EXPECT_TRUE(ExpectUserConsents({}));

  // Once we turn on Sync, the consent gets uploaded.
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(ExpectUserConsents({specifics1}));

  // Another consent can also be added now.
  consent_service->RecordSyncConsent(GetAccountId(), consent2);
  EXPECT_TRUE(ExpectUserConsents({specifics1, specifics2}));
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientUserConsentsSyncTest,
                       ShouldSubmitIfSignedInAlthoughFullSyncNotEnabled) {
  // We avoid calling SetupSync(), because we don't want to turn on full sync,
  // only sign in such that the standalone transport starts.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive())
      << "Full sync should be disabled";
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::USER_CONSENTS));

  SyncConsent sync_consent;
  sync_consent.set_confirmation_grd_id(1);
  sync_consent.set_status(UserConsentTypes::GIVEN);

  ConsentAuditorFactory::GetForProfile(GetProfile(0))
      ->RecordSyncConsent(GetAccountId(), sync_consent);

  UserConsentSpecifics specifics;
  SyncConsent* expected_sync_consent = specifics.mutable_sync_consent();
  expected_sync_consent->set_confirmation_grd_id(1);
  expected_sync_consent->set_status(UserConsentTypes::GIVEN);
  // Account id may be compared to the synced account, thus, we need them to
  // match.
  specifics.set_account_id(GetAccountId().ToString());
  EXPECT_TRUE(ExpectUserConsents({specifics}));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
