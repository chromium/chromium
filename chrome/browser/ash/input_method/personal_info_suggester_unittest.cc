// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/personal_info_suggester.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using ime::TextSuggestion;
using ime::TextSuggestionMode;
using ime::TextSuggestionType;

// TODO(crbug/1201529): Update this unit test to use `FakeSuggestionHandler`
// instead.
class TestSuggestionHandler : public SuggestionHandlerInterface {
 public:
  bool DismissSuggestion(int context_id, std::string* error) override {
    suggestion_text_ = base::EmptyString16();
    previous_suggestions_.clear();
    confirmed_length_ = 0;
    suggestion_accepted_ = false;
    return true;
  }

  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override {
    suggestion_text_ = details.text;
    confirmed_length_ = details.confirmed_length;
    show_accept_annotation_ = details.show_accept_annotation;
    show_setting_link_ = details.show_setting_link;
    return true;
  }

  bool AcceptSuggestion(int context_id, std::string* error) override {
    suggestion_text_ = base::EmptyString16();
    confirmed_length_ = 0;
    suggestion_accepted_ = true;
    return true;
  }

  void VerifySuggestion(const std::u16string text,
                        const size_t confirmed_length) {
    EXPECT_EQ(suggestion_text_, text);
    EXPECT_EQ(confirmed_length_, confirmed_length);
    suggestion_text_ = base::EmptyString16();
    confirmed_length_ = 0;
  }

  void VerifySuggestionDispatchedToExtension(
      const std::vector<std::string>& suggestions) {
    EXPECT_THAT(previous_suggestions_, testing::ContainerEq(suggestions));
  }

  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {
    previous_suggestions_ = suggestions;
  }

  void ClickButton(const ui::ime::AssistiveWindowButton& button) override {
    button_clicked_ = button.id;
  }

  bool SetButtonHighlighted(int context_id,
                            const ui::ime::AssistiveWindowButton& button,
                            bool highlighted,
                            std::string* error) override {
    return false;
  }

  bool AcceptSuggestionCandidate(int context_id,
                                 const std::u16string& candidate,
                                 std::string* error) override {
    return false;
  }

  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override {
    return false;
  }

  void VerifyShowAnnotation(const bool show_accept_annotation) {
    EXPECT_EQ(show_accept_annotation_, show_accept_annotation);
  }
  void VerifyShowSettingLink(const bool show_setting_link) {
    EXPECT_EQ(show_setting_link_, show_setting_link);
  }
  void VerifyButtonClicked(const ui::ime::ButtonId id) {
    EXPECT_EQ(button_clicked_, id);
  }

  bool IsSuggestionAccepted() { return suggestion_accepted_; }

  void Announce(const std::u16string& message) override {
    announced_text_ = message;
  }

  void VerifyAnnouncement(const std::u16string& expected_text) {
    EXPECT_EQ(announced_text_, expected_text);
  }

 private:
  std::u16string suggestion_text_;
  size_t confirmed_length_ = 0;
  bool show_accept_annotation_ = false;
  bool show_setting_link_ = false;
  bool suggestion_accepted_ = false;
  ui::ime::ButtonId button_clicked_ = ui::ime::ButtonId::kNone;
  std::vector<std::string> previous_suggestions_;
  std::u16string announced_text_ = u"";
};

}  // namespace

class PersonalInfoSuggesterTest : public testing::Test {
 protected:
  PersonalInfoSuggesterTest() {
    autofill_client_.SetPrefs(autofill::test::PrefServiceForTesting());
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    suggestion_handler_ = std::make_unique<TestSuggestionHandler>();

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
  std::unique_ptr<TestSuggestionHandler> suggestion_handler_;
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
};

TEST_F(PersonalInfoSuggesterTest, SuggestsEmail) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifySuggestion(email_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"My email is: ", 13);
  suggestion_handler_->VerifySuggestion(email_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"hi, my email: ", 14);
  suggestion_handler_->VerifySuggestion(email_, 0);
}

TEST_F(PersonalInfoSuggesterTest, SuggestsEmailWithMultilineText) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"\nmy email is ", 13);
  suggestion_handler_->VerifySuggestion(email_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"Hey\nMan\nmy email is ", 20);
  suggestion_handler_->VerifySuggestion(email_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestWhenPrefixIsntOnLastLine) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"\nmy email is \n", 14);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"\nmy email is \n ", 15);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"Hey\nMan\nmy email is \nhey ",
                                            25);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestEmailWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfoEmail});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestEmailWhenPrefixDoesNotMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is John", 16);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"our email is: ", 14);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
}

