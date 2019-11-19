// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_address_accessory_controller.h"
#include "chrome/browser/autofill/mock_credit_card_accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/autofill/mock_password_accessory_controller.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/browser/password_manager/touch_to_fill_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessoryTabType;
using autofill::mojom::FocusedFieldType;
using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::StrictMock;
using testing::WithArgs;
using FillingSource = ManualFillingController::FillingSource;

constexpr char kExampleSite[] = "https://example.com";
constexpr int kIconSize = 75;  // An example size for favicons (=> 3.5*20px).

AccessorySheetData empty_passwords_sheet() {
  constexpr char kTitle[] = "Example title";
  return AccessorySheetData(AccessoryTabType::PASSWORDS,
                            base::ASCIIToUTF16(kTitle));
}

AccessorySheetData populate_sheet(AccessoryTabType type) {
  constexpr char kTitle[] = "Suggestions available!";
  return AccessorySheetData::Builder(type, base::ASCIIToUTF16(kTitle))
      .AddUserInfo()
      .Build();
}

}  // namespace

class ManualFillingControllerTest : public testing::Test {
 public:
  ManualFillingControllerTest() = default;

  void SetUp() override {
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), favicon_service(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(), mock_cc_controller_.AsWeakPtr(),
        std::make_unique<NiceMock<MockManualFillingView>>());
  }

  void FocusFieldAndClearExpectations(FocusedFieldType fieldType) {
    // Depending on |fieldType|, different calls can be expected. All of them
    // are irrelevant during setup.
    controller()->NotifyFocusedInputChanged(fieldType);
    testing::Mock::VerifyAndClearExpectations(view());
  }

  void SetSuggestionsAndClearExpectations(AccessorySheetData sheet) {
    // Depending on |sheet| and last set field type, different calls can be
    // expected. All of them are irrelevant during setup.
    controller()->RefreshSuggestions(std::move(sheet));
    testing::Mock::VerifyAndClearExpectations(view());
  }

  ManualFillingControllerImpl* controller() {
    return ManualFillingControllerImpl::FromWebContents(web_contents());
  }

  favicon::MockFaviconService* favicon_service() {
    return mock_favicon_service_.get();
  }

  content::WebContents* web_contents() { return web_contents_; }

  MockManualFillingView* view() {
    return static_cast<MockManualFillingView*>(controller()->view());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  content::WebContents* web_contents_ =
      web_contents_factory_.CreateWebContents(&profile_);

  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockCreditCardAccessoryController> mock_cc_controller_;

  std::unique_ptr<StrictMock<favicon::MockFaviconService>>
      mock_favicon_service_ =
          std::make_unique<StrictMock<favicon::MockFaviconService>>();
};

TEST_F(ManualFillingControllerTest, IsNotRecreatedForSameWebContents) {
  ManualFillingController* initial_controller =
      ManualFillingControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  ManualFillingControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(ManualFillingControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(ManualFillingControllerTest, ClosesSheetWhenFocusingUnfillableField) {
  SetSuggestionsAndClearExpectations(
      populate_sheet(AccessoryTabType::PASSWORDS));

  EXPECT_CALL(*view(), CloseAccessorySheet());
  EXPECT_CALL(*view(), SwapSheetWithKeyboard()).Times(0);
  controller()->NotifyFocusedInputChanged(FocusedFieldType::kUnfillableElement);
}

TEST_F(ManualFillingControllerTest, ClosesSheetWhenFocusingFillableField) {
  SetSuggestionsAndClearExpectations(
      populate_sheet(AccessoryTabType::PASSWORDS));

  EXPECT_CALL(*view(), CloseAccessorySheet()).Times(0);
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->NotifyFocusedInputChanged(
      FocusedFieldType::kFillablePasswordField);
}

TEST_F(ManualFillingControllerTest, ClosesSheetWhenFocusingSearchField) {
  SetSuggestionsAndClearExpectations(
      populate_sheet(AccessoryTabType::PASSWORDS));

  EXPECT_CALL(*view(), CloseAccessorySheet());
  EXPECT_CALL(*view(), SwapSheetWithKeyboard()).Times(0);
  controller()->NotifyFocusedInputChanged(
      FocusedFieldType::kFillableSearchField);
}

TEST_F(ManualFillingControllerTest, ClosesSheetWhenFocusingTextArea) {
  SetSuggestionsAndClearExpectations(
      populate_sheet(AccessoryTabType::PASSWORDS));

  EXPECT_CALL(*view(), CloseAccessorySheet());
  EXPECT_CALL(*view(), SwapSheetWithKeyboard()).Times(0);
  controller()->NotifyFocusedInputChanged(FocusedFieldType::kFillableTextArea);
}

TEST_F(ManualFillingControllerTest, AlwaysShowsAccessoryForPasswordFields) {
  controller()->RefreshSuggestions(empty_passwords_sheet());

  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible());
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);
}

