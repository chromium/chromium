// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/fast_checkout_delegate.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "ui/gfx/native_widget_types.h"

using ::autofill::AutofillDriver;
using ::autofill::AutofillProfile;
using ::autofill::CreditCard;
using ::testing::_;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;

namespace {

CreditCard GetEmptyCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), "");
  autofill::test::SetCreditCardInfo(&credit_card, /*name_on_card=*/"",
                                    /*card_number=*/"",
                                    autofill::test::NextMonth().c_str(),
                                    autofill::test::NextYear().c_str(), "1");
  return credit_card;
}

constexpr char kUrl[] = "https://www.example.com";
const AutofillProfile kProfile1 = autofill::test::GetFullProfile();
const AutofillProfile kProfile2 = autofill::test::GetFullProfile2();
const AutofillProfile kIncompleteProfile =
    autofill::test::GetIncompleteProfile1();
const CreditCard kCreditCard1 = autofill::test::GetCreditCard();
const CreditCard kCreditCard2 = autofill::test::GetCreditCard2();
const CreditCard kEmptyCreditCard = GetEmptyCreditCard();

std::unique_ptr<KeyedService> BuildTestPersonalDataManager(
    content::BrowserContext* context) {
  auto personal_data_manager =
      std::make_unique<autofill::TestPersonalDataManager>();
  personal_data_manager->SetAutofillProfileEnabled(true);
  personal_data_manager->SetAutofillCreditCardEnabled(true);
  personal_data_manager->SetAutofillWalletImportEnabled(true);
  personal_data_manager->AddProfile(kProfile1);
  personal_data_manager->AddProfile(kProfile2);
  // Add incomplete autofill profile, should not be shown on the sheet.
  personal_data_manager->AddProfile(kIncompleteProfile);
  personal_data_manager->AddCreditCard(kCreditCard1);
  personal_data_manager->AddCreditCard(kCreditCard2);
  // Add empty credit card, should not be shown on the sheet.
  personal_data_manager->AddCreditCard(kEmptyCreditCard);
  return personal_data_manager;
}

struct SupportsConsentlessExecution {
  bool client_supports_consentless = false;
  bool script_supports_consentless = false;
  bool run_consentless = false;
};

}  // namespace

class MockFastCheckoutController : public FastCheckoutController {
 public:
  MockFastCheckoutController() : FastCheckoutController() {}
  ~MockFastCheckoutController() override = default;

  MOCK_METHOD(void,
              Show,
              (const std::vector<AutofillProfile*>& autofill_profiles,
               const std::vector<CreditCard*>& credit_cards),
              (override));
  MOCK_METHOD(void,
              OnOptionsSelected,
              (std::unique_ptr<AutofillProfile> profile,
               std::unique_ptr<CreditCard> credit_card),
              (override));
  MOCK_METHOD(void, OnDismiss, (), (override));
  MOCK_METHOD(void, OpenAutofillProfileSettings, (), (override));
  MOCK_METHOD(void, OpenCreditCardSettings, (), (override));
  MOCK_METHOD(gfx::NativeView, GetNativeView, (), (override));
};

class MockAutofillDriver : public autofill::TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;

  // Mock methods to enable testability.
  MOCK_METHOD(void, SetShouldSuppressKeyboard, (bool), (override));
};

class MockFastCheckoutDelegate : public autofill::FastCheckoutDelegate {
 public:
  explicit MockFastCheckoutDelegate(MockAutofillDriver* driver)
      : driver_(driver) {}
  ~MockFastCheckoutDelegate() override = default;

  MOCK_METHOD(bool,
              TryToShowFastCheckout,
              (const autofill::FormData&, const autofill::FormFieldData&),
              (override));
  MOCK_METHOD(bool, IsShowingFastCheckoutUI, (), (const, override));
  MOCK_METHOD(void, HideFastCheckoutUI, (), (override));
  MOCK_METHOD(void, OnFastCheckoutUIHidden, (), (override));
  MOCK_METHOD(void, Reset, (), (override));

  AutofillDriver* GetDriver() override { return driver_; }

  base::WeakPtr<MockFastCheckoutDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const raw_ptr<MockAutofillDriver> driver_;
  base::WeakPtrFactory<MockFastCheckoutDelegate> weak_factory_{this};
};

