// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/browsing_data/core/counters/autofill_counter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_annotations/test_user_annotations_service.h"
#include "components/user_annotations/user_annotations_service.h"
#include "components/user_annotations/user_annotations_types.h"
#include "content/public/test/browser_test.h"

namespace {

class AutofillCounterTest : public InProcessBrowserTest {
 public:
  AutofillCounterTest() = default;

  AutofillCounterTest(const AutofillCounterTest&) = delete;
  AutofillCounterTest& operator=(const AutofillCounterTest&) = delete;

  ~AutofillCounterTest() override {}

  void SetUpOnMainThread() override {
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForBrowserContext(
            browser()->profile());
    web_data_service_ = WebDataServiceFactory::GetAutofillWebDataForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    user_annotations_service_ =
        std::make_unique<user_annotations::TestUserAnnotationsService>();

    SetAutofillDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  void TearDownOnMainThread() override {
    // Release our refs to let browser tear down of services complete in the
    // same order as usual.
    personal_data_manager_ = nullptr;
    web_data_service_ = nullptr;
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

  // User annotations ----------------------------------------------------------

  void SetUserAnnotations(user_annotations::UserAnnotationsEntries entries) {
    user_annotations_service_->ReplaceAllEntries(std::move(entries));
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

  autofill::PersonalDataManager* GetPersonalDataManager() {
    return personal_data_manager_;
  }

  scoped_refptr<autofill::AutofillWebDataService> GetWebDataService() {
    return web_data_service_;
  }

  raw_ptr<user_annotations::TestUserAnnotationsService>
  GetUserAnnotationsService() {
    return user_annotations_service_.get();
  }

  // Callback and result retrieval ---------------------------------------------

  void WaitForCounting() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  browsing_data::BrowsingDataCounter::ResultInt GetNumSuggestions() {
    DCHECK(finished_);
    return num_suggestions_;
  }

  browsing_data::BrowsingDataCounter::ResultInt GetNumCreditCards() {
    DCHECK(finished_);
    return num_credit_cards_;
  }

  browsing_data::BrowsingDataCounter::ResultInt GetNumAddresses() {
    DCHECK(finished_);
    return num_addresses_;
  }

  browsing_data::BrowsingDataCounter::ResultInt GetNumUserAnnotations() {
    DCHECK(finished_);
    return num_user_annotations_;
  }

  void Callback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    finished_ = result->Finished();

    if (finished_) {
      browsing_data::AutofillCounter::AutofillResult* autofill_result =
          static_cast<browsing_data::AutofillCounter::AutofillResult*>(
              result.get());

      num_suggestions_ = autofill_result->Value();
      num_credit_cards_ = autofill_result->num_credit_cards();
      num_addresses_ = autofill_result->num_addresses();
      num_user_annotations_ = autofill_result->num_user_annotation_entries();

      if (run_loop_)
        run_loop_->Quit();
    }
  }

 private:
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;

  std::vector<std::string> credit_card_ids_;
  std::vector<std::string> address_ids_;

  raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;
  std::unique_ptr<user_annotations::TestUserAnnotationsService>
      user_annotations_service_;

  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt num_suggestions_;
  browsing_data::BrowsingDataCounter::ResultInt num_credit_cards_;
  browsing_data::BrowsingDataCounter::ResultInt num_addresses_;
  browsing_data::BrowsingDataCounter::ResultInt num_user_annotations_;
};

// Tests that we count the correct number of autocomplete suggestions.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, AutocompleteSuggestions) {
  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter(
      GetPersonalDataManager(), GetWebDataService(),
      /*user_annotations_service=*/nullptr, /*sync_service=*/nullptr);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&AutofillCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0, GetNumSuggestions());

  AddAutocompleteSuggestion("email", "example@example.com");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(1, GetNumSuggestions());

  AddAutocompleteSuggestion("tel", "+123456789");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(2, GetNumSuggestions());

  AddAutocompleteSuggestion("tel", "+987654321");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(3, GetNumSuggestions());

  RemoveAutocompleteSuggestion("email", "example@example.com");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(2, GetNumSuggestions());

  ClearAutocompleteSuggestions();
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0, GetNumSuggestions());
}