TEST_F(ManualFillingControllerTest,
       HidesAccessoryWithoutSuggestionsOnNonPasswordFields) {
  SetSuggestionsAndClearExpectations(
      populate_sheet(AccessoryTabType::PASSWORDS));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillKeyboardAccessory);
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), Hide());
  controller()->RefreshSuggestions(empty_passwords_sheet());
}

TEST_F(ManualFillingControllerTest, ShowsAccessoryWithSuggestions) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible());
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));
}

TEST_F(ManualFillingControllerTest, DoesntShowFallbacksOutsideUsernameInV1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          autofill::features::kAutofillKeyboardAccessory,
          autofill::features::kAutofillManualFallbackAndroid});
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Hide());
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));
}

TEST_F(ManualFillingControllerTest, ShowsFallbacksOutsideUsernameInV2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillKeyboardAccessory);
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible());
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));
}

// TODO(fhorschig): Check for recorded metrics here or similar to this.
TEST_F(ManualFillingControllerTest, ShowsAccessoryWhenRefreshingSuggestions) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible());
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));
}

TEST_F(ManualFillingControllerTest, ShowsAndHidesAccessoryForPasswords) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible());
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);

  EXPECT_CALL(*view(), Hide());
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/false);
}

TEST_F(ManualFillingControllerTest, UpdatesCreditCardControllerOnFocusChange) {
  EXPECT_CALL(mock_cc_controller_, RefreshSuggestions);
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);
}

TEST_F(ManualFillingControllerTest, HidesAccessoryWithoutAvailableSources) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillKeyboardAccessory);
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible()).Times(2);
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
  // This duplicate call is a noop.
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  testing::Mock::VerifyAndClearExpectations(view());

  // Hiding just one of two active filling sources won't have any effect at all.
  EXPECT_CALL(*view(), Hide()).Times(0);
  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible()).Times(0);
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/false);
  testing::Mock::VerifyAndClearExpectations(view());

  // Hiding the remaining second source will result in the view being hidden.
  EXPECT_CALL(*view(), Hide()).Times(1);
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/false);
}

TEST_F(ManualFillingControllerTest, OnAutomaticGenerationStatusChanged) {
  EXPECT_CALL(*view(), OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationStatusChanged(true);

  EXPECT_CALL(*view(), OnAutomaticGenerationStatusChanged(false));
  controller()->OnAutomaticGenerationStatusChanged(false);
}

TEST_F(ManualFillingControllerTest, OnFillingTriggeredFillsAndClosesSheet) {
  const char kTextToFill[] = "TextToFill";
  const base::string16 text_to_fill(base::ASCIIToUTF16(kTextToFill));
  const autofill::UserInfo::Field field(text_to_fill, text_to_fill, false,
                                        true);

  EXPECT_CALL(mock_pwd_controller_, OnFillingTriggered(field));
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->OnFillingTriggered(AccessoryTabType::PASSWORDS, field);
}

TEST_F(ManualFillingControllerTest, ForwardsPasswordManagingToController) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_PASSWORDS));
  controller()->OnOptionSelected(AccessoryAction::MANAGE_PASSWORDS);
}

TEST_F(ManualFillingControllerTest, ForwardsPasswordGenerationToController) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}

TEST_F(ManualFillingControllerTest, ForwardsAddressManagingToController) {
  EXPECT_CALL(mock_address_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_ADDRESSES));
  controller()->OnOptionSelected(AccessoryAction::MANAGE_ADDRESSES);
}

TEST_F(ManualFillingControllerTest, ForwardsCreditCardManagingToController) {
  EXPECT_CALL(mock_cc_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS));
  controller()->OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS);
}

TEST_F(ManualFillingControllerTest, OnAutomaticGenerationRequested) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_AUTOMATIC);
}

TEST_F(ManualFillingControllerTest, OnManualGenerationRequested) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}

TEST_F(ManualFillingControllerTest, RequestsFaviconForOrigin) {
  base::MockCallback<ManualFillingController::IconCallback> mock_callback;

  EXPECT_CALL(*favicon_service(), GetRawFaviconForPageURL(GURL(kExampleSite), _,
                                                          kIconSize, _, _, _))
      .WillOnce(
          WithArgs<4, 5>([](favicon_base::FaviconRawBitmapCallback callback,
                            base::CancelableTaskTracker* tracker) {
            return tracker->PostTask(
                base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
                base::BindOnce(std::move(callback),
                               favicon_base::FaviconRawBitmapResult()));
          }));
  EXPECT_CALL(mock_callback, Run);
  controller()->GetFavicon(kIconSize, kExampleSite, mock_callback.Get());

  base::RunLoop().RunUntilIdle();
}
