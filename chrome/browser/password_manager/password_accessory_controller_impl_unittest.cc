// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/browser/password_manager/password_generation_controller.h"
#include "chrome/browser/password_manager/password_generation_controller_impl.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

namespace {
using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessoryTabType;
using autofill::FooterCommand;
using autofill::PasswordForm;
using autofill::UserInfo;
using autofill::mojom::FocusedFieldType;
using base::ASCIIToUTF16;
using password_manager::CreateEntry;
using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using FillingSource = ManualFillingController::FillingSource;

constexpr char kExampleSite[] = "https://example.com/";
constexpr char kExampleSiteMobile[] = "https://m.example.com/";
constexpr char kExampleDomain[] = "example.com";

class MockPasswordGenerationController
    : public PasswordGenerationControllerImpl {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  explicit MockPasswordGenerationController(content::WebContents* web_contents);

  MOCK_METHOD1(OnGenerationRequested,
               void(autofill::password_generation::PasswordGenerationType));
};

// static
void MockPasswordGenerationController::CreateForWebContents(
    content::WebContents* web_contents) {
  ASSERT_FALSE(FromWebContents(web_contents));
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new MockPasswordGenerationController(web_contents)));
}

MockPasswordGenerationController::MockPasswordGenerationController(
    content::WebContents* web_contents)
    : PasswordGenerationControllerImpl(web_contents) {}

base::string16 password_for_str(const base::string16& user) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, user);
}

base::string16 password_for_str(const std::string& user) {
  return password_for_str(ASCIIToUTF16(user));
}

base::string16 passwords_empty_str(const std::string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE,
      ASCIIToUTF16(domain));
}

base::string16 passwords_title_str(const std::string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE, ASCIIToUTF16(domain));
}

base::string16 no_user_str() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
}

base::string16 manage_passwords_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
}

base::string16 generate_password_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_GENERATE_PASSWORD_BUTTON_TITLE);
}

// Creates a AccessorySheetDataBuilder object with a "Manage passwords..."
// footer.
AccessorySheetData::Builder PasswordAccessorySheetDataBuilder(
    const base::string16& title) {
  return AccessorySheetData::Builder(AccessoryTabType::PASSWORDS, title)
      .AppendFooterCommand(manage_passwords_str(),
                           autofill::AccessoryAction::MANAGE_PASSWORDS);
}

}  // namespace

class PasswordAccessoryControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordAccessoryControllerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    ASSERT_TRUE(web_contents()->GetFocusedFrame());
    ASSERT_EQ(url::Origin::Create(GURL(kExampleSite)),
              web_contents()->GetFocusedFrame()->GetLastCommittedOrigin());

    MockPasswordGenerationController::CreateForWebContents(web_contents());
    PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), cache(), mock_manual_filling_controller_.AsWeakPtr());
    NavigateAndCommit(GURL(kExampleSite));
  }

  PasswordAccessoryController* controller() {
    return PasswordAccessoryControllerImpl::FromWebContents(web_contents());
  }

  password_manager::CredentialCache* cache() { return &credential_cache_; }

 protected:
  StrictMock<MockManualFillingController> mock_manual_filling_controller_;

 private:
  password_manager::CredentialCache credential_cache_;
};

