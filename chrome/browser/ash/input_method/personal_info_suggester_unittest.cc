// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/personal_info_suggester.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;
using ime::AssistiveSuggestionType;
using ::testing::ElementsAre;

class PersonalInfoSuggesterTest : public testing::Test {
 protected:
  PersonalInfoSuggesterTest() {
    autofill_client_.SetPrefs(autofill::test::PrefServiceForTesting());
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    suggestion_handler_ = std::make_unique<FakeSuggestionHandler>();

    personal_data_ = std::make_unique<autofill::TestPersonalDataManager>();
    personal_data_->SetPrefService(autofill_client_.GetPrefs());

    suggester_ = std::make_unique<PersonalInfoSuggester>(
        suggestion_handler_.get(), profile_.get(), personal_data_.get());

    chrome_keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
    chrome_keyboard_controller_client_->set_keyboard_visible_for_test(false);
  }

  void SendKeyboardEvent(const ui::DomCode& code) {
    suggester_->HandleKeyEvent(
        ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, code, ui::EF_NONE,
                     ui::DomKey::NONE, ui::EventTimeForNow()));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FakeSuggestionHandler> suggestion_handler_;
  std::unique_ptr<PersonalInfoSuggester> suggester_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;

  autofill::TestAutofillClient autofill_client_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_;

  const std::u16string email_ = u"johnwayne@me.xyz";
  const std::u16string first_name_ = u"John";
  const std::u16string last_name_ = u"Wayne";
  const std::u16string full_name_ = u"John Wayne";
  const std::u16string address_ = u"1 Dream Road, Hollywood, CA 12345";
  const std::u16string phone_number_ = u"16505678910";
  const int context_id_ = 24601;
};

TEST_F(PersonalInfoSuggesterTest, SuggestsEmail) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->OnFocus(context_id_);
  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), email_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);

  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"My email is: ", gfx::Range(13));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), email_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"hi, my email: ", gfx::Range(14));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), email_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
}  // namespace

TEST_F(PersonalInfoSuggesterTest, SuggestsEmailWithMultilineText) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"\nmy email is ", gfx::Range(13));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), email_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"Hey\nMan\nmy email is ",
                                            gfx::Range(20));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), email_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestWhenPrefixIsntOnLastLine) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"\nmy email is \n",
                                            gfx::Range(14));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"\nmy email is \n ",
                                            gfx::Range(15));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"Hey\nMan\nmy email is \nhey ",
                                            gfx::Range(25));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestWhenContainsCursorSelection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ",
                                            gfx::Range(12, 10));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestWhenStringDoesntEndWithSpace) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is", gfx::Range(11));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestWhenCursorNotEndOfLine) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(11));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, SuggestWhenEndOfLineWhenNewLineExist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is \nBOTTOM TEXT",
                                            gfx::Range(12));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), email_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestEmailWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfoEmail});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestEmailWhenPrefixDoesNotMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is John",
                                            gfx::Range(16));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"our email is: ", gfx::Range(14));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestWhenVirtualKeyboardEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  chrome_keyboard_controller_client_->set_keyboard_visible_for_test(true);
  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest,
       SendsEmailSuggestionToExtensionWhenVirtualKeyboardEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  chrome_keyboard_controller_client_->set_keyboard_visible_for_test(true);
  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_THAT(suggestion_handler_->GetLastOnSuggestionChangedEventSuggestions(),
              ElementsAre(base::UTF16ToUTF8(email_)));
}

TEST_F(PersonalInfoSuggesterTest, SuggestsNames) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoName},
      /*disabled_features=*/{});

  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                              first_name_);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST, last_name_);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, full_name_);
  personal_data_->AddProfile(autofill_profile);
  suggester_->OnFocus(context_id_);

  suggester_->TrySuggestWithSurroundingText(u"my first name is ",
                                            gfx::Range(17));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), first_name_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);

  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my last name is: ",
                                            gfx::Range(17));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), last_name_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my name is ", gfx::Range(11));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), full_name_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);

  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"Hmm... my FULL name: ",
                                            gfx::Range(21));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), full_name_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
}

