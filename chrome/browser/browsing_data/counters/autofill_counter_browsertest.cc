// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/autofill_counter.h"

#include <array>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace {

using browsing_data::AutofillCounter;
typedef base::test::TestFuture<
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result>>
    CounterFuture;
class AutofillCounterTest : public InProcessBrowserTest {
 public:
  AutofillCounterTest() = default;

  AutofillCounterTest(const AutofillCounterTest&) = delete;
  AutofillCounterTest& operator=(const AutofillCounterTest&) = delete;

  ~AutofillCounterTest() override = default;

  void SetUpOnMainThread() override {
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForBrowserContext(
            browser()->profile());
    web_data_service_ = WebDataServiceFactory::GetAutofillWebDataForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);

    SetAutofillDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  void TearDownOnMainThread() override {
    // Release our refs to let browser tear down of services complete in the
    // same order as usual.
    personal_data_manager_ = nullptr;
    web_data_service_ = nullptr;
  }

  browsing_data::AutofillCounter GetCounter() {
    return {GetPersonalDataManager(), GetWebDataService(),
            GetEntityDataManager(), /*sync_service=*/nullptr};
  }

  // Autocomplete suggestions --------------------------------------------------

  void AddAutocompleteSuggestion(const std::string& name,
                                 const std::string& value) {
    autofill::FormFieldData field;
    field.set_name(base::ASCIIToUTF16(name));
    field.set_value(base::ASCIIToUTF16(value));

    std::vector<autofill::FormFieldData> form_fields;
    form_fields.push_back(field);
    web_data_service_->AddFormFields(form_fields);
  }

  void RemoveAutocompleteSuggestion(const std::string& name,
                                    const std::string& value) {
    web_data_service_->RemoveFormValueForElementName(
        base::ASCIIToUTF16(name),
        base::ASCIIToUTF16(value));
  }

  void ClearAutocompleteSuggestions() {
    web_data_service_->RemoveFormElementsAddedBetween(
        base::Time(), base::Time::Max());
  }

  // Credit cards --------------------------------------------------------------

  void AddCreditCard(const char* card_number,
                     const char* exp_month,
                     const char* exp_year,
                     const char* billing_address_id) {
    autofill::CreditCard card;
    autofill::test::SetCreditCardInfo(&card, nullptr, card_number, exp_month,
                                      exp_year, billing_address_id);
    credit_card_ids_.push_back(card.guid());
    personal_data_manager_->payments_data_manager().AddCreditCard(card);
    autofill::PersonalDataChangedWaiter(*personal_data_manager_).Wait();
  }

  void RemoveLastCreditCard() {
    personal_data_manager_->payments_data_manager().RemoveByGUID(
        credit_card_ids_.back());
    autofill::PersonalDataChangedWaiter(*personal_data_manager_).Wait();
    credit_card_ids_.pop_back();
  }

  // Addresses -----------------------------------------------------------------

