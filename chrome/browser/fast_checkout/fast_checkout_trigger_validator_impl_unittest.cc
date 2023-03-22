// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper.h"
#include "chrome/browser/fast_checkout/mock_fast_checkout_capabilities_fetcher.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Return;

class MockBrowserAutofillManager : public autofill::TestBrowserAutofillManager {
 public:
  using autofill::TestBrowserAutofillManager::TestBrowserAutofillManager;
  MOCK_METHOD(bool, CanShowAutofillUi, (), (const override));
};

class MockAutofillClient : public autofill::TestContentAutofillClient {
 public:
  using autofill::TestContentAutofillClient::TestContentAutofillClient;
  MOCK_METHOD(autofill::LogManager*, GetLogManager, (), (const override));
  MOCK_METHOD(bool, IsContextSecure, (), (const override));
};

class MockPersonalDataHelper : public FastCheckoutPersonalDataHelper {
 public:
  MockPersonalDataHelper() = default;
  ~MockPersonalDataHelper() override = default;

  MOCK_METHOD(std::vector<autofill::CreditCard*>,
              GetValidCreditCards,
              (),
              (const override));
  MOCK_METHOD(std::vector<autofill::AutofillProfile*>,
              GetValidAddressProfiles,
              (),
              (const override));
  MOCK_METHOD(autofill::PersonalDataManager*,
              GetPersonalDataManager,
              (),
              (const override));
  MOCK_METHOD(std::vector<autofill::AutofillProfile*>,
              GetProfilesToSuggest,
              (),
              (const override));
  MOCK_METHOD(std::vector<autofill::CreditCard*>,
              GetCreditCardsToSuggest,
              (),
              (const override));
};

class MockPersonalDataManager : public autofill::PersonalDataManager {
 public:
  MockPersonalDataManager() : PersonalDataManager("en-US") {}
  ~MockPersonalDataManager() override = default;

  MOCK_METHOD(bool, IsAutofillProfileEnabled, (), (const override));
  MOCK_METHOD(bool, IsAutofillCreditCardEnabled, (), (const override));
};

class FastCheckoutTriggerValidatorTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FastCheckoutTriggerValidatorTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFastCheckout},
        /*disabled_features=*/{});
  }

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    pdm_ = std::make_unique<MockPersonalDataManager>();
    capabilities_fetcher_ =
        std::make_unique<MockFastCheckoutCapabilitiesFetcher>();
    personal_data_helper_ = std::make_unique<MockPersonalDataHelper>();
    validator_ = std::make_unique<FastCheckoutTriggerValidatorImpl>(
        autofill_client(), capabilities_fetcher(), personal_data_helper());
    credit_card_ = autofill::test::GetCreditCard();
    profile_ = autofill::test::GetFullProfile();
    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));

    ON_CALL(*autofill_manager(), CanShowAutofillUi).WillByDefault(Return(true));
    ON_CALL(*capabilities_fetcher(), IsTriggerFormSupported)
        .WillByDefault(Return(true));
    ON_CALL(*personal_data_helper(), GetValidCreditCards)
        .WillByDefault(
            Return(std::vector<autofill::CreditCard*>{&credit_card_}));
    ON_CALL(*personal_data_helper(), GetValidAddressProfiles)
        .WillByDefault(
            Return(std::vector<autofill::AutofillProfile*>{&profile_}));
    ON_CALL(*personal_data_helper(), GetPersonalDataManager)
        .WillByDefault(Return(pdm()));
    ON_CALL(*pdm(), IsAutofillCreditCardEnabled).WillByDefault(Return(true));
    ON_CALL(*pdm(), IsAutofillProfileEnabled).WillByDefault(Return(true));
    ON_CALL(*autofill_client(), IsContextSecure).WillByDefault(Return(true));
  }

  MockPersonalDataManager* pdm() { return pdm_.get(); }
  MockAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }
  MockFastCheckoutCapabilitiesFetcher* capabilities_fetcher() {
    return capabilities_fetcher_.get();
  }
  MockPersonalDataHelper* personal_data_helper() {
    return personal_data_helper_.get();
  }
  MockBrowserAutofillManager* autofill_manager() {
    return autofill_manager_injector_[web_contents()];
  }
  FastCheckoutTriggerValidatorImpl* validator() { return validator_.get(); }

  bool ShouldRun() {
    return validator()->ShouldRun(form_, field_, ui_state_, is_running_,
                                  *autofill_manager());
  }

  // Protected for access in tests below.
  autofill::FormFieldData field_;
  FastCheckoutUIState ui_state_ = FastCheckoutUIState::kNotShownYet;
  bool is_running_ = false;
  base::HistogramTester histogram_tester_;

 private:
  autofill::AutofillProfile profile_;
  autofill::CreditCard credit_card_;
  autofill::FormData form_;
  base::test::ScopedFeatureList feature_list_;
  autofill::TestAutofillClientInjector<MockAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillDriverInjector<autofill::TestContentAutofillDriver>
      autofill_driver_injector_;
  autofill::TestAutofillManagerInjector<MockBrowserAutofillManager>
      autofill_manager_injector_;
  std::unique_ptr<FastCheckoutTriggerValidatorImpl> validator_;
  std::unique_ptr<MockFastCheckoutCapabilitiesFetcher> capabilities_fetcher_;
  std::unique_ptr<MockPersonalDataHelper> personal_data_helper_;
  std::unique_ptr<MockPersonalDataManager> pdm_;
};