TEST_F(PersonalInfoSuggesterTest, SuggestsNamesButInsufficientData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoName},
      /*disabled_features=*/{});
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  personal_data_->AddProfile(autofill_profile);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.InsufficientData",
                                      AssistiveType::kPersonalName, 0);

  suggester_->TrySuggestWithSurroundingText(u"my name is ", gfx::Range(11));
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.InsufficientData",
                                      AssistiveType::kPersonalName, 1);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestNamesWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfoName});

  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                              first_name_);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST, last_name_);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, full_name_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->TrySuggestWithSurroundingText(u"my first name is ",
                                            gfx::Range(17));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"my last name is: ",
                                            gfx::Range(17));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"my name is ", gfx::Range(11));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestNamesWhenPrefixDoesNotMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                              first_name_);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST, last_name_);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, full_name_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->TrySuggestWithSurroundingText(u"our first name is ",
                                            gfx::Range(18));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"our last name is: ",
                                            gfx::Range(18));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"our name is ", gfx::Range(12));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"our full name: ", gfx::Range(15));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, SuggestsAddress) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoAddress},
      /*disabled_features=*/{});

  autofill::CountryNames::SetLocaleString("en-US");
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_LINE1,
                              u"1 Dream Road");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_CITY,
                              u"Hollywood");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_ZIP,
                              u"12345");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STATE,
                              u"CA");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_COUNTRY,
                              u"US");
  personal_data_->AddProfile(autofill_profile);
  suggester_->OnFocus(context_id_);

  suggester_->TrySuggestWithSurroundingText(u"my address is ", gfx::Range(14));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), address_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);

  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"our address is: ",
                                            gfx::Range(16));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), address_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my shipping address: ",
                                            gfx::Range(21));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), address_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"our billing address is ",
                                            gfx::Range(23));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), address_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my current address: ",
                                            gfx::Range(20));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), address_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestAddressWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfoAddress});

  autofill::CountryNames::SetLocaleString("en-US");
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_LINE1,
                              u"1 Dream Road");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_CITY,
                              u"Hollywood");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_ZIP,
                              u"12345");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STATE,
                              u"CA");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_COUNTRY,
                              u"US");
  personal_data_->AddProfile(autofill_profile);

  suggester_->TrySuggestWithSurroundingText(u"my address is ", gfx::Range(14));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestAddressWhenPrefixDoesNotMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoAddress},
      /*disabled_features=*/{});

  autofill::CountryNames::SetLocaleString("en-US");
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_LINE1,
                              u"1 Dream Road");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_CITY,
                              u"Hollywood");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_ZIP,
                              u"12345");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STATE,
                              u"CA");
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_COUNTRY,
                              u"US");
  personal_data_->AddProfile(autofill_profile);

  suggester_->TrySuggestWithSurroundingText(u"my address ", gfx::Range(11));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"my last address is: ",
                                            gfx::Range(20));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"our address number is ",
                                            gfx::Range(22));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, SuggestsPhoneNumber) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoPhoneNumber},
      /*disabled_features=*/{});

  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER, phone_number_);
  personal_data_->AddProfile(autofill_profile);
  suggester_->OnFocus(context_id_);

  suggester_->TrySuggestWithSurroundingText(u"my phone number is ",
                                            gfx::Range(19));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), phone_number_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my number is ", gfx::Range(13));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), phone_number_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my mobile number is: ",
                                            gfx::Range(21));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), phone_number_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my number: ", gfx::Range(11));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), phone_number_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my telephone number is ",
                                            gfx::Range(23));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), phone_number_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestPhoneNumberWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfoPhoneNumber});

  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER, phone_number_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->TrySuggestWithSurroundingText(u"my phone number is ",
                                            gfx::Range(20));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest,
       DoesntSuggestPhoneNumberWhenPrefixDoesNotMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoPhoneNumber},
      /*disabled_features=*/{});

  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER, phone_number_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->TrySuggestWithSurroundingText(u"our phone number is ",
                                            gfx::Range(20));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"my number ", gfx::Range(10));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"my number phone is: ",
                                            gfx::Range(20));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  suggester_->TrySuggestWithSurroundingText(u"my phone phone: ",
                                            gfx::Range(16));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, AcceptsSuggestionWithDownEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ENTER);

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  EXPECT_TRUE(suggestion_handler_->GetAcceptedSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, AcceptsSuggestionWithUpEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->Set(kPersonalInfoSuggesterAcceptanceCount, 1);
  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  SendKeyboardEvent(ui::DomCode::ARROW_UP);
  SendKeyboardEvent(ui::DomCode::ENTER);

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  EXPECT_TRUE(suggestion_handler_->GetAcceptedSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DismissesSuggestion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoName},
      /*disabled_features=*/{});

  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, full_name_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->TrySuggestWithSurroundingText(u"my name is ", gfx::Range(11));
  SendKeyboardEvent(ui::DomCode::ESCAPE);
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());

  EXPECT_FALSE(suggestion_handler_->GetAcceptedSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, SuggestsWithConfirmedLength) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoPhoneNumber},
      /*disabled_features=*/{});

  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER, phone_number_);
  personal_data_->AddProfile(autofill_profile);
  suggester_->OnFocus(context_id_);

  suggester_->TrySuggestWithSurroundingText(u"my phone number is ",
                                            gfx::Range(19));
  suggester_->TrySuggestWithSurroundingText(u"my phone number is 16",
                                            gfx::Range(21));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), phone_number_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 2u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
}

