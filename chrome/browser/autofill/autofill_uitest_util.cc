// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_uitest_util.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

static PersonalDataManager* GetPersonalDataManager(Profile* profile) {
  return PersonalDataManagerFactory::GetForProfile(profile);
}

void AddTestProfile(Profile* base_profile, const AutofillProfile& profile) {
  PersonalDataManager* pdm = GetPersonalDataManager(base_profile);
  PersonalDataChangedWaiter waiter(*pdm);
  pdm->address_data_manager().AddProfile(profile);
  std::move(waiter).Wait();
}

void AddTestCreditCard(Profile* base_profile, const CreditCard& card) {
  PersonalDataManager* pdm = GetPersonalDataManager(base_profile);
  PersonalDataChangedWaiter waiter(*pdm);
  pdm->payments_data_manager().AddCreditCard(card);
  std::move(waiter).Wait();
}

void AddTestServerCreditCard(Profile* base_profile, const CreditCard& card) {
  PersonalDataManager* pdm = GetPersonalDataManager(base_profile);
  PersonalDataChangedWaiter waiter(*pdm);
  test_api(pdm->payments_data_manager()).AddServerCreditCard(card);
  std::move(waiter).Wait();
}

void AddTestAutofillData(Profile* base_profile,
                         const AutofillProfile& profile,
                         const CreditCard& card) {
  AddTestProfile(base_profile, profile);
  AddTestCreditCard(base_profile, card);
}

void WaitForPersonalDataChange(Profile* base_profile) {
  PersonalDataChangedWaiter(*GetPersonalDataManager(base_profile)).Wait();
}

void WaitForPersonalDataManagerToBeLoaded(Profile* base_profile) {
  PersonalDataManager* pdm =
      PersonalDataManagerFactory::GetForProfile(base_profile);
  while (!pdm->IsDataLoaded())
    WaitForPersonalDataChange(base_profile);
}

void GenerateTestAutofillPopup(ContentAutofillDriver& driver,
                               gfx::RectF element_bounds) {
  FormData form;
  form.url = GURL("https://foo.com/bar");
  form.fields.emplace_back();
  form.fields.front().is_focusable = true;
  form.fields.front().should_autocomplete = true;

  TestAutofillManagerWaiter waiter(driver.GetAutofillManager(),
                                   {AutofillManagerEvent::kAskForValuesToFill});
  driver.renderer_events().AskForValuesToFill(
      form, form.fields.front(), element_bounds,
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  ASSERT_TRUE(waiter.Wait());
  ASSERT_EQ(1u, driver.GetAutofillManager().form_structures().size());
  // `form.host_frame` and `form.url` have only been set by
  // ContentAutofillDriver::AskForValuesToFill().
  form = driver.GetAutofillManager()
             .form_structures()
             .begin()
             ->second->ToFormData();

  std::vector<Suggestion> suggestions = {Suggestion(u"Test suggestion")};
  test_api(static_cast<BrowserAutofillManager&>(driver.GetAutofillManager()))
      .external_delegate()
      ->OnSuggestionsReturned(form.fields.front().global_id(), suggestions);
}

}  // namespace autofill
