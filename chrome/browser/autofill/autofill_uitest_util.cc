// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "content/public/test/test_utils.h"

namespace autofill {

// This class is used to wait for asynchronous updates to PersonalDataManager
// to complete.
class PdmChangeWaiter : public PersonalDataManagerObserver {
 public:
  explicit PdmChangeWaiter(Browser* browser)
      : alerted_(false), has_run_message_loop_(false), browser_(browser) {
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        AddObserver(this);
  }

  ~PdmChangeWaiter() override {}

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override {
    if (has_run_message_loop_) {
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
      has_run_message_loop_ = false;
    }
    alerted_ = true;
  }

  void OnInsufficientFormData() override { OnPersonalDataChanged(); }

  void Wait() {
    if (!alerted_) {
      has_run_message_loop_ = true;
      content::RunMessageLoop();
    }
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        RemoveObserver(this);
  }

 private:
  bool alerted_;
  bool has_run_message_loop_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(PdmChangeWaiter);
};

static PersonalDataManager* GetPersonalDataManager(Profile* profile) {
  return PersonalDataManagerFactory::GetForProfile(profile);
}

void AddTestProfile(Browser* browser, const AutofillProfile& profile) {
  PdmChangeWaiter observer(browser);
  GetPersonalDataManager(browser->profile())->AddProfile(profile);

  // AddProfile is asynchronous. Wait for it to finish before continuing the
  // tests.
  observer.Wait();
}

void SetTestProfile(Browser* browser, const AutofillProfile& profile) {
  std::vector<AutofillProfile> profiles;
  profiles.push_back(profile);
  SetTestProfiles(browser, &profiles);
}

void SetTestProfiles(Browser* browser, std::vector<AutofillProfile>* profiles) {
  PdmChangeWaiter observer(browser);
  GetPersonalDataManager(browser->profile())->SetProfiles(profiles);
  observer.Wait();
}

void AddTestCreditCard(Browser* browser, const CreditCard& card) {
  PdmChangeWaiter observer(browser);
  GetPersonalDataManager(browser->profile())->AddCreditCard(card);

  // AddCreditCard is asynchronous. Wait for it to finish before continuing the
  // tests.
  observer.Wait();
}

void AddTestServerCreditCard(Browser* browser, const CreditCard& card) {
  PdmChangeWaiter observer(browser);
  GetPersonalDataManager(browser->profile())->AddFullServerCreditCard(card);

  // AddCreditCard is asynchronous. Wait for it to finish before continuing the
  // tests.
  observer.Wait();
}

void AddTestAutofillData(Browser* browser,
                         const AutofillProfile& profile,
                         const CreditCard& card) {
  AddTestProfile(browser, profile);
  PdmChangeWaiter observer(browser);
  GetPersonalDataManager(browser->profile())->AddCreditCard(card);
  observer.Wait();
}

void WaitForPersonalDataChange(Browser* browser) {
  PdmChangeWaiter observer(browser);
  observer.Wait();
}

}  // namespace autofill