TEST_F(FastCheckoutTriggerValidatorTest, ShouldRun_AllChecksPass_ReturnsTrue) {
  EXPECT_TRUE(ShouldRun());
  histogram_tester_.ExpectUniqueSample(kUmaKeyFastCheckoutTriggerOutcome,
                                       FastCheckoutTriggerOutcome::kSuccess,
                                       1u);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_FeatureDisabled_ReturnsFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(::features::kFastCheckout);

  EXPECT_FALSE(ShouldRun());
  EXPECT_EQ(histogram_tester_.GetTotalSum(kUmaKeyFastCheckoutTriggerOutcome),
            0);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_AlreadyRunning_ReturnsFalse) {
  is_running_ = true;

  EXPECT_FALSE(ShouldRun());
  EXPECT_EQ(histogram_tester_.GetTotalSum(kUmaKeyFastCheckoutTriggerOutcome),
            0);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_NotContextSecure_ReturnsFalse) {
  ON_CALL(*autofill_client(), IsContextSecure).WillByDefault(Return(false));

  EXPECT_FALSE(ShouldRun());
  EXPECT_EQ(histogram_tester_.GetTotalSum(kUmaKeyFastCheckoutTriggerOutcome),
            0);
}

TEST_F(FastCheckoutTriggerValidatorTest, ShouldRun_NoTriggerForm_ReturnsFalse) {
  ON_CALL(*capabilities_fetcher(), IsTriggerFormSupported)
      .WillByDefault(Return(false));

  EXPECT_FALSE(ShouldRun());
  EXPECT_EQ(histogram_tester_.GetTotalSum(kUmaKeyFastCheckoutTriggerOutcome),
            0);
}

TEST_F(FastCheckoutTriggerValidatorTest, ShouldRun_UiIsShowing_ReturnsFalse) {
  ui_state_ = FastCheckoutUIState::kIsShowing;

  EXPECT_FALSE(ShouldRun());
  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureShownBefore, 1u);
}

TEST_F(FastCheckoutTriggerValidatorTest, ShouldRun_UiWasShown_ReturnsFalse) {
  ui_state_ = FastCheckoutUIState::kWasShown;

  EXPECT_FALSE(ShouldRun());
  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureShownBefore, 1u);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_FieldNotFocusable_ReturnsFalse) {
  field_.is_focusable = false;

  EXPECT_FALSE(ShouldRun());
  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureFieldNotFocusable, 1u);
}

TEST_F(FastCheckoutTriggerValidatorTest, ShouldRun_FieldHasValue_ReturnsFalse) {
  field_.value = u"value";

  EXPECT_FALSE(ShouldRun());
  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureFieldNotEmpty, 1u);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_CannotShowAutofillUi_ReturnsFalse) {
  ON_CALL(*autofill_manager(), CanShowAutofillUi).WillByDefault(Return(false));

  EXPECT_FALSE(ShouldRun());
  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureCannotShowAutofillUi, 1u);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_AutofillProfileDisabled_ReturnsFalse) {
  ON_CALL(*pdm(), IsAutofillProfileEnabled).WillByDefault(Return(false));

  EXPECT_FALSE(ShouldRun());
  EXPECT_EQ(histogram_tester_.GetTotalSum(kUmaKeyFastCheckoutTriggerOutcome),
            0);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_CreditCardDisabled_ReturnsFalse) {
  ON_CALL(*pdm(), IsAutofillCreditCardEnabled).WillByDefault(Return(false));

  EXPECT_FALSE(ShouldRun());
  EXPECT_EQ(histogram_tester_.GetTotalSum(kUmaKeyFastCheckoutTriggerOutcome),
            0);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_NoValidAddressProfiles_ReturnsFalse) {
  ON_CALL(*personal_data_helper(), GetValidAddressProfiles)
      .WillByDefault(Return(std::vector<autofill::AutofillProfile*>{}));

  EXPECT_FALSE(ShouldRun());
  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile, 1u);
}

TEST_F(FastCheckoutTriggerValidatorTest,
       ShouldRun_NoValidCreditCards_ReturnsFalse) {
  ON_CALL(*personal_data_helper(), GetValidCreditCards)
      .WillByDefault(Return(std::vector<autofill::CreditCard*>{}));

  EXPECT_FALSE(ShouldRun());
  histogram_tester_.ExpectUniqueSample(
      kUmaKeyFastCheckoutTriggerOutcome,
      FastCheckoutTriggerOutcome::kFailureNoValidCreditCard, 1u);
}

}  // namespace
