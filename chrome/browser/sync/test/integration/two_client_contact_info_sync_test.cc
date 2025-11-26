// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/contact_info_helper.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::AutofillProfile;
using autofill::PersonalDataManager;
using contact_info_helper::AddressDataManagerProfileChecker;
using contact_info_helper::BuildTestAccountProfile;
using contact_info_helper::GetAddressDataManager;
using contact_info_helper::GetPersonalDataManager;
using testing::UnorderedElementsAre;

MATCHER(PointeeEquals, "") {
  return *std::get<0>(arg) == *std::get<1>(arg);
}

// Helper class to wait until all clients have the same profiles.
class AutofillProfilesEqualChecker
    : public StatusChangeChecker,
      public autofill::AddressDataManager::Observer {
 public:
  explicit AutofillProfilesEqualChecker(
      std::vector<raw_ptr<Profile, VectorExperimental>> profiles) {
    for (Profile* profile : profiles) {
      pdms_.push_back(contact_info_helper::GetPersonalDataManager(profile));
      pdms_.back()->address_data_manager().AddObserver(this);
    }
  }

  ~AutofillProfilesEqualChecker() override {
    for (PersonalDataManager* pdm : pdms_) {
      pdm->address_data_manager().RemoveObserver(this);
    }
  }

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    if (pdms_.empty()) {
      return true;
    }
    // Compare the profiles of `pdms_[0]` with every other PDM's profiles.
    testing::Matcher<std::vector<const AutofillProfile*>> matcher =
        testing::UnorderedPointwise(
            PointeeEquals(), pdms_[0]->address_data_manager().GetProfiles());
    for (size_t i = 1; i < pdms_.size(); i++) {
      testing::StringMatchResultListener listener;
      if (!testing::ExplainMatchResult(
              matcher, pdms_[i]->address_data_manager().GetProfiles(),
              &listener)) {
        *os << listener.str();
        return false;
      }
    }
    return true;
  }

  // AddressDataManager::Observer implementation.
  void OnAddressDataChanged() override { CheckExitCondition(); }

 private:
  std::vector<raw_ptr<PersonalDataManager, VectorExperimental>> pdms_;
};

class TwoClientContactInfoSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientContactInfoSyncTest() : SyncTest(TWO_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

  bool SetupSyncAndHideAccountNameEmailProfile() {
    if (!SetupSync()) {
      return false;
    }
    HideAccountNameEmailProfile(0);
    HideAccountNameEmailProfile(1);
    return true;
  }

  void HideAccountNameEmailProfile(int profile) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile(profile));
    autofill::test::HideAccountNameEmailProfile(
        GetProfile(profile)->GetPrefs(),
        identity_manager->FindExtendedAccountInfo(
            identity_manager->GetPrimaryAccountInfo(
                signin::ConsentLevel::kSignin)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         TwoClientContactInfoSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(TwoClientContactInfoSyncTest, SyncAddUpdateDelete) {
  ASSERT_TRUE(SetupSyncAndHideAccountNameEmailProfile());
  // Add `profile` on client 0 and expect it to appear on client 1.
  AutofillProfile profile = BuildTestAccountProfile();
  GetAddressDataManager(GetProfile(0))->AddProfile(profile);
  // Here and below `AutofillProfilesEqualChecker()` cannot be used directly.
  // Since `AddProfile()` happens asynchronously, the condition would be true
  // immediately, because no profiles are stored in either PDM yet.
  EXPECT_TRUE(
      AddressDataManagerProfileChecker(GetAddressDataManager(GetProfile(1)),
                                       UnorderedElementsAre(profile))
          .Wait());
  // Now update it from client 1 and expect the update on client 0.
  profile.SetRawInfo(autofill::EMAIL_ADDRESS,
                     u"new-" + profile.GetRawInfo(autofill::EMAIL_ADDRESS));
  GetAddressDataManager(GetProfile(1))->UpdateProfile(profile);
  EXPECT_TRUE(
      AddressDataManagerProfileChecker(GetAddressDataManager(GetProfile(0)),
                                       UnorderedElementsAre(profile))
          .Wait());
  // Finally, remove the `profile` from client 0 and expect removal on client 1.
  GetAddressDataManager(GetProfile(0))->RemoveProfile(profile.guid());
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  GetAddressDataManager(GetProfile(1)), testing::IsEmpty())
                  .Wait());
}

// Adds different profiles with identical GUIDs from both clients. Expects that
// only one copy is retained at each client.
// Since new and migrated profiles receive a randomly generated GUID, collisions
// like this are not expected in practice.
// Whichever profile makes it to the server first wins, since clients always
// merge in favor of the remote version.
IN_PROC_BROWSER_TEST_P(TwoClientContactInfoSyncTest, DuplicateGUID) {
  const AutofillProfile kProfile0 = BuildTestAccountProfile();
  // Constructs a second profile with the same GUID but different content (the
  // exact difference is irrelevant). The lambda is used to keep it const.
  const AutofillProfile kProfile1 = [&] {
    AutofillProfile profile = kProfile0;
    profile.SetRawInfo(autofill::EMAIL_ADDRESS,
                       u"new-" + profile.GetRawInfo(autofill::EMAIL_ADDRESS));
    return profile;
  }();
  CHECK_EQ(kProfile0.guid(), kProfile1.guid());
  CHECK_NE(kProfile0, kProfile1);
  ASSERT_TRUE(SetupSyncAndHideAccountNameEmailProfile());
  HideAccountNameEmailProfile(0);
  HideAccountNameEmailProfile(1);

  // Since `AddProfile()` happens asynchronously, wait for the change to
  // propagate locally.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();
  GetAddressDataManager(GetProfile(0))->AddProfile(kProfile0);
  GetAddressDataManager(GetProfile(1))->AddProfile(kProfile1);
  EXPECT_TRUE(
      AddressDataManagerProfileChecker(GetAddressDataManager(GetProfile(0)),
                                       UnorderedElementsAre(kProfile0))
          .Wait());
  EXPECT_TRUE(
      AddressDataManagerProfileChecker(GetAddressDataManager(GetProfile(1)),
                                       UnorderedElementsAre(kProfile1))
          .Wait());

  // Sync and expect equal profiles eventually.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  EXPECT_TRUE(AutofillProfilesEqualChecker(GetAllProfiles()).Wait());
}

}  // namespace
