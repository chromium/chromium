// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/contact_info_helper.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contact_info_helper {

namespace {

using autofill::AutofillProfile;
using autofill::PersonalDataManager;

}  // namespace

autofill::AutofillProfile BuildTestAccountProfile() {
  AutofillProfile profile = autofill::test::GetFullProfile();
  // The CONTACT_INFO data type is only concerned with kAccount profiles.
  // kLocalOrSyncable profiles are handled by the AUTOFILL_PROFILE type.
  profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  return profile;
}

PersonalDataManager* GetPersonalDataManager(Profile* profile) {
  return autofill::PersonalDataManagerFactory::GetForProfile(profile);
}

PersonalDataManagerProfileChecker::PersonalDataManagerProfileChecker(
    PersonalDataManager* pdm,
    const testing::Matcher<std::vector<AutofillProfile>>& matcher)
    : pdm_(pdm), matcher_(matcher) {
  pdm_->AddObserver(this);
}

PersonalDataManagerProfileChecker::~PersonalDataManagerProfileChecker() {
  pdm_->RemoveObserver(this);
}

bool PersonalDataManagerProfileChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  std::vector<AutofillProfile> profiles;
  for (AutofillProfile* profile : pdm_->GetProfiles()) {
    profiles.push_back(*profile);
  }
  testing::StringMatchResultListener listener;
  bool matches = testing::ExplainMatchResult(matcher_, profiles, &listener);
  *os << listener.str();
  return matches;
}

void PersonalDataManagerProfileChecker::OnPersonalDataChanged() {
  CheckExitCondition();
}

}  // namespace contact_info_helper
