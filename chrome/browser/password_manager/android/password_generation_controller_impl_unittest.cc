// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/password_manager/android/password_generation_controller_impl.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_manual_filling_controller.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/fake_touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/mock_touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::password_generation::PasswordGenerationType;
using password_manager::metrics_util::GenerationDialogChoice;

namespace {
using autofill::FooterCommand;
using autofill::mojom::FocusedFieldType;
using autofill::password_generation::PasswordGenerationUIData;
using base::ASCIIToUTF16;
using password_manager::ContentPasswordManagerDriver;
using password_manager::MockPasswordStoreInterface;
using password_manager::PasswordForm;
using testing::_;
using testing::AtMost;
using testing::ByMove;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using ShouldShowAction = ManualFillingController::ShouldShowAction;
using TouchToFillOutcome =
    password_manager::metrics_util::TouchToFillPasswordGenerationTriggerOutcome;

const char kTouchToFillTriggerOutcomeHistogramName[] =
    "PasswordManager.TouchToFill.PasswordGeneration.TriggerOutcome";

class TestPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  TestPasswordManagerClient();
  ~TestPasswordManagerClient() override;

  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override;

  MOCK_METHOD((const password_manager::PasswordManager*),
              GetPasswordManager,
              (),
              (const, override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const override));

 private:
  scoped_refptr<MockPasswordStoreInterface> mock_password_store_;
};

TestPasswordManagerClient::TestPasswordManagerClient() {
  mock_password_store_ = new MockPasswordStoreInterface();
}

TestPasswordManagerClient::~TestPasswordManagerClient() = default;

password_manager::PasswordStoreInterface*
TestPasswordManagerClient::GetProfilePasswordStore() const {
  return mock_password_store_.get();
}

PasswordGenerationUIData GetTestGenerationUIData1() {
  PasswordGenerationUIData data;

  data.form_data.set_action(GURL("http://www.example1.com/accounts/Login"));
  data.form_data.set_url(GURL("http://www.example1.com/accounts/LoginAuth"));

  data.generation_element = u"testelement1";
  data.max_length = 10;

  return data;
}

PasswordGenerationUIData GetTestGenerationUIData2() {
  PasswordGenerationUIData data;

  data.form_data.set_action(GURL("http://www.example2.com/accounts/Login"));
  data.form_data.set_url(GURL("http://www.example2.com/accounts/LoginAuth"));

  data.generation_element = u"testelement2";
  data.max_length = 10;

  return data;
}

MATCHER_P(PointsToSameAddress, expected, "") {
  return arg.get() == expected;
}

}  // namespace

class PasswordGenerationControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  using CreateTouchToFillGenerationControllerFactory = base::RepeatingCallback<
      std::unique_ptr<TouchToFillPasswordGenerationController>()>;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    test_pwd_manager_client_ = std::make_unique<TestPasswordManagerClient>();
    password_manager_ = std::make_unique<password_manager::PasswordManager>(
        test_pwd_manager_client_.get());
    ON_CALL(*test_pwd_manager_client_, GetPasswordManager())
        .WillByDefault(Return(password_manager_.get()));
    ON_CALL(*test_pwd_manager_client_, GetPrefs())
        .WillByDefault(Return(pref_service()));

    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kPasswordGenerationBottomSheetDismissCount, 0);

    password_manager_driver_ = std::make_unique<ContentPasswordManagerDriver>(
        main_rfh(), test_pwd_manager_client_.get());
    another_password_manager_driver_ =
        std::make_unique<ContentPasswordManagerDriver>(
            main_rfh(), test_pwd_manager_client_.get());

    // TODO(crbug.com/41462048): Remove once kAutofillKeyboardAccessory is
    // enabled.
    password_autofill_manager_ =
        std::make_unique<password_manager::PasswordAutofillManager>(
            password_manager_driver_.get(), &test_autofill_client_,
            test_pwd_manager_client_.get());

    ON_CALL(create_ttf_generation_controller_, Run).WillByDefault([this]() {
      return controller()->CreateTouchToFillGenerationControllerForTesting(
          std::make_unique<MockTouchToFillPasswordGenerationBridge>(),
          mock_manual_filling_controller_.AsWeakPtr());
    });

    PasswordGenerationControllerImpl::CreateForWebContentsForTesting(
        web_contents(), test_pwd_manager_client_.get(),
        mock_manual_filling_controller_.AsWeakPtr(),
        create_ttf_generation_controller_.Get());
    EXPECT_CALL(mock_manual_filling_controller_,
                OnAccessoryActionAvailabilityChanged(
                    ShouldShowAction(false),
                    autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
    controller()->FocusedInputChanged(FocusedFieldType::kFillablePasswordField,
                                      active_driver());
  }

  PasswordGenerationController* controller() {
    return PasswordGenerationControllerImpl::FromWebContents(web_contents());
  }

  base::WeakPtr<password_manager::ContentPasswordManagerDriver>
  active_driver() {
    return password_manager_driver_->AsWeakPtrImpl();
  }

  base::WeakPtr<password_manager::ContentPasswordManagerDriver>
  non_active_driver() {
    return another_password_manager_driver_->AsWeakPtrImpl();
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 protected:
  StrictMock<MockManualFillingController> mock_manual_filling_controller_;

  std::unique_ptr<ContentPasswordManagerDriver> password_manager_driver_;
  std::unique_ptr<ContentPasswordManagerDriver>
      another_password_manager_driver_;
  base::MockCallback<CreateTouchToFillGenerationControllerFactory>
      create_ttf_generation_controller_;
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<password_manager::PasswordManager> password_manager_;
  std::unique_ptr<password_manager::PasswordAutofillManager>
      password_autofill_manager_;
  std::unique_ptr<TestPasswordManagerClient> test_pwd_manager_client_;
  autofill::TestAutofillClient test_autofill_client_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordGenerationControllerTest, IsNotRecreatedForSameWebContents) {
  PasswordGenerationController* initial_controller =
      PasswordGenerationControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  PasswordGenerationControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(PasswordGenerationControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(PasswordGenerationControllerTest, RelaysAutomaticGenerationAvailable) {
  // TODO (crbug.com/1421753): Test this is for the
  // PasswordGenerationBottomSheet flag disabled. Add one more test for the case
  // after the bottom sheet is dismissed.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/true, gfx::RectF(100, 20));
}

// Tests that if AutomaticGenerationAvailable is called for different
// password forms, the form and field signatures used for password generation
// are updated.
TEST_F(PasswordGenerationControllerTest,
       UpdatesSignaturesForDifferentGenerationForms) {
  // Called twice for different forms.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC))
      .Times(2);
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/true, gfx::RectF(100, 20));
  PasswordGenerationUIData new_ui_data = GetTestGenerationUIData2();
  controller()->OnAutomaticGenerationAvailable(active_driver(), new_ui_data,
                                               /*has_saved_credentials=*/true,
                                               gfx::RectF(100, 20));

  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(new_ui_data.form_data);
  autofill::FieldSignature field_signature =
      autofill::CalculateFieldSignatureByNameAndType(
          new_ui_data.generation_element,
          autofill::FormControlType::kInputPassword);
  EXPECT_EQ(controller()->get_form_signature_for_testing(), form_signature);
  EXPECT_EQ(controller()->get_field_signature_for_testing(), field_signature);

  EXPECT_CALL(create_ttf_generation_controller_, Run);
  controller()->OnGenerationRequested(PasswordGenerationType::kAutomatic);
}

TEST_F(PasswordGenerationControllerTest,
       RecordsGeneratedPasswordAcceptedAutomatic) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/true, gfx::RectF(100, 20));

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->GeneratedPasswordAccepted(u"t3stp@ssw0rd", active_driver(),
                                          PasswordGenerationType::kAutomatic);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GenerationDialogChoice.Automatic",
      GenerationDialogChoice::kAccepted, 1);
}

TEST_F(PasswordGenerationControllerTest,
       RecordsGeneratedPasswordRejectedAutomatic) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->GeneratedPasswordRejected(PasswordGenerationType::kAutomatic);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GenerationDialogChoice.Automatic",
      GenerationDialogChoice::kRejected, 1);
}

TEST_F(PasswordGenerationControllerTest,
       RecordsGeneratedPasswordAcceptedManual) {
  base::HistogramTester histogram_tester;

  controller()->OnGenerationRequested(PasswordGenerationType::kManual);

  EXPECT_CALL(create_ttf_generation_controller_, Run);
  controller()->ShowManualGenerationDialog(password_manager_driver_.get(),
                                           GetTestGenerationUIData1());

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->GeneratedPasswordAccepted(u"t3stp@ssw0rd", active_driver(),
                                          PasswordGenerationType::kManual);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GenerationDialogChoice.Manual",
      GenerationDialogChoice::kAccepted, 1);
}

TEST_F(PasswordGenerationControllerTest,
       RecordsGeneratedPasswordRejectedManual) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->GeneratedPasswordRejected(PasswordGenerationType::kManual);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GenerationDialogChoice.Manual",
      GenerationDialogChoice::kRejected, 1);
}

