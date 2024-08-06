// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AutofillProfileDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit AutofillProfileDisabledChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}
  ~AutofillProfileDisabledChecker() override = default;

  // SingleClientStatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for AUTOFILL_PROFILE to get disabled";
    return service()->GetTransportState() ==
               syncer::SyncService::TransportState::ACTIVE &&
           !service()->GetActiveDataTypes().Has(syncer::AUTOFILL_PROFILE);
  }
};

class SingleClientAutofillProfileSyncTest : public SyncTest {
 public:
  SingleClientAutofillProfileSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientAutofillProfileSyncTest(
      const SingleClientAutofillProfileSyncTest&) = delete;
  SingleClientAutofillProfileSyncTest& operator=(
      const SingleClientAutofillProfileSyncTest&) = delete;

  ~SingleClientAutofillProfileSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientAutofillProfileSyncTest,
                       DisablingAutofillDoesNotDisableSyncing) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_PROFILE));

  // Add an autofill profile.
  autofill_helper::AddProfile(0, autofill_helper::CreateAutofillProfile(
                                     autofill_helper::PROFILE_HOMER));
  autofill::PersonalDataManager* pdm =
      autofill_helper::GetPersonalDataManager(0);
  ASSERT_EQ(1uL, pdm->address_data_manager().GetProfiles().size());

  // Disable autofill (e.g. via chrome://settings).
  autofill::prefs::SetAutofillProfileEnabled(GetProfile(0)->GetPrefs(), false);

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetClient(0)->service()->GetTransportState());

  // This should not disable syncing of autofill profiles. Otherwise, if the
  // user deletes profiles while Autofill is disabled and then re-enables
  // Autofill, sync retrieves the seemingly deleted profiles
  // (crbug.com/1320097).
  EXPECT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_PROFILE));
  // The autofill profile itself should still be there.
  EXPECT_EQ(1uL, pdm->address_data_manager().GetProfiles().size());
}

}  // namespace
