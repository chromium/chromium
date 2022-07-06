// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillDataModel;
using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::ServerFieldType;
using autofill_assistant::external::CreditCardProto;
using autofill_assistant::external::ProfileProto;
using fast_checkout::CreateCreditCardProto;
using fast_checkout::CreateProfileProto;

namespace {

constexpr char kLocale[] = "en-US";
constexpr char kFirstName[] = "John";
constexpr char kLastName[] = "Doe";
constexpr char kEmail[] = "jd@example.com";
constexpr char kAddressLine1[] = "Erika-Mann-Str. 33";
constexpr char kAddressCity[] = "Munich";
constexpr char kAddressZip[] = "80636";
constexpr char kCreditCardNumber[] = "4111111111111111";
constexpr int64_t kInstrumentId = 123;
constexpr char kServerId[] = "server id";
constexpr CreditCard::RecordType kRecordType =
    CreditCard::RecordType::MASKED_SERVER_CARD;

void SetInfo(AutofillDataModel* model,
             ServerFieldType type,
             const std::string& value) {
  model->SetInfo(type, base::UTF8ToUTF16(value), kLocale);
}

}  // namespace

TEST(FastCheckoutUtilTest, CreateProfileProto) {
  auto autofill_profile = std::make_unique<AutofillProfile>();
  SetInfo(autofill_profile.get(), ServerFieldType::NAME_FIRST, kFirstName);
  SetInfo(autofill_profile.get(), ServerFieldType::NAME_LAST, kLastName);
  SetInfo(autofill_profile.get(), ServerFieldType::EMAIL_ADDRESS, kEmail);
  SetInfo(autofill_profile.get(), ServerFieldType::ADDRESS_HOME_LINE1,
          kAddressLine1);
  SetInfo(autofill_profile.get(), ServerFieldType::ADDRESS_HOME_CITY,
          kAddressCity);
  SetInfo(autofill_profile.get(), ServerFieldType::ADDRESS_HOME_ZIP,
          kAddressZip);
  autofill_profile->FinalizeAfterImport();

  ProfileProto profile_proto = CreateProfileProto(*autofill_profile);

  EXPECT_EQ(profile_proto.values().at(ServerFieldType::NAME_FIRST), kFirstName);
  EXPECT_EQ(profile_proto.values().at(ServerFieldType::NAME_LAST), kLastName);
  EXPECT_EQ(profile_proto.values().at(ServerFieldType::EMAIL_ADDRESS), kEmail);
  EXPECT_EQ(profile_proto.values().at(ServerFieldType::ADDRESS_HOME_LINE1),
            kAddressLine1);
  EXPECT_EQ(profile_proto.values().at(ServerFieldType::ADDRESS_HOME_CITY),
            kAddressCity);
  EXPECT_EQ(profile_proto.values().at(ServerFieldType::ADDRESS_HOME_ZIP),
            kAddressZip);
}

TEST(FastCheckoutUtilTest, CreateCreditCardProto) {
  auto credit_card = std::make_unique<CreditCard>();
  SetInfo(credit_card.get(), ServerFieldType::CREDIT_CARD_NUMBER,
          kCreditCardNumber);
  credit_card->set_record_type(kRecordType);
  credit_card->set_instrument_id(kInstrumentId);
  credit_card->set_server_id(kServerId);
  credit_card->SetNetworkForMaskedCard(autofill::kVisaCard);
  std::string last_4_digits =
      base::UTF16ToUTF8(credit_card->NetworkAndLastFourDigits());

  CreditCardProto card_proto = CreateCreditCardProto(*credit_card);

  EXPECT_EQ(card_proto.values().at(ServerFieldType::CREDIT_CARD_NUMBER),
            last_4_digits);
  EXPECT_EQ(card_proto.record_type(), kRecordType);
  EXPECT_EQ(card_proto.instrument_id(), kInstrumentId);
  EXPECT_EQ(card_proto.server_id(), kServerId);
  EXPECT_EQ(card_proto.network(), autofill::kVisaCard);
}