TEST_F(PasswordGenerationControllerTest,
       SetActiveFrameOnAutomaticGenerationAvailable) {
  // TODO(crbug.com/40259397): Refactor PasswordGenerationController so that
  // OnAccessoryActionAvailabilityChanged would be called only once. Right now
  // it's called twice: the first call resets the manual filling controller
  // status and the second one sets it according to the focused input.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  _, autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC))
      .Times(AtMost(2));

  controller()->OnAutomaticGenerationAvailable(
      non_active_driver(), GetTestGenerationUIData2(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));
}

TEST_F(PasswordGenerationControllerTest,
       ResetStateWhenFocusChangesToNonPassword) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));

  controller()->FocusedInputChanged(FocusedFieldType::kFillableUsernameField,
                                    active_driver());
  EXPECT_FALSE(controller()->GetActiveFrameDriver());
}

TEST_F(PasswordGenerationControllerTest,
       ResetStateWhenFocusChangesToOtherFramePassword) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));

  controller()->FocusedInputChanged(FocusedFieldType::kFillablePasswordField,
                                    non_active_driver());
  EXPECT_EQ(another_password_manager_driver_.get(),
            controller()->GetActiveFrameDriver().get());
}

TEST_F(PasswordGenerationControllerTest,
       RejectShowManualDialogForNonActiveFrame) {
  EXPECT_CALL(create_ttf_generation_controller_, Run).Times(0);
  controller()->ShowManualGenerationDialog(
      another_password_manager_driver_.get(), GetTestGenerationUIData1());
}

TEST_F(PasswordGenerationControllerTest, DontShowManualDialogIfFocusChanged) {
  controller()->OnGenerationRequested(PasswordGenerationType::kManual);

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->FocusedInputChanged(FocusedFieldType::kFillablePasswordField,
                                    non_active_driver());
  EXPECT_CALL(create_ttf_generation_controller_, Run).Times(0);
  controller()->ShowManualGenerationDialog(password_manager_driver_.get(),
                                           GetTestGenerationUIData1());
}

TEST_F(PasswordGenerationControllerTest,
       DoesNotCallKeyboardAccessoryWhenGenerationBottomSheetRequired) {
  base::HistogramTester histogram_tester;

  auto ttf_password_generation_bridge =
      std::make_unique<MockTouchToFillPasswordGenerationBridge>();
  MockTouchToFillPasswordGenerationBridge* ttf_password_generation_bridge_ptr =
      ttf_password_generation_bridge.get();
  EXPECT_CALL(create_ttf_generation_controller_, Run)
      .WillOnce([this, &ttf_password_generation_bridge]() {
        return controller()->CreateTouchToFillGenerationControllerForTesting(
            std::move(ttf_password_generation_bridge),
            mock_manual_filling_controller_.AsWeakPtr());
      });

  // Keyboard accessory shouldn't show up.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC))
      .Times(0);
  EXPECT_CALL(*ttf_password_generation_bridge_ptr, Show);
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));
  histogram_tester.ExpectUniqueSample(kTouchToFillTriggerOutcomeHistogramName,
                                      TouchToFillOutcome::kShown, 1);
  // Removes the keyboard suppression callback from the render widget host. It
  // needs to be done before the `PasswordGenerationController` destructor is
  // called.
  controller()->HideBottomSheetIfNeeded();
}

TEST_F(PasswordGenerationControllerTest,
       DoesNotCallKeyboardAccessoryWhenBottomSheetIsDisplayed) {
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));

  // Keyboard accessory shouldn't be called.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  _, autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC))
      .Times(0);
  EXPECT_CALL(create_ttf_generation_controller_, Run).Times(0);
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));
  // Removes the keyboard suppression callback from the render widget host. It
  // needs to be done before the `PasswordGenerationController` destructor is
  // called.
  controller()->HideBottomSheetIfNeeded();
}

TEST_F(PasswordGenerationControllerTest,
       CallsKeyboardAccessoryWhenGenerationBottomSheetFailedToShow) {
  base::HistogramTester histogram_tester;

  auto ttf_password_generation_bridge =
      std::make_unique<MockTouchToFillPasswordGenerationBridge>();
  EXPECT_CALL(*ttf_password_generation_bridge, Show).WillOnce(Return(false));
  EXPECT_CALL(create_ttf_generation_controller_, Run)
      .WillOnce([this, &ttf_password_generation_bridge]() {
        return controller()->CreateTouchToFillGenerationControllerForTesting(
            std::move(ttf_password_generation_bridge),
            mock_manual_filling_controller_.AsWeakPtr());
      });

  // Keyboard accessory should show up.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));
  histogram_tester.ExpectUniqueSample(kTouchToFillTriggerOutcomeHistogramName,
                                      TouchToFillOutcome::kFailedToDisplay, 1);
}

