// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/contact_info_helper.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contact_info_helper {

namespace {

using autofill::AutofillProfile;

}  // namespace

AutofillProfile BuildTestAccountProfile() {
  AutofillProfile profile = autofill::test::GetFullProfile();
  // The CONTACT_INFO data type is only concerned with kAccount profiles.
  // kLocalOrSyncable profiles are handled by the AUTOFILL_PROFILE type.
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccount);
  return profile;
}

autofill::PersonalDataManager* GetPersonalDataManager(Profile* profile) {
  return autofill::PersonalDataManagerFactory::GetForBrowserContext(profile);
}

AddressDataManagerProfileChecker::AddressDataManagerProfileChecker(
    autofill::AddressDataManager* adm,
    const testing::Matcher<std::vector<AutofillProfile>>& matcher)
    : adm_(adm), matcher_(matcher) {
  adm_->AddObserver(this);
}

AddressDataManagerProfileChecker::~AddressDataManagerProfileChecker() {
  adm_->RemoveObserver(this);
}

bool AddressDataManagerProfileChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  std::vector<AutofillProfile> profiles;
  for (const AutofillProfile* profile : adm_->GetProfiles()) {
    profiles.push_back(*profile);
  }
  testing::StringMatchResultListener listener;
  bool matches = testing::ExplainMatchResult(matcher_, profiles, &listener);
  *os << listener.str();
  return matches;
}

void AddressDataManagerProfileChecker::OnAddressDataChanged() {
  CheckExitCondition();
}

}  // namespace contact_info_helper
