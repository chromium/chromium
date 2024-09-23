// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper_impl.h"

#include "base/uuid.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::autofill::AutofillProfile;
using ::autofill::CreditCard;
using ::testing::Eq;

CreditCard GetEmptyCreditCard() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "");
  autofill::test::SetCreditCardInfo(&credit_card, /*name_on_card=*/"",
                                    /*card_number=*/"",
                                    autofill::test::NextMonth().c_str(),
                                    autofill::test::NextYear().c_str(), "1");
  return credit_card;
}

const AutofillProfile kProfile = autofill::test::GetFullProfile();
const AutofillProfile kIncompleteProfile =
    autofill::test::GetIncompleteProfile1();
const CreditCard kCreditCard = autofill::test::GetCreditCard();
const CreditCard kEmptyCreditCard = GetEmptyCreditCard();

std::unique_ptr<KeyedService> BuildTestPersonalDataManager(
    content::BrowserContext* context) {
  auto personal_data_manager =
      std::make_unique<autofill::TestPersonalDataManager>();
  personal_data_manager->test_address_data_manager().SetAutofillProfileEnabled(
      true);
  personal_data_manager->test_payments_data_manager()
      .SetAutofillPaymentMethodsEnabled(true);
  personal_data_manager->test_payments_data_manager()
      .SetAutofillWalletImportEnabled(true);
  return personal_data_manager;
}

class FastCheckoutPersonalDataHelperTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&BuildTestPersonalDataManager));

    personal_data_helper_ =
        std::make_unique<FastCheckoutPersonalDataHelperImpl>(web_contents());
  }

  FastCheckoutPersonalDataHelperImpl* personal_data_helper() {
    return personal_data_helper_.get();
  }

 private:
  std::unique_ptr<FastCheckoutPersonalDataHelperImpl> personal_data_helper_;
};

TEST_F(FastCheckoutPersonalDataHelperTest,
       GetPersonalDataManager_ReturnsValidPointer) {
  EXPECT_TRUE(personal_data_helper()->GetPersonalDataManager());
}

TEST_F(FastCheckoutPersonalDataHelperTest,
       NoValidAddressProfiles_HasValidProfiles_ReturnsFalse) {
  personal_data_helper()
      ->GetPersonalDataManager()
      ->address_data_manager()
      .AddProfile(kProfile);

  EXPECT_FALSE(personal_data_helper()->GetValidAddressProfiles().empty());
}

TEST_F(FastCheckoutPersonalDataHelperTest,
       NoValidAddressProfiles_HasOnlyInvalidProfiles_ReturnsTrue) {
  personal_data_helper()
      ->GetPersonalDataManager()
      ->address_data_manager()
      .AddProfile(kIncompleteProfile);

  EXPECT_TRUE(personal_data_helper()->GetValidAddressProfiles().empty());
}

TEST_F(FastCheckoutPersonalDataHelperTest,
       NoValidAddressProfiles_HasValidAndInvalidProfiles_ReturnsFalse) {
  personal_data_helper()
      ->GetPersonalDataManager()
      ->address_data_manager()
      .AddProfile(kProfile);
  personal_data_helper()
      ->GetPersonalDataManager()
      ->address_data_manager()
      .AddProfile(kIncompleteProfile);

  EXPECT_FALSE(personal_data_helper()->GetValidAddressProfiles().empty());
}

TEST_F(FastCheckoutPersonalDataHelperTest,
       NoValidCreditCards_HasValidCreditCards_ReturnsFalse) {
  personal_data_helper()
      ->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(kCreditCard);

  EXPECT_FALSE(personal_data_helper()->GetValidCreditCards().empty());
}

TEST_F(FastCheckoutPersonalDataHelperTest,
       NoValidCreditCards_HasOnlyInvalidCreditCards_ReturnsTrue) {
  personal_data_helper()
      ->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(kEmptyCreditCard);

  EXPECT_TRUE(personal_data_helper()->GetValidCreditCards().empty());
}

TEST_F(FastCheckoutPersonalDataHelperTest,
       NoValidCreditCards_HasValidAndInvalidCreditCards_ReturnsFalse) {
  personal_data_helper()
      ->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(kCreditCard);
  personal_data_helper()
      ->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(kEmptyCreditCard);

  EXPECT_FALSE(personal_data_helper()->GetValidCreditCards().empty());
}

TEST_F(FastCheckoutPersonalDataHelperTest,
       GetCreditCardsToSuggest_ReturnsOnlyCardsWithNumber) {
  personal_data_helper()
      ->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(kCreditCard);
  personal_data_helper()
      ->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(kEmptyCreditCard);

  std::vector<autofill::CreditCard*> cards =
      personal_data_helper()->GetCreditCardsToSuggest();

  EXPECT_EQ(cards.size(), 1UL);
  EXPECT_EQ(*cards.front(), kCreditCard);
}

TEST_F(FastCheckoutPersonalDataHelperTest,
       GetProfilesToSuggest_ReturnsAllProfiles) {
  personal_data_helper()
      ->GetPersonalDataManager()
      ->address_data_manager()
      .AddProfile(kProfile);
  personal_data_helper()
      ->GetPersonalDataManager()
      ->address_data_manager()
      .AddProfile(kIncompleteProfile);

  std::vector<const autofill::AutofillProfile*> profiles =
      personal_data_helper()->GetProfilesToSuggest();

  EXPECT_EQ(profiles.size(), 2UL);
}

}  // namespace