TEST_F(PersonalInfoSuggesterTest, AnnouncesSpokenFeedbackWhenChromeVoxIsOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(suggestion_handler_->GetAnnouncements().back(),
            u"Personal info suggested. Press down "
            u"arrow to access; escape to ignore.");

  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ENTER);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(suggestion_handler_->GetAnnouncements().back(),
            u"Suggestion inserted.");

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  task_environment_.FastForwardBy(base::Milliseconds(1500));
  EXPECT_THAT(suggestion_handler_->GetAnnouncements().back(),
              u"Personal info suggested. Press down arrow to access; escape to "
              u"ignore.");

  SendKeyboardEvent(ui::DomCode::ESCAPE);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(suggestion_handler_->GetAnnouncements().back(),
            u"Suggestion dismissed.");
}

TEST_F(PersonalInfoSuggesterTest, DoesntShowAnnotationAfterMaxAcceptanceCount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});
  suggester_->OnFocus(context_id_);

  for (int i = 0; i < kMaxAcceptanceCount; i++) {
    suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
    SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
    SendKeyboardEvent(ui::DomCode::ENTER);
    EXPECT_TRUE(
        suggestion_handler_->GetLastSuggestionDetails().show_accept_annotation);
  }
  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_FALSE(
      suggestion_handler_->GetLastSuggestionDetails().show_accept_annotation);
}

TEST_F(PersonalInfoSuggesterTest, ShowsSettingLink) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});
  suggester_->OnFocus(context_id_);

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->Remove(kPersonalInfoSuggesterShowSettingCount);
  update->Remove(kPersonalInfoSuggesterAcceptanceCount);
  for (int i = 0; i < kMaxShowSettingCount; i++) {
    suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
    // Dismiss suggestion.
    SendKeyboardEvent(ui::DomCode::ESCAPE);
    EXPECT_TRUE(
        suggestion_handler_->GetLastSuggestionDetails().show_setting_link);
  }
  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_FALSE(
      suggestion_handler_->GetLastSuggestionDetails().show_setting_link);
}