  void AddAddress(const std::string& name,
                  const std::string& surname,
                  const std::string& address) {
    autofill::AutofillProfile profile(
        autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
    std::string id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    address_ids_.push_back(id);
    profile.set_guid(id);
    profile.SetInfo(autofill::NAME_FIRST, base::ASCIIToUTF16(name), "en-US");
    profile.SetInfo(autofill::NAME_LAST, base::ASCIIToUTF16(surname), "en-US");
    profile.SetInfo(autofill::ADDRESS_HOME_LINE1, base::ASCIIToUTF16(address),
                    "en-US");
    personal_data_manager_->address_data_manager().AddProfile(profile);
    autofill::PersonalDataChangedWaiter(*personal_data_manager_).Wait();
  }

  void RemoveLastAddress() {
    personal_data_manager_->address_data_manager().RemoveProfile(
        address_ids_.back());
    autofill::PersonalDataChangedWaiter(*personal_data_manager_).Wait();
    address_ids_.pop_back();
  }

  // Other autofill utils ------------------------------------------------------

  void ClearCreditCardsAndAddresses() {
    while (!credit_card_ids_.empty()) {
      RemoveLastCreditCard();
    }
    while (!address_ids_.empty()) {
      RemoveLastAddress();
    }
  }

  // Other utils ---------------------------------------------------------------

  void SetAutofillDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeleteFormData, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  autofill::EntityDataManager* GetEntityDataManager() {
    return autofill::AutofillEntityDataManagerFactory::GetForProfile(
        browser()->profile());
  }

  autofill::PersonalDataManager* GetPersonalDataManager() {
    return personal_data_manager_;
  }

  scoped_refptr<autofill::AutofillWebDataService> GetWebDataService() {
    return web_data_service_;
  }

  // Result retrieval ---------------------------------------------

  browsing_data::BrowsingDataCounter::ResultInt GetNumSuggestions() {
    return num_suggestions_;
  }

  browsing_data::BrowsingDataCounter::ResultInt GetNumCreditCards() {
    return num_credit_cards_;
  }

  browsing_data::BrowsingDataCounter::ResultInt GetNumAddresses() {
    return num_addresses_;
  }

  browsing_data::BrowsingDataCounter::ResultInt GetNumEntities() {
    return num_entities_;
  }

  browsing_data::AutofillCounter::ResultInt GetCounterValue() {
    return counter_value_;
  }

  void WaitForResult() {
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result =
        future.Take();
    while (!result->Finished()) {
      future.Clear();
      result = future.Take();
    }

    AutofillCounter::AutofillResult* autofill_result =
        static_cast<AutofillCounter::AutofillResult*>(result.get());
    counter_value_ = autofill_result->Value();
    num_credit_cards_ = autofill_result->num_credit_cards();
    num_addresses_ = autofill_result->num_addresses();
    num_entities_ = autofill_result->num_entities();
  }

 protected:
  CounterFuture future;

 private:
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::vector<std::string> credit_card_ids_;
  std::vector<std::string> address_ids_;

  std::unique_ptr<base::RunLoop> run_loop_;

  raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  browsing_data::BrowsingDataCounter::ResultInt counter_value_;
  browsing_data::BrowsingDataCounter::ResultInt num_suggestions_;
  browsing_data::BrowsingDataCounter::ResultInt num_credit_cards_;
  browsing_data::BrowsingDataCounter::ResultInt num_addresses_;
  browsing_data::BrowsingDataCounter::ResultInt num_entities_;
};

// Tests that we count the correct number of autocomplete suggestions.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, AutocompleteSuggestions) {
  browsing_data::AutofillCounter counter = GetCounter();
  counter.Init(browser()->profile()->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               future.GetRepeatingCallback());
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(0, GetCounterValue());

  AddAutocompleteSuggestion("email", "example@example.com");
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(1, GetCounterValue());

  AddAutocompleteSuggestion("tel", "+123456789");
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(2, GetCounterValue());

  AddAutocompleteSuggestion("tel", "+987654321");
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(3, GetCounterValue());

  RemoveAutocompleteSuggestion("email", "example@example.com");
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(2, GetCounterValue());

  ClearAutocompleteSuggestions();
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(0, GetCounterValue());
}

IN_PROC_BROWSER_TEST_F(AutofillCounterTest, Entities) {
  browsing_data::AutofillCounter counter = GetCounter();
  counter.Init(browser()->profile()->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               future.GetRepeatingCallback());
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(GetNumEntities(), 0);

  autofill::EntityInstance passport =
      autofill::test::GetPassportEntityInstance();
  GetEntityDataManager()->AddOrUpdateEntityInstance(passport);
  autofill::EntityDataChangedWaiter(GetEntityDataManager()).Wait();
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(GetNumEntities(), 1);

  autofill::EntityInstance drivers_license =
      autofill::test::GetDriversLicenseEntityInstance();
  GetEntityDataManager()->AddOrUpdateEntityInstance(drivers_license);
  autofill::EntityDataChangedWaiter(GetEntityDataManager()).Wait();
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(GetNumEntities(), 2);

  GetEntityDataManager()->RemoveEntityInstance(passport.guid());
  autofill::EntityDataChangedWaiter(GetEntityDataManager()).Wait();
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(GetNumEntities(), 1);
}

// Tests that we count the correct number of credit cards.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, CreditCards) {
  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter = GetCounter();

  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               future.GetRepeatingCallback());
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(0, GetNumCreditCards());

  AddCreditCard("0000-0000-0000-0000", "1", "2015", "1");
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(1, GetNumCreditCards());

  AddCreditCard("0123-4567-8910-1112", "10", "2015", "1");
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(2, GetNumCreditCards());

  AddCreditCard("1211-1098-7654-3210", "10", "2030", "1");
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(3, GetNumCreditCards());

  RemoveLastCreditCard();
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(2, GetNumCreditCards());

  ClearCreditCardsAndAddresses();
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(0, GetNumCreditCards());
}