TEST_F(PersonalInfoSuggesterTest, DoesntSuggestWhenVirtualKeyboardEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  chrome_keyboard_controller_client_->set_keyboard_visible_for_test(true);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
}

TEST_F(PersonalInfoSuggesterTest,
       SendsEmailSuggestionToExtensionWhenVirtualKeyboardEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  chrome_keyboard_controller_client_->set_keyboard_visible_for_test(true);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifySuggestionDispatchedToExtension(
      std::vector<std::string>{base::UTF16ToUTF8(email_)});
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

  suggester_->TrySuggestWithSurroundingText(u"my first name is ", 17);
  suggestion_handler_->VerifySuggestion(first_name_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my last name is: ", 17);
  suggestion_handler_->VerifySuggestion(last_name_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my name is ", 12);
  suggestion_handler_->VerifySuggestion(full_name_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"Hmm... my FULL name: ", 21);
  suggestion_handler_->VerifySuggestion(full_name_, 0);
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

  suggester_->TrySuggestWithSurroundingText(u"my name is ", 12);
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

  suggester_->TrySuggestWithSurroundingText(u"my first name is ", 17);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"my last name is: ", 17);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"my name is ", 12);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
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

  suggester_->TrySuggestWithSurroundingText(u"our first name is ", 18);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"our last name is: ", 18);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"our name is ", 12);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"our full name: ", 15);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
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

  suggester_->TrySuggestWithSurroundingText(u"my address is ", 14);
  suggestion_handler_->VerifySuggestion(address_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"our address is: ", 16);
  suggestion_handler_->VerifySuggestion(address_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my shipping address: ", 21);
  suggestion_handler_->VerifySuggestion(address_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"our billing address is ", 23);
  suggestion_handler_->VerifySuggestion(address_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my current address: ", 20);
  suggestion_handler_->VerifySuggestion(address_, 0);
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

  suggester_->TrySuggestWithSurroundingText(u"my address is ", 14);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
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

  suggester_->TrySuggestWithSurroundingText(u"my address ", 11);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"my last address is: ", 20);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"our address number is ", 22);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
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

  suggester_->TrySuggestWithSurroundingText(u"my phone number is ", 19);
  suggestion_handler_->VerifySuggestion(phone_number_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my number is ", 13);
  suggestion_handler_->VerifySuggestion(phone_number_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my mobile number is: ", 21);
  suggestion_handler_->VerifySuggestion(phone_number_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my number: ", 11);
  suggestion_handler_->VerifySuggestion(phone_number_, 0);
  SendKeyboardEvent(ui::DomCode::ESCAPE);

  suggester_->TrySuggestWithSurroundingText(u"my telephone number is ", 23);
  suggestion_handler_->VerifySuggestion(phone_number_, 0);
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

  suggester_->TrySuggestWithSurroundingText(u"my phone number is ", 20);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
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

  suggester_->TrySuggestWithSurroundingText(u"our phone number is ", 20);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"my number ", 10);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"my number phone is: ", 20);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);

  suggester_->TrySuggestWithSurroundingText(u"my phone phone: ", 16);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
}

TEST_F(PersonalInfoSuggesterTest, AcceptsSuggestionWithDownEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ENTER);

  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
  EXPECT_TRUE(suggestion_handler_->IsSuggestionAccepted());
}

TEST_F(PersonalInfoSuggesterTest, AcceptsSuggestionWithUpEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->SetIntKey(kPersonalInfoSuggesterAcceptanceCount, 1);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  SendKeyboardEvent(ui::DomCode::ARROW_UP);
  SendKeyboardEvent(ui::DomCode::ENTER);

  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
  EXPECT_TRUE(suggestion_handler_->IsSuggestionAccepted());
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

  suggester_->TrySuggestWithSurroundingText(u"my name is ", 11);
  SendKeyboardEvent(ui::DomCode::ESCAPE);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
  EXPECT_FALSE(suggestion_handler_->IsSuggestionAccepted());
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

  suggester_->TrySuggestWithSurroundingText(u"my phone number is ", 19);
  suggester_->TrySuggestWithSurroundingText(u"my phone number is 16", 21);
  suggestion_handler_->VerifySuggestion(phone_number_, 2);
}