TEST_F(PasswordGenerationControllerTest,
       DoesNotShowGenerationBottomSheetIfSavedPasswordsAvailable) {
  base::HistogramTester histogram_tester;

  // Password generation bottom sheet must not show up. Keyboard accessory
  // should show up instead.
  EXPECT_CALL(create_ttf_generation_controller_, Run).Times(0);
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/true, gfx::RectF(100, 20));
  histogram_tester.ExpectUniqueSample(kTouchToFillTriggerOutcomeHistogramName,
                                      TouchToFillOutcome::kHasSavedCredentials,
                                      1);
}

TEST_F(PasswordGenerationControllerTest,
       DoesNotShowGenerationBottomSheetIfDismissCountAtLeast4) {
  base::HistogramTester histogram_tester;

  pref_service()->SetInteger(
      password_manager::prefs::kPasswordGenerationBottomSheetDismissCount, 4);

  // Password generation bottom sheet must not show up. Keyboard accessory
  // should show up instead.
  EXPECT_CALL(create_ttf_generation_controller_, Run).Times(0);
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));
  histogram_tester.ExpectUniqueSample(
      kTouchToFillTriggerOutcomeHistogramName,
      TouchToFillOutcome::kDismissed4TimesInARow, 1);
}

TEST_F(PasswordGenerationControllerTest,
       CallsKeyboardAccessoryAfterBottomSheetDismissed) {
  base::HistogramTester histogram_tester;
  auto ttf_password_generation_bridge =
      std::make_unique<FakeTouchToFillPasswordGenerationBridge>();
  FakeTouchToFillPasswordGenerationBridge* ttf_password_generation_bridge_ptr =
      ttf_password_generation_bridge.get();
  EXPECT_CALL(create_ttf_generation_controller_, Run)
      .WillOnce([this, &ttf_password_generation_bridge]() {
        return controller()->CreateTouchToFillGenerationControllerForTesting(
            std::move(ttf_password_generation_bridge),
            mock_manual_filling_controller_.AsWeakPtr());
      });

  // Keyboard accessory shouldn't be called.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  _, autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC))
      .Times(0);
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));
  histogram_tester.ExpectBucketCount(kTouchToFillTriggerOutcomeHistogramName,
                                     TouchToFillOutcome::kShown, 1);

  ttf_password_generation_bridge_ptr->OnDismissed(
      /*env=*/nullptr, /*generated_password_accepted=*/false);

  // Keyboard accessory should be displayed.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  _, autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));
  histogram_tester.ExpectBucketCount(kTouchToFillTriggerOutcomeHistogramName,
                                     TouchToFillOutcome::kShownBefore, 1);

  // Removes the keyboard suppression callback from the render widget host. It
  // needs to be done before the `PasswordGenerationController` destructor is
  // called.
  controller()->HideBottomSheetIfNeeded();
}

TEST_F(PasswordGenerationControllerTest,
       RequestingGenerationResetsBottomSheetDismissCount) {
  // Set up as if user has dismissed the bottom sheet for 4 times before.
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordGenerationBottomSheetDismissCount, 4);
  ASSERT_EQ(
      pref_service()->GetInteger(
          password_manager::prefs::kPasswordGenerationBottomSheetDismissCount),
      4);

  controller()->OnGenerationRequested(PasswordGenerationType::kManual);
  EXPECT_THAT(
      pref_service()->GetInteger(
          password_manager::prefs::kPasswordGenerationBottomSheetDismissCount),
      0);
}

TEST_F(PasswordGenerationControllerTest,
       ShowsBottomSheetWhenManualGenerationRequestedWithFeatureOn) {
  controller()->OnGenerationRequested(PasswordGenerationType::kManual);

  EXPECT_CALL(create_ttf_generation_controller_, Run);
  controller()->ShowManualGenerationDialog(password_manager_driver_.get(),
                                           GetTestGenerationUIData1());
}

TEST_F(PasswordGenerationControllerTest,
       ShowsBottomSheetWhenAutomaticGenerationRequestedWithFeatureOn) {
  controller()->OnAutomaticGenerationAvailable(
      active_driver(), GetTestGenerationUIData1(),
      /*has_saved_credentials=*/false, gfx::RectF(100, 20));

  EXPECT_CALL(create_ttf_generation_controller_, Run);
  controller()->OnGenerationRequested(PasswordGenerationType::kAutomatic);
}