TEST_F(PasswordAccessoryControllerTest, IsNotRecreatedForSameWebContents) {
  PasswordAccessoryControllerImpl* initial_controller =
      PasswordAccessoryControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  PasswordAccessoryControllerImpl::CreateForWebContents(web_contents(),
                                                        cache());
  EXPECT_EQ(PasswordAccessoryControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(PasswordAccessoryControllerTest, TransformsMatchesToSuggestions) {
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           true)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, HintsToEmptyUserNames) {
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("", "S3cur3", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(no_user_str(), no_user_str(), false, false)
              .AppendField(ASCIIToUTF16("S3cur3"),
                           password_for_str(no_user_str()), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, SortsAlphabeticalDuringTransform) {
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleDomain), false).first,
       CreateEntry("Zebra", "M3h", GURL(kExampleDomain), false).first,
       CreateEntry("Alf", "PWD", GURL(kExampleDomain), false).first,
       CreateEntry("Cat", "M1@u", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));

  AccessorySheetData result(AccessoryTabType::PASSWORDS, base::string16());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(
      result,
      PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
          .AddUserInfo()
          .AppendField(ASCIIToUTF16("Alf"), ASCIIToUTF16("Alf"), false, true)
          .AppendField(ASCIIToUTF16("PWD"), password_for_str("Alf"), true,
                       false)
          .AddUserInfo()
          .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false, true)
          .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"), true,
                       false)
          .AddUserInfo()
          .AppendField(ASCIIToUTF16("Cat"), ASCIIToUTF16("Cat"), false, true)
          .AppendField(ASCIIToUTF16("M1@u"), password_for_str("Cat"), true,
                       false)
          .AddUserInfo()
          .AppendField(ASCIIToUTF16("Zebra"), ASCIIToUTF16("Zebra"), false,
                       true)
          .AppendField(ASCIIToUTF16("M3h"), password_for_str("Zebra"), true,
                       false)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, RepeatsSuggestionsForSameFrame) {
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));

  // Pretend that any input in the same frame was focused.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           true)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, ProvidesEmptySuggestionsMessage) {
  cache()->SaveCredentialsForOrigin({},
                                    url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestions(PasswordAccessorySheetDataBuilder(
                                     passwords_empty_str(kExampleDomain))
                                     .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, PasswordFieldChangesSuggestionType) {
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));
  // Pretend a username field was focused. This should result in non-interactive
  // suggestion.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           true)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  // Pretend that we focus a password field now: By triggering a refresh with
  // |is_password_field| set to true, all suggestions should become interactive.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, true)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, CachesIsReplacedByNewPasswords) {
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           true)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Alf", "M3lm4k", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Alf"), ASCIIToUTF16("Alf"), false,
                           true)
              .AppendField(ASCIIToUTF16("M3lm4k"), password_for_str("Alf"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, HidesEntriesForPSLMatchedOriginsInV1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      autofill::features::kAutofillKeyboardAccessory);
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false).first,
       CreateEntry("Alf", "R4nd0m", GURL(kExampleSiteMobile), true).first},
      url::Origin::Create(GURL(kExampleSite)));

  AccessorySheetData result(AccessoryTabType::PASSWORDS, base::string16());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(
      result,
      PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
          .AddUserInfo()
          .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"),
                       /*is_obfuscated=*/false, /*selectable=*/true)
          .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                       /*is_obfuscated=*/true, /*selectable=*/false)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, SetsTitleForPSLMatchedOriginsInV2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillKeyboardAccessory);
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false).first,
       CreateEntry("Alf", "R4nd0m", GURL(kExampleSiteMobile), true).first},
      url::Origin::Create(GURL(kExampleSite)));

  AccessorySheetData result(AccessoryTabType::PASSWORDS, base::string16());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(
      result,
      PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
          .AddUserInfo()
          .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"),
                       /*is_obfuscated=*/false, /*selectable=*/true)
          .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                       /*is_obfuscated=*/true, /*selectable=*/false)
          .AddUserInfo(kExampleSiteMobile)
          .AppendField(ASCIIToUTF16("Alf"), ASCIIToUTF16("Alf"),
                       /*is_obfuscated=*/false, /*selectable=*/true)
          .AppendField(ASCIIToUTF16("R4nd0m"), password_for_str("Alf"),
                       /*is_obfuscated=*/true, /*selectable=*/false)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, UnfillableFieldClearsSuggestions) {
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));
  // Pretend a username field was focused. This should result in non-emtpy
  // suggestions.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           true)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  // Pretend that the focus was lost or moved to an unfillable field. Now, only
  // the empty state message should be sent.
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestions(PasswordAccessorySheetDataBuilder(
                                     passwords_empty_str(kExampleDomain))
                                     .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kUnfillableElement,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, NavigatingMainFrameClearsSuggestions) {
  // Set any, non-empty password list and pretend a username field was focused.
  // This should result in non-emtpy suggestions.
  cache()->SaveCredentialsForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleDomain), false).first},
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           true)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  // Pretend that the focus was lost or moved to an unfillable field.
  NavigateAndCommit(GURL("https://random.other-site.org/"));

  // Now, only the empty state message should be sent.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(PasswordAccessorySheetDataBuilder(
                             passwords_empty_str("random.other-site.org"))
                             .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kUnfillableElement,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, OnAutomaticGenerationRequested) {
  MockPasswordGenerationController* mock_pwd_generation_controller =
      static_cast<MockPasswordGenerationController*>(
          PasswordGenerationController::GetIfExisting(web_contents()));
  EXPECT_CALL(
      *mock_pwd_generation_controller,
      OnGenerationRequested(
          autofill::password_generation::PasswordGenerationType::kAutomatic));
  controller()->OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType::kAutomatic);
}

TEST_F(PasswordAccessoryControllerTest, AddsGenerationCommandWhenAvailable) {
  cache()->SaveCredentialsForOrigin({},
                                    url::Origin::Create(GURL(kExampleSite)));
  AccessorySheetData::Builder data_builder(AccessoryTabType::PASSWORDS,
                                           passwords_empty_str(kExampleDomain));
  data_builder
      .AppendFooterCommand(generate_password_str(),
                           autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL)
      .AppendFooterCommand(manage_passwords_str(),
                           autofill::AccessoryAction::MANAGE_PASSWORDS);
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestions(std::move(data_builder).Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/true);
}

TEST_F(PasswordAccessoryControllerTest, NoGenerationCommandIfNotPasswordField) {
  cache()->SaveCredentialsForOrigin({},
                                    url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestions(PasswordAccessorySheetDataBuilder(
                                     passwords_empty_str(kExampleDomain))
                                     .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/true);
}

TEST_F(PasswordAccessoryControllerTest, OnManualGenerationRequested) {
  MockPasswordGenerationController* mock_pwd_generation_controller =
      static_cast<MockPasswordGenerationController*>(
          PasswordGenerationController::GetIfExisting(web_contents()));
  EXPECT_CALL(mock_manual_filling_controller_, Hide());
  EXPECT_CALL(
      *mock_pwd_generation_controller,
      OnGenerationRequested(
          autofill::password_generation::PasswordGenerationType::kManual));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}
