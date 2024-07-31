// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/mock_sharing_service.h"
#include "components/sharing_message/pref_names.h"
#include "components/sharing_message/sharing_fcm_handler.h"
#include "components/sharing_message/sharing_fcm_sender.h"
#include "components/sharing_message/sharing_handler_registry.h"
#include "components/sharing_message/sharing_service.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sharing_message/vapid_key_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

using SharingMessage = components_sharing_message::SharingMessage;

namespace {

const char kEmptyTelUrl[] = "tel:";
const char kTelUrl[] = "tel:+9876543210";
const char kNonTelUrl[] = "https://google.com";

const char kSelectionTextWithNumber[] = "9876543210";

class ClickToCallUtilsTest : public testing::Test {
 public:
  ClickToCallUtilsTest() = default;

  ClickToCallUtilsTest(const ClickToCallUtilsTest&) = delete;
  ClickToCallUtilsTest& operator=(const ClickToCallUtilsTest&) = delete;

  ~ClickToCallUtilsTest() override = default;

  void SetUp() override {
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&ClickToCallUtilsTest::CreateService,
                                       base::Unretained(this)));
  }

  void ExpectClickToCallDisabledForSelectionText(
      const std::string& selection_text,
      bool use_incognito_profile = false) {
    Profile* profile_to_use =
        use_incognito_profile
            ? profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)
            : &profile_;
    std::optional<std::string> phone_number =
        ExtractPhoneNumberForClickToCall(profile_to_use, selection_text);
    EXPECT_FALSE(phone_number.has_value())
        << " Found phone number: " << phone_number.value()
        << " in selection text: " << selection_text;
  }

 protected:
  base::test::ScopedFeatureList features_{kClickToCall};

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    return create_service_ ? std::make_unique<MockSharingService>() : nullptr;
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  bool create_service_ = true;
};

}  // namespace

TEST_F(ClickToCallUtilsTest, NoSharingService_DoNotOfferAnyMenu) {
  create_service_ = false;
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kTelUrl)));
  ExpectClickToCallDisabledForSelectionText(kSelectionTextWithNumber);
}

TEST_F(ClickToCallUtilsTest, PolicyDisabled_DoNotOfferAnyMenu) {
  profile_.GetPrefs()->SetBoolean(prefs::kClickToCallEnabled, false);
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kTelUrl)));
  ExpectClickToCallDisabledForSelectionText(kSelectionTextWithNumber);
}

TEST_F(ClickToCallUtilsTest, IncognitoProfile_DoNotOfferAnyMenu) {
  EXPECT_FALSE(ShouldOfferClickToCallForURL(
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true), GURL(kTelUrl)));
  ExpectClickToCallDisabledForSelectionText(kSelectionTextWithNumber,
                                            /*use_incognito_profile =*/true);
}

TEST_F(ClickToCallUtilsTest, EmptyTelLink_DoNotOfferForLink) {
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kEmptyTelUrl)));
}

TEST_F(ClickToCallUtilsTest, TelLink_OfferForLink) {
  EXPECT_TRUE(ShouldOfferClickToCallForURL(&profile_, GURL(kTelUrl)));
}

TEST_F(ClickToCallUtilsTest, NonTelLink_DoNotOfferForLink) {
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kNonTelUrl)));
}

TEST_F(ClickToCallUtilsTest, TelLinkWithFragment) {
  GURL fragment("tel:123#456");
  EXPECT_TRUE(ShouldOfferClickToCallForURL(&profile_, fragment));
  EXPECT_EQ("123", fragment.GetContent());
}

TEST_F(ClickToCallUtilsTest, TelLinkWithEncodedCharacters) {
  // %23 == '#'
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL("tel:123%23456")));
  // %2A == '*'
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL("tel:123%2A456")));
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL("tel:123*456")));
  // %25 == '%'
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL("tel:123%25456")));

  // %2B == '+'
  EXPECT_TRUE(ShouldOfferClickToCallForURL(&profile_, GURL("tel:%2B44123")));
  EXPECT_TRUE(ShouldOfferClickToCallForURL(&profile_, GURL("tel:+44123")));
}