// Tests that we count the correct number of credit cards.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, CreditCards) {
  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter(
      GetPersonalDataManager(), GetWebDataService(),
      /*user_annotations_service=*/nullptr, /*sync_service=*/nullptr);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&AutofillCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0, GetNumCreditCards());

  AddCreditCard("0000-0000-0000-0000", "1", "2015", "1");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(1, GetNumCreditCards());

  AddCreditCard("0123-4567-8910-1112", "10", "2015", "1");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(2, GetNumCreditCards());

  AddCreditCard("1211-1098-7654-3210", "10", "2030", "1");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(3, GetNumCreditCards());

  RemoveLastCreditCard();
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(2, GetNumCreditCards());

  ClearCreditCardsAndAddresses();
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0, GetNumCreditCards());
}

// Tests that we count the correct number of addresses.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, Addresses) {
  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter(
      GetPersonalDataManager(), GetWebDataService(),
      /*user_annotations_service=*/nullptr, /*sync_service=*/nullptr);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&AutofillCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0, GetNumAddresses());

  AddAddress("John", "Doe", "Main Street 12345");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(1, GetNumAddresses());

  AddAddress("Jane", "Smith", "Main Street 12346");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(2, GetNumAddresses());

  AddAddress("John", "Smith", "Side Street 47");
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(3, GetNumAddresses());

  RemoveLastAddress();
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(2, GetNumAddresses());

  ClearCreditCardsAndAddresses();
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0, GetNumAddresses());
}

// Tests that we count the correct number of user annotations.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, UserAnnotations) {
  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter(
      GetPersonalDataManager(), GetWebDataService(),
      GetUserAnnotationsService(), /*sync_service=*/nullptr);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&AutofillCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(0, GetNumUserAnnotations());

  SetUserAnnotations(
      std::vector(10, optimization_guide::proto::UserAnnotationsEntry{}));
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(10, GetNumUserAnnotations());
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
  browsing_data::AutofillCounter counter(
      GetPersonalDataManager(), GetWebDataService(),
      /*user_annotations_service=*/nullptr, /*sync_service=*/nullptr);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&AutofillCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(5, GetNumSuggestions());
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
    const browsing_data::BrowsingDataCounter::ResultInt
        expected_num_user_annotations;
  } test_cases[] = {{base::Time(), 2, 3, 3, 0},
                    {kTime1, 2, 3, 3, 0},
                    {kTime2, 1, 2, 2, 0},
                    {kTime3, 1, 1, 0, 0}};

  Profile* profile = browser()->profile();
  browsing_data::AutofillCounter counter(
      GetPersonalDataManager(), GetWebDataService(),
      /*user_annotations_service=*/nullptr, /*sync_service=*/nullptr);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&AutofillCounterTest::Callback,
                                   base::Unretained(this)));

  for (size_t i = 0; i < std::size(test_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test case %zu", i));
    const auto& test_case = test_cases[i];
    counter.SetPeriodStartForTesting(test_case.period_start);
    counter.Restart();
    WaitForCounting();
    EXPECT_EQ(test_case.expected_num_suggestions, GetNumSuggestions());
    EXPECT_EQ(test_case.expected_num_credit_cards, GetNumCreditCards());
    EXPECT_EQ(test_case.expected_num_addresses, GetNumAddresses());
    EXPECT_EQ(test_case.expected_num_user_annotations, GetNumUserAnnotations());
  }

  // Test the results for different ending points and base::Time as start.
  counter.SetPeriodStartForTesting(base::Time());
  counter.SetPeriodEndForTesting(kTime2);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(1, GetNumSuggestions());
  EXPECT_EQ(1, GetNumCreditCards());
  EXPECT_EQ(1, GetNumAddresses());
  EXPECT_EQ(0, GetNumUserAnnotations());

  counter.SetPeriodEndForTesting(kTime3);
  counter.Restart();
  WaitForCounting();
  EXPECT_EQ(1, GetNumSuggestions());
  EXPECT_EQ(2, GetNumCreditCards());
  EXPECT_EQ(3, GetNumAddresses());
  EXPECT_EQ(0, GetNumUserAnnotations());
}

}  // namespace