TEST_F(PersonalInfoSuggesterTest, DoesntShowSettingLinkAfterAcceptance) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});
  suggester_->OnFocus(context_id_);

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->Set(kPersonalInfoSuggesterShowSettingCount, 0);

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_TRUE(
      suggestion_handler_->GetLastSuggestionDetails().show_setting_link);
  // Accept suggestion.
  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ENTER);
  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_FALSE(
      suggestion_handler_->GetLastSuggestionDetails().show_setting_link);
}

TEST_F(PersonalInfoSuggesterTest, ClicksSettingsWithDownDownEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->Remove(kPersonalInfoSuggesterShowSettingCount);
  update->Remove(kPersonalInfoSuggesterAcceptanceCount);
  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ENTER);

  EXPECT_EQ(suggestion_handler_->GetLastClickedButton(),
            ui::ime::ButtonId::kSmartInputsSettingLink);
}

TEST_F(PersonalInfoSuggesterTest, ClicksSettingsWithUpEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->Remove(kPersonalInfoSuggesterShowSettingCount);
  update->Remove(kPersonalInfoSuggesterAcceptanceCount);
  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  SendKeyboardEvent(ui::DomCode::ARROW_UP);
  SendKeyboardEvent(ui::DomCode::ENTER);

  EXPECT_EQ(suggestion_handler_->GetLastClickedButton(),
            ui::ime::ButtonId::kSmartInputsSettingLink);
}

TEST_F(PersonalInfoSuggesterTest,
       HasSuggestionsReturnsTrueWhenCandidatesAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), email_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  EXPECT_TRUE(suggester_->HasSuggestions());
}

TEST_F(PersonalInfoSuggesterTest,
       HasSuggestionsReturnsFalseWhenCandidatesUnavailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"", gfx::Range(0));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_FALSE(suggester_->HasSuggestions());
}

TEST_F(PersonalInfoSuggesterTest,
       GetsSuggestionsReturnsCandidatesWhenAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), email_);
  EXPECT_EQ(suggestion_handler_->GetConfirmedLength(), 0u);
  EXPECT_EQ(suggestion_handler_->GetContextId(), context_id_);
  EXPECT_EQ(suggester_->GetSuggestions(),
            (std::vector<AssistiveSuggestion>{AssistiveSuggestion{
                .mode = AssistiveSuggestionMode::kPrediction,
                .type = AssistiveSuggestionType::kAssistivePersonalInfo,
                .text = base::UTF16ToUTF8(email_)}}));
}

TEST_F(PersonalInfoSuggesterTest,
       GetsSuggestionsReturnsZeroCandidatesWhenCandidatesUnavailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"", gfx::Range(0));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_TRUE(suggester_->GetSuggestions().empty());
}

TEST_F(PersonalInfoSuggesterTest, AfterBlurDoesNotShowSuggestion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->OnBlur();
  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(11));

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_TRUE(suggester_->GetSuggestions().empty());
}

TEST_F(PersonalInfoSuggesterTest, AfterBlurAcceptSuggestionDoesNotCallHandler) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  suggester_->OnBlur();
  suggester_->AcceptSuggestion();

  EXPECT_FALSE(suggestion_handler_->GetAcceptedSuggestion());
}

TEST_F(PersonalInfoSuggesterTest, DismissSuggestionCallsDismiss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  suggester_->DismissSuggestion();

  EXPECT_TRUE(suggestion_handler_->GetDismissedSuggestion());
}
TEST_F(PersonalInfoSuggesterTest,
       AfterBlurDismissSuggestionDoesNotCallHandler) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  suggester_->OnFocus(context_id_);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", gfx::Range(12));
  suggester_->OnBlur();
  suggester_->DismissSuggestion();

  EXPECT_FALSE(suggestion_handler_->GetDismissedSuggestion());
}
}  // namespace input_method
}  // namespace ash