class TestFastCheckoutClientImpl : public FastCheckoutClientImpl {
 public:
  static TestFastCheckoutClientImpl* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestFastCheckoutClientImpl(content::WebContents* web_contents)
      : FastCheckoutClientImpl(web_contents) {}

  std::unique_ptr<FastCheckoutController> CreateFastCheckoutController()
      override {
    return std::move(fast_checkout_controller_);
  }

  void InjectFastCheckoutController(
      std::unique_ptr<FastCheckoutController> fast_checkout_controller) {
    fast_checkout_controller_ = std::move(fast_checkout_controller);
  }

 private:
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;
};

// static
TestFastCheckoutClientImpl* TestFastCheckoutClientImpl::CreateForWebContents(
    content::WebContents* web_contents) {
  const void* key = WebContentsUserData<FastCheckoutClientImpl>::UserDataKey();
  web_contents->SetUserData(
      key, std::make_unique<TestFastCheckoutClientImpl>(web_contents));
  return static_cast<TestFastCheckoutClientImpl*>(
      web_contents->GetUserData(key));
}

class FastCheckoutClientImplTest : public ChromeRenderViewHostTestHarness {
 public:
  FastCheckoutClientImplTest() {
    feature_list_.InitWithFeatures({features::kFastCheckout}, {});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&BuildTestPersonalDataManager));

    test_client_ =
        TestFastCheckoutClientImpl::CreateForWebContents(web_contents());

    // Prepare the FastCheckoutController.
    auto fast_checkout_controller =
        std::make_unique<MockFastCheckoutController>();
    fast_checkout_controller_ = fast_checkout_controller.get();
    test_client_->InjectFastCheckoutController(
        std::move(fast_checkout_controller));

    // Prepare the FastCheckoutDelegate.
    autofill_driver_ = std::make_unique<MockAutofillDriver>();
    fast_checkout_delegate_ =
        std::make_unique<MockFastCheckoutDelegate>(autofill_driver_.get());

    // Set AutofillManager on AutofillDriver.
    auto test_autofill_client =
        std::make_unique<autofill::TestAutofillClient>();
    auto test_browser_autofill_manager =
        std::make_unique<autofill::TestBrowserAutofillManager>(
            autofill_driver_.get(), test_autofill_client.release());
    autofill_driver_->set_autofill_manager(
        std::move(test_browser_autofill_manager));
  }

  autofill::TestPersonalDataManager* personal_data_manager() {
    return static_cast<autofill::TestPersonalDataManager*>(
        autofill::PersonalDataManagerFactory::GetForProfile(profile()));
  }

  TestFastCheckoutClientImpl* fast_checkout_client() { return test_client_; }

  MockFastCheckoutController* fast_checkout_controller() {
    return fast_checkout_controller_;
  }

  MockAutofillDriver* autofill_driver() { return autofill_driver_.get(); }

  base::WeakPtr<MockFastCheckoutDelegate> delegate() {
    return fast_checkout_delegate_->GetWeakPtr();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;

  raw_ptr<MockFastCheckoutController> fast_checkout_controller_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
  std::unique_ptr<MockFastCheckoutDelegate> fast_checkout_delegate_;
  raw_ptr<TestFastCheckoutClientImpl> test_client_;
};

TEST_F(
    FastCheckoutClientImplTest,
    GetOrCreateForWebContents_ClientWasAlreadyCreated_ReturnsExistingInstance) {
  raw_ptr<FastCheckoutClient> client =
      FastCheckoutClient::GetOrCreateForWebContents(web_contents());

  // There is only one client per `WebContents`.
  EXPECT_EQ(client, fast_checkout_client());
}