TEST_F(ClickToCallUtilsTest,
       SelectionText_ValidPhoneNumberRegex_OfferForSelection) {
  // Stores a mapping of selected text to expected phone number parsed.
  std::map<std::string, std::string> expectations;
  // Selection text only consists of the phone number.
  expectations.emplace("9876543210", "9876543210");
  // Check for phone number at end of text.
  expectations.emplace("Call on 9876543210", "9876543210");
  // Check for international number with a space between code and phone number.
  expectations.emplace("Call +44 9876543210 now", "+44 9876543210");
  // Check for international number without spacing.
  expectations.emplace("call (+44)9876543210 now", "(+44)9876543210");
  // Check for dashes.
  expectations.emplace("(+44)987-654-3210 now", "(+44)987-654-3210");
  // Check for spaces and dashes.
  expectations.emplace("call (+44) 987 654-3210 now", "(+44) 987 654-3210");
  // The first number is always returned.
  expectations.emplace("9876543210 and 12", "9876543210");
  // Spaces are allowed in between numbers.
  expectations.emplace("9 8 7 6 5 4 3 2 1 0", "9 8 7 6 5 4 3 2 1 0");
  // Two spaces in between.
  expectations.emplace("9  8 7 6 5  4 3 2 1 0", "9  8 7 6 5  4 3 2 1 0");
  // Non breaking spaces around number.
  expectations.emplace("\u00A09876543210\u00A0", "9876543210");
  // Chrome version string.
  expectations.emplace("78.0.3904.108", "78.0.3904.108");

  for (auto& expectation : expectations) {
    std::optional<std::string> phone_number =
        ExtractPhoneNumberForClickToCall(&profile_, expectation.first);
    ASSERT_NE(std::nullopt, phone_number);
    EXPECT_EQ(expectation.second, phone_number.value());
  }
}

TEST_F(ClickToCallUtilsTest,
       SelectionText_InvalidPhoneNumberRegex_DoNotOfferForSelection) {
  std::vector<std::string> invalid_selection_texts;

  // Does not contain any number.
  invalid_selection_texts.push_back("Call me maybe");
  // Although this is a valid number, its not caught by the regex.
  invalid_selection_texts.push_back("+44 1800-FLOWERS");
  // Number does not start as new word.
  invalid_selection_texts.push_back("No space9876543210");
  // Minimum length for regex match not satisfied.
  invalid_selection_texts.push_back("Small number 98765");
  // Number does not start as new word.
  invalid_selection_texts.push_back("Buy for $9876543210");
  // More than two spaces in between.
  invalid_selection_texts.push_back("9   8   7   6   5   4    3   2");
  // Space dash space formatting.
  invalid_selection_texts.push_back("999 - 999 - 9999");

  for (auto& text : invalid_selection_texts)
    ExpectClickToCallDisabledForSelectionText(text);
}

TEST_F(ClickToCallUtilsTest, SelectionText_Length) {
  // Expect text length of 30 to pass.
  EXPECT_NE(std::nullopt, ExtractPhoneNumberForClickToCall(
                              &profile_, " +1 2 3 4 5 6 7 8 9 0 1 2 3 45"));
  // Expect text length of 31 to fail.
  EXPECT_EQ(std::nullopt, ExtractPhoneNumberForClickToCall(
                              &profile_, " +1 2 3 4 5 6 7 8 9 0 1 2 3 4 5"));
}

TEST_F(ClickToCallUtilsTest, SelectionText_Digits) {
  // Expect text with 15 digits to pass.
  EXPECT_NE(std::nullopt,
            ExtractPhoneNumberForClickToCall(&profile_, "+123456789012345"));
  // Expect text with 16 digits to fail.
  EXPECT_EQ(std::nullopt,
            ExtractPhoneNumberForClickToCall(&profile_, "+1234567890123456"));
}

TEST_F(ClickToCallUtilsTest, IsUrlSafeForClickToCall) {
  EXPECT_FALSE(IsUrlSafeForClickToCall(GURL("tel:123%23456")));
  EXPECT_FALSE(IsUrlSafeForClickToCall(GURL("tel:123%2A456")));
  EXPECT_FALSE(IsUrlSafeForClickToCall(GURL("tel:123*456")));
  EXPECT_FALSE(IsUrlSafeForClickToCall(GURL("tel:123%25456")));

  EXPECT_TRUE(IsUrlSafeForClickToCall(GURL("tel:%2B44123")));
  EXPECT_TRUE(IsUrlSafeForClickToCall(GURL("tel:+44123")));
}