TEST_F(PersonalInfoSuggesterTest, AnnouncesSpokenFeedbackWhenChromeVoxIsOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  suggestion_handler_->VerifyAnnouncement(
      u"Personal info suggested. Press down arrow to access; escape to "
      u"ignore.");

  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ENTER);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  suggestion_handler_->VerifyAnnouncement(u"Suggestion inserted.");

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  task_environment_.FastForwardBy(base::Milliseconds(1500));
  suggestion_handler_->VerifyAnnouncement(
      u"Personal info suggested. Press down arrow to access; escape to "
      u"ignore.");
  SendKeyboardEvent(ui::DomCode::ESCAPE);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  suggestion_handler_->VerifyAnnouncement(u"Suggestion dismissed.");
}

TEST_F(PersonalInfoSuggesterTest, DoesntShowAnnotationAfterMaxAcceptanceCount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  for (int i = 0; i < kMaxAcceptanceCount; i++) {
    suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
    SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
    SendKeyboardEvent(ui::DomCode::ENTER);
    suggestion_handler_->VerifyShowAnnotation(true);
  }
  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifyShowAnnotation(false);
}

TEST_F(PersonalInfoSuggesterTest, ShowsSettingLink) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->RemoveKey(kPersonalInfoSuggesterShowSettingCount);
  update->RemoveKey(kPersonalInfoSuggesterAcceptanceCount);
  for (int i = 0; i < kMaxShowSettingCount; i++) {
    suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
    // Dismiss suggestion.
    SendKeyboardEvent(ui::DomCode::ESCAPE);
    suggestion_handler_->VerifyShowSettingLink(true);
  }
  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifyShowSettingLink(false);
}

TEST_F(PersonalInfoSuggesterTest, DoesntShowSettingLinkAfterAcceptance) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->SetIntKey(kPersonalInfoSuggesterShowSettingCount, 0);
  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifyShowSettingLink(true);
  // Accept suggestion.
  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ENTER);
  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifyShowSettingLink(false);
}

TEST_F(PersonalInfoSuggesterTest, ClicksSettingsWithDownDownEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->RemoveKey(kPersonalInfoSuggesterShowSettingCount);
  update->RemoveKey(kPersonalInfoSuggesterAcceptanceCount);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ARROW_DOWN);
  SendKeyboardEvent(ui::DomCode::ENTER);

  suggestion_handler_->VerifyButtonClicked(
      ui::ime::ButtonId::kSmartInputsSettingLink);
}

TEST_F(PersonalInfoSuggesterTest, ClicksSettingsWithUpEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->RemoveKey(kPersonalInfoSuggesterShowSettingCount);
  update->RemoveKey(kPersonalInfoSuggesterAcceptanceCount);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  SendKeyboardEvent(ui::DomCode::ARROW_UP);
  SendKeyboardEvent(ui::DomCode::ENTER);

  suggestion_handler_->VerifyButtonClicked(
      ui::ime::ButtonId::kSmartInputsSettingLink);
}

TEST_F(PersonalInfoSuggesterTest,
       HasSuggestionsReturnsTrueWhenCandidatesAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifySuggestion(email_, 0);
  EXPECT_TRUE(suggester_->HasSuggestions());
}

TEST_F(PersonalInfoSuggesterTest,
       HasSuggestionsReturnsFalseWhenCandidatesUnavailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"", 0);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
  EXPECT_FALSE(suggester_->HasSuggestions());
}

TEST_F(PersonalInfoSuggesterTest,
       GetsSuggestionsReturnsCandidatesWhenAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"my email is ", 12);
  suggestion_handler_->VerifySuggestion(email_, 0);
  EXPECT_EQ(suggester_->GetSuggestions(),
            (std::vector<TextSuggestion>{TextSuggestion{
                .mode = TextSuggestionMode::kPrediction,
                .type = TextSuggestionType::kAssistivePersonalInfo,
                .text = base::UTF16ToUTF8(email_)}}));
}

TEST_F(PersonalInfoSuggesterTest,
       GetsSuggestionsReturnsZeroCandidatesWhenCandidatesUnavailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfoEmail},
      /*disabled_features=*/{});

  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->TrySuggestWithSurroundingText(u"", 0);
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
  EXPECT_TRUE(suggester_->GetSuggestions().empty());
}

}  // namespace input_method
}  // namespace ash