TEST_F(FastCheckoutClientImplTest, Start_FeatureDisabled_NoRuns) {
  // Disable Fast Checkout feature
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kFastCheckout});

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden).Times(0);
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting is not successful which is also represented by the internal state.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Start_FeatureEnabled_RunsSuccessfully) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(true));

  // Expect bottomsheet to show up.
  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                Pointee(kIncompleteProfile)),
           UnorderedElementsAre(Pointee(kCreditCard1), Pointee(kCreditCard2))));

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Cannot start another run.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoProfilesOnFile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all profiles.
  personal_data_manager()->ClearProfiles();
  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      autofill::kUmaKeyFastCheckoutTriggerOutcome,
      autofill::FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile, 1u);
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCompleteProfile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all Profiles.
  personal_data_manager()->ClearProfiles();
  personal_data_manager()->AddProfile(autofill::test::GetIncompleteProfile1());
  personal_data_manager()->AddProfile(autofill::test::GetIncompleteProfile2());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      autofill::kUmaKeyFastCheckoutTriggerOutcome,
      autofill::FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile, 1u);
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCreditCardsOnFile) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all credit cards.
  personal_data_manager()->ClearCreditCards();

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      autofill::kUmaKeyFastCheckoutTriggerOutcome,
      autofill::FastCheckoutTriggerOutcome::kFailureNoValidCreditCard, 1u);
}

TEST_F(FastCheckoutClientImplTest, Start_FailsIfNoCompleteorValidCreditCard) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Remove all credit Cards.
  personal_data_manager()->ClearCreditCards();
  personal_data_manager()->AddCreditCard(
      autofill::test::GetExpiredCreditCard());
  personal_data_manager()->AddCreditCard(
      autofill::test::GetIncompleteCreditCard());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard).Times(0);

  // Starting the run unsuccessfully.
  EXPECT_FALSE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is not running.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  histogram_tester_.ExpectUniqueSample(
      autofill::kUmaKeyFastCheckoutTriggerOutcome,
      autofill::FastCheckoutTriggerOutcome::kFailureNoValidCreditCard, 1u);
}

TEST_F(FastCheckoutClientImplTest,
       OnPersonalDataChanged_StopIfInvalidProfiles) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(1);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Bottom sheet UI is showing.
  ON_CALL(*delegate(), IsShowingFastCheckoutUI).WillByDefault(Return(true));

  // User removes all the profiles.
  personal_data_manager()->ClearProfiles();
  // User adds an incomplete profile only.
  personal_data_manager()->AddProfile(autofill::test::GetIncompleteProfile1());

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnPersonalDataChanged_StopIfInvalidCreditCards) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(1);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Bottom sheet UI is showing.
  ON_CALL(*delegate(), IsShowingFastCheckoutUI).WillByDefault(Return(true));

  // User removes all valid credit cards and adds an incomplete one.
  personal_data_manager()->ClearCreditCards();
  personal_data_manager()->AddCreditCard(
      autofill::test::GetIncompleteCreditCard());

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnPersonalDataChanged_UpdatesTheUIWithNewData) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  EXPECT_CALL(
      *fast_checkout_controller(),
      Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                Pointee(kIncompleteProfile)),
           UnorderedElementsAre(Pointee(kCreditCard1), Pointee(kCreditCard2))));

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Bottom sheet UI is showing.
  ON_CALL(*delegate(), IsShowingFastCheckoutUI).WillByDefault(Return(true));

  // Expect bottomsheet to display the updated info.
  EXPECT_CALL(*fast_checkout_controller(),
              Show(UnorderedElementsAre(Pointee(kProfile1), Pointee(kProfile2),
                                        Pointee(kIncompleteProfile)),
                   UnorderedElementsAre(Pointee(kCreditCard1))));

  // User removes all valid credit cards and adds a valid card.
  personal_data_manager()->ClearCreditCards();
  personal_data_manager()->AddCreditCard(kCreditCard1);

  // `FastCheckoutClient` is still running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Stop_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  fast_checkout_client()->Stop();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, OnDismiss_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden);

  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnOptionsSelected_MovesSelectionsToExternalActionDelegate) {
  // Starting the run successfully starts keyboard suppression.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(true));
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  // Profile selection turns off keyboard suppression again.
  EXPECT_CALL(*autofill_driver(), SetShouldSuppressKeyboard(false));
  EXPECT_CALL(*delegate(), OnFastCheckoutUIHidden);

  // User selected profile and card in bottomsheet.
  fast_checkout_client()->OnOptionsSelected(std::make_unique<AutofillProfile>(),
                                            std::make_unique<CreditCard>());
}

TEST_F(FastCheckoutClientImplTest, RunsSuccessfullyIfDelegateIsDestroyed) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(delegate(), GURL(kUrl)));

  fast_checkout_delegate_.reset();
  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}
