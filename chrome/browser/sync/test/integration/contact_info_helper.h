// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONTACT_INFO_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONTACT_INFO_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contact_info_helper {

autofill::AutofillProfile BuildTestAccountProfile();

autofill::PersonalDataManager* GetPersonalDataManager(Profile* profile);

// Helper class to wait until the AddressDataManager's profiles match a given
// predicate.
class AddressDataManagerProfileChecker
    : public StatusChangeChecker,
      public autofill::AddressDataManager::Observer {
 public:
  AddressDataManagerProfileChecker(
      autofill::AddressDataManager* adm,
      const testing::Matcher<std::vector<autofill::AutofillProfile>>& matcher);
  ~AddressDataManagerProfileChecker() override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // AddressDataManager::Observer overrides.
  void OnAddressDataChanged() override;

 private:
  const raw_ptr<autofill::AddressDataManager> adm_;
  const testing::Matcher<std::vector<autofill::AutofillProfile>> matcher_;
};

}  // namespace contact_info_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONTACT_INFO_HELPER_H_
