// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_uitest_util.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::AssertionResult;

static PersonalDataManager* GetPersonalDataManager(Profile* profile) {
  return PersonalDataManagerFactory::GetForBrowserContext(profile);
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
      PersonalDataManagerFactory::GetForBrowserContext(base_profile);
  while (!pdm->IsDataLoaded())
    WaitForPersonalDataChange(base_profile);
}

[[nodiscard]] AssertionResult GenerateTestAutofillPopup(
    ContentAutofillDriver& driver,
    Profile* profile,
    bool expect_popup_to_be_shown,
    gfx::RectF element_bounds) {
  ChromeAutofillClient& client =
      static_cast<ChromeAutofillClient&>(driver.GetAutofillClient());
  // It can happen that the window is resized immediately after showing the
  // popup, resulting in the popup to be hidden. If `expect_popup_to_be_shown`
  // is true, the tests assume that the popup will be shown by the end of this
  // function. Not keeping the popup open for testing can result in the popup
  // being randomly hidden and in breaking this assumption. This causes
  // flakiness.
  client.SetKeepPopupOpenForTesting(true);

  FormFieldData field = test::CreateTestFormField(
      "Full name", "name", "", FormControlType::kInputText, "name");
  field.set_is_focusable(true);
  field.set_should_autocomplete(true);
  field.set_bounds(element_bounds);
  FormData form;
  form.set_url(GURL("https://foo.com/bar"));
  form.set_fields({field});

  // Not adding a profile would result in `AskForValuesToFill()` not finding any
  // suggestions and hiding the Autofill Popup.
  // Note: The popup is only shown later in this function. But, without an
  // Autofill Profile, a sequence of nested asynchronous tasks posted on both
  // database and UI threads would result (sometimes) in `AskForValuesToFill()`
  // triggering the hiding of the Autofill Popup when
  // `base::RunLoop().RunUntilIdle()` is called at the end of this function.
  AutofillProfile autofill_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  autofill_profile.SetRawInfo(NAME_FULL, u"John Doe");
  AddTestProfile(profile, autofill_profile);

  TestAutofillManagerSingleEventWaiter wait_for_ask_for_values_to_fill(
      driver.GetAutofillManager(),
      &AutofillManager::Observer::OnAfterAskForValuesToFill);
  gfx::PointF p = element_bounds.origin();
  driver.renderer_events().AskForValuesToFill(
      form, form.fields().front().renderer_id(),
      /*caret_bounds=*/gfx::Rect(gfx::Point(p.x(), p.y()), gfx::Size(0, 10)),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  if (AssertionResult a = std::move(wait_for_ask_for_values_to_fill).Wait();
      !a) {
    return a << " " << __func__ << "(): "
             << "TestAutofillManagerSingleEventWaiter assertion failed";
  }
  if (driver.GetAutofillManager().form_structures().size() != 1u) {
    return testing::AssertionFailure()
           << " " << __func__
           << "(): driver.GetAutofillManager().form_structures().size() != 1u";
  }

  // `form.host_frame` and `form.url` have only been set by
  // ContentAutofillDriver::AskForValuesToFill().
  form = driver.GetAutofillManager()
             .form_structures()
             .begin()
             ->second->ToFormData();

  std::vector<Suggestion> suggestions = {Suggestion(u"John Doe")};
  TestAutofillExternalDelegate* delegate =
      static_cast<TestAutofillExternalDelegate*>(
          test_api(
              static_cast<BrowserAutofillManager&>(driver.GetAutofillManager()))
              .external_delegate());

  // Showing the Autofill Popup is an asynchronous task.
  if (expect_popup_to_be_shown) {
    // `base::RunLoop().RunUntilIdle()` can cause flakiness when waiting for the
    // popup to be shown.
    if (!base::test::RunUntil([&]() { return !delegate->popup_hidden(); })) {
      return testing::AssertionFailure()
             << " " << __func__ << "(): Showing the autofill popup timed out.";
    }
  } else {
    // `base::test::RunUntil()` cannot be used to wait for something not to
    // happen (i.e. for the popup not to be shown).
    base::RunLoop().RunUntilIdle();
    if (!delegate->popup_hidden()) {
      return testing::AssertionFailure()
             << " " << __func__
             << "(): Expected the autofill popup to be hidden, but it is not.";
    }
  }

  // Allow the popup to be hidden. Tests sometimes use this function to create a
  // popup, and then the tests try to hide it.
  client.SetKeepPopupOpenForTesting(false);
  return testing::AssertionSuccess();
}

}  // namespace autofill