// Tests that we count the correct number of addresses.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, Addresses) {
  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter = GetCounter();

  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               future.GetRepeatingCallback());
  counter.Restart();

  WaitForResult();
  EXPECT_EQ(0, GetNumAddresses());

  AddAddress("John", "Doe", "Main Street 12345");
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(1, GetNumAddresses());

  AddAddress("Jane", "Smith", "Main Street 12346");
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(2, GetNumAddresses());

  AddAddress("John", "Smith", "Side Street 47");
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(3, GetNumAddresses());

  RemoveLastAddress();
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(2, GetNumAddresses());

  ClearCreditCardsAndAddresses();
  counter.Restart();
  WaitForResult();

  EXPECT_EQ(0, GetNumAddresses());
}

// Tests that we return the correct complex result when counting more than
// one type of items.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, ComplexResult) {
  AddAutocompleteSuggestion("email", "example@example.com");
  AddAutocompleteSuggestion("zip", "12345");
  AddAutocompleteSuggestion("tel", "+123456789");
  AddAutocompleteSuggestion("tel", "+987654321");
  AddAutocompleteSuggestion("city", "Munich");

  AddCreditCard("0000-0000-0000-0000", "1", "2015", "1");
  AddCreditCard("1211-1098-7654-3210", "10", "2030", "1");

  AddAddress("John", "Doe", "Main Street 12345");
  AddAddress("Jane", "Smith", "Main Street 12346");
  AddAddress("John", "Smith", "Side Street 47");

  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter = GetCounter();

  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               future.GetRepeatingCallback());
  counter.Restart();

  WaitForResult();

  EXPECT_EQ(5, GetCounterValue());
  EXPECT_EQ(2, GetNumCreditCards());
  EXPECT_EQ(3, GetNumAddresses());
}

// Tests that the counting respects time ranges.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, TimeRanges) {
  autofill::TestAutofillClock test_clock;
  const base::Time kTime1 = base::Time::FromSecondsSinceUnixEpoch(25);
  test_clock.SetNow(kTime1);
  AddAutocompleteSuggestion("email", "example@example.com");
  AddCreditCard("0000-0000-0000-0000", "1", "2015", "1");
  AddAddress("John", "Doe", "Main Street 12345");
  base::ThreadPoolInstance::Get()->FlushForTesting();

  const base::Time kTime2 = kTime1 + base::Seconds(10);
  test_clock.SetNow(kTime2);
  AddCreditCard("0123-4567-8910-1112", "10", "2015", "1");
  AddAddress("Jane", "Smith", "Main Street 12346");
  AddAddress("John", "Smith", "Side Street 47");
  base::ThreadPoolInstance::Get()->FlushForTesting();

  const base::Time kTime3 = kTime2 + base::Seconds(10);
  test_clock.SetNow(kTime3);
  AddAutocompleteSuggestion("tel", "+987654321");
  AddCreditCard("1211-1098-7654-3210", "10", "2030", "1");
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Test the results for different starting points.
  struct TestCase {
    const base::Time period_start;
    const browsing_data::BrowsingDataCounter::ResultInt
        expected_num_suggestions;
    const browsing_data::BrowsingDataCounter::ResultInt
        expected_num_credit_cards;
    const browsing_data::BrowsingDataCounter::ResultInt expected_num_addresses;
  };
  auto test_cases = std::to_array<TestCase>({
      {base::Time(), 2, 3, 3},
      {kTime1, 2, 3, 3},
      {kTime2, 1, 2, 2},
      {kTime3, 1, 1, 0},
  });

  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter = GetCounter();

  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               future.GetRepeatingCallback());

  for (size_t i = 0; i < std::size(test_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test case %zu", i));
    const auto& test_case = test_cases[i];
    counter.SetPeriodStartForTesting(test_case.period_start);
    counter.Restart();

    WaitForResult();

    EXPECT_EQ(test_case.expected_num_suggestions, GetCounterValue());
    EXPECT_EQ(test_case.expected_num_credit_cards, GetNumCreditCards());
    EXPECT_EQ(test_case.expected_num_addresses, GetNumAddresses());
    future.Clear();
  }

  // Test the results for different ending points and base::Time as start.
  counter.SetPeriodStartForTesting(base::Time());
  counter.SetPeriodEndForTesting(kTime2);
  counter.Restart();

  WaitForResult();
  EXPECT_EQ(1, GetCounterValue());
  EXPECT_EQ(1, GetNumCreditCards());
  EXPECT_EQ(1, GetNumAddresses());

  counter.SetPeriodEndForTesting(kTime3);
  counter.Restart();

  WaitForResult();

  EXPECT_EQ(1, GetCounterValue());
  EXPECT_EQ(2, GetNumCreditCards());
  EXPECT_EQ(3, GetNumAddresses());
}

}  // namespace
