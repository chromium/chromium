// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_feedback.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/manta/proto/scanner.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::base::test::IsJson;

TEST(ScannerFeedbackTest, UnsetActionToDict) {
  base::Value::Dict dict = ScannerActionToDict(manta::proto::ScannerAction());

  EXPECT_THAT(dict, IsJson("{}"));
}

TEST(ScannerFeedbackTest, NewEventActionToDict) {
  manta::proto::ScannerAction action;
  manta::proto::NewEventAction& new_event = *action.mutable_new_event();
  new_event.set_title("🌏");
  new_event.set_description("formerly \"Geo Sync\"");
  new_event.set_dates("20241014T160000/20241014T161500");
  new_event.set_location("Wonderland");

  base::Value::Dict dict = ScannerActionToDict(std::move(action));

  EXPECT_THAT(dict, IsJson(R"json({
    "new_event": {
      "title": "🌏",
      "description": "formerly \"Geo Sync\"",
      "dates": "20241014T160000/20241014T161500",
      "location": "Wonderland",
    }
  })json"));
}

TEST(ScannerFeedbackTest, NewContactActionToDict) {
  manta::proto::ScannerAction action;
  manta::proto::NewContactAction& new_contact = *action.mutable_new_contact();
  new_contact.set_given_name("André");
  new_contact.set_family_name("François");
  new_contact.set_email("afrancois@example.com");
  new_contact.set_phone("+61400000000");
  manta::proto::NewContactAction::EmailAddress& home_email =
      *new_contact.add_email_addresses();
  home_email.set_value("afrancois@example.com");
  home_email.set_type("home");
  manta::proto::NewContactAction::EmailAddress& work_email =
      *new_contact.add_email_addresses();
  work_email.set_value("afrancois@work.example.com");
  work_email.set_type("work");
  manta::proto::NewContactAction::PhoneNumber& mobile_number =
      *new_contact.add_phone_numbers();
  mobile_number.set_value("+61400000000");
  mobile_number.set_type("mobile");
  manta::proto::NewContactAction::PhoneNumber& home_number =
      *new_contact.add_phone_numbers();
  home_number.set_value("+61390000000");
  home_number.set_type("home");

  base::Value::Dict dict = ScannerActionToDict(std::move(action));

  EXPECT_THAT(dict, IsJson(R"json({
    "new_contact": {
      "given_name": "André",
      "family_name": "François",
      "email": "afrancois@example.com",
      "phone": "+61400000000",
      "email_addresses": [
        {
          "value": "afrancois@example.com",
          "type": "home",
        },
        {
          "value": "afrancois@work.example.com",
          "type": "work",
        },
      ],
      "phone_numbers": [
        {
          "value": "+61400000000",
          "type": "mobile",
        },
        {
          "value": "+61390000000",
          "type": "home",
        },
      ],
    }
  })json"));
}

TEST(ScannerFeedbackTest, NewGoogleDocActionToDict) {
  manta::proto::ScannerAction action;
  manta::proto::NewGoogleDocAction& new_google_doc =
      *action.mutable_new_google_doc();
  new_google_doc.set_title("Doc Title");
  new_google_doc.set_html_contents("<span>Contents</span>");

  base::Value::Dict dict = ScannerActionToDict(std::move(action));

  EXPECT_THAT(dict, IsJson(R"json({
    "new_google_doc": {
      "title": "Doc Title",
      "html_contents": "<span>Contents</span>",
    }
  })json"));
}

TEST(ScannerFeedbackTest, NewGoogleSheetActionToDict) {
  manta::proto::ScannerAction action;
  manta::proto::NewGoogleSheetAction& new_google_sheet =
      *action.mutable_new_google_sheet();
  new_google_sheet.set_title("Sheet Title");
  new_google_sheet.set_csv_contents("a,b\n1,2");

  base::Value::Dict dict = ScannerActionToDict(std::move(action));

  EXPECT_THAT(dict, IsJson(R"json({
    "new_google_sheet": {
      "title": "Sheet Title",
      "csv_contents": "a,b\n1,2",
    }
  })json"));
}

TEST(ScannerFeedbackTest, CopyToClipboardActionToDict) {
  manta::proto::ScannerAction action;
  manta::proto::CopyToClipboardAction& copy_to_clipboard =
      *action.mutable_copy_to_clipboard();
  copy_to_clipboard.set_plain_text("Hello");
  copy_to_clipboard.set_html_text("<b>Hello</b>");

  base::Value::Dict dict = ScannerActionToDict(std::move(action));

  EXPECT_THAT(dict, IsJson(R"json({
    "copy_to_clipboard": {
      "plain_text": "Hello",
      "html_text": "<b>Hello</b>",
    }
  })json"));
}

struct ValueToUserFacingStringTestCase {
  std::string name;
  std::string json_string;
  std::optional<std::string> user_string;
  size_t depth_limit = 20;
  size_t output_limit = 1000;
};

class ScannerValueToUserFacingStringTest
    : public ::testing::TestWithParam<ValueToUserFacingStringTestCase> {};

// These tests assume that `Dict`s are iterated through in sorted order. If this
// ever changes, these tests should be updated to use gMock's
// `UnorderedElementsAre` matcher over the lines of the user-facing string.
INSTANTIATE_TEST_SUITE_P(
    ,
    ScannerValueToUserFacingStringTest,
    ::testing::ValuesIn({
        ValueToUserFacingStringTestCase{
            .name = "Basic",
            .json_string =
                R"json({
                  "new_event": {
                    "title": "🌏",
                    "description": "formerly \"Geo Sync\"",
                    "dates": "20241014T160000/20241014T161500",
                    "location": "Wonderland",
                  }
                })json",
            .user_string =
                R"user(new_event.dates: 20241014T160000/20241014T161500
new_event.description: formerly "Geo Sync"
new_event.location: Wonderland
new_event.title: 🌏
)user",
        },
        ValueToUserFacingStringTestCase{
            .name = "Complex",
            .json_string =
                R"json({
                  "new_contact": {
                    "given_name": "André",
                    "family_name": "François",
                    "email": "afrancois@example.com",
                    "phone": "+61400000000",
                    "email_addresses": [
                      {
                        "value": "afrancois@example.com",
                        "type": "home",
                      },
                      {
                        "value": "afrancois@work.example.com",
                        "type": "work",
                      },
                    ],
                    "phone_numbers": [
                      {
                        "value": "+61400000000",
                        "type": "mobile",
                      },
                      {
                        "value": "+61390000000",
                        "type": "home",
                      },
                    ],
                  }
                })json",
            .user_string =
                R"user(new_contact.email: afrancois@example.com
new_contact.email_addresses.0.type: home
new_contact.email_addresses.0.value: afrancois@example.com
new_contact.email_addresses.1.type: work
new_contact.email_addresses.1.value: afrancois@work.example.com
new_contact.family_name: François
new_contact.given_name: André
new_contact.phone: +61400000000
new_contact.phone_numbers.0.type: mobile
new_contact.phone_numbers.0.value: +61400000000
new_contact.phone_numbers.1.type: home
new_contact.phone_numbers.1.value: +61390000000
)user",
        },
        ValueToUserFacingStringTestCase{
            .name = "DoesNotHtmlEscapeStrings",
            .json_string =
                R"json({
                  "copy_to_clipboard": {
                    "plain_text": "Hello",
                    "html_text": "\u003Cb>Hello\u003C/b>",
                  }
                })json",
            .user_string =
                R"user(copy_to_clipboard.html_text: <b>Hello</b>
copy_to_clipboard.plain_text: Hello
)user",
        },
        ValueToUserFacingStringTestCase{
            .name = "PlainFalse",
            .json_string = "false",
            .user_string = "false\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "PlainTrue",
            .json_string = "true",
            .user_string = "true\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "PlainNull",
            .json_string = "null",
            .user_string = "null\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "PlainNumber",
            .json_string = "123",
            .user_string = "123\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "PlainString",
            .json_string = "\"string\"",
            .user_string = "string\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "NestedDict",
            .json_string =
                R"json({
                  "a": {
                    "b": {
                      "c": 1,
                    },
                    "d": 2,
                  },
                  "e": 3,
                })json",
            .user_string = "a.b.c: 1\na.d: 2\ne: 3\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "NestedList",
            .json_string =
                R"json([
                  [
                    [1],
                    2,
                  ],
                  3,
                ])json",
            .user_string = "0.0.0: 1\n0.1: 2\n1: 3\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "DictInList",
            .json_string =
                R"json([
                  {
                    "a": 1,
                  },
                  {
                    "b": 2,
                  },
                ])json",
            .user_string = "0.a: 1\n1.b: 2\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "ListInDict",
            .json_string =
                R"json({
                  "a": [1, 2],
                  "b": [3, 4],
                })json",
            .user_string = "a.0: 1\na.1: 2\nb.0: 3\nb.1: 4\n",
        },
        ValueToUserFacingStringTestCase{
            .name = "HitsDepthLimitWithDictionary",
            .json_string = R"json({"a":{"b":{"c": 1}}})json",
            .user_string = "a.b.c: 1\n",
            .depth_limit = 3,
        },
        ValueToUserFacingStringTestCase{
            .name = "ExceedsDepthLimitWithDictionary",
            .json_string = R"json({"a":{"b":{"c": 1}}})json",
            .user_string = std::nullopt,
            .depth_limit = 2,
        },
        ValueToUserFacingStringTestCase{
            .name = "HitsDepthLimitWithList",
            .json_string = R"json([[[1]]])json",
            .user_string = "0.0.0: 1\n",
            .depth_limit = 3,
        },
        ValueToUserFacingStringTestCase{
            .name = "ExceedsDepthLimitWithList",
            .json_string = R"json([[[1]]])json",
            .user_string = std::nullopt,
            .depth_limit = 2,
        },
        ValueToUserFacingStringTestCase{
            .name = "HitsDepthLimitWithDictionaryAndList",
            .json_string = R"json([[{"a": 1}]])json",
            .user_string = "0.0.a: 1\n",
            .depth_limit = 3,
        },
        ValueToUserFacingStringTestCase{
            .name = "ExceedsDepthLimitWithDictionaryAndList",
            .json_string = R"json([[{"a": 1}]])json",
            .user_string = std::nullopt,
            .depth_limit = 2,
        },
        ValueToUserFacingStringTestCase{
            .name = "HitsOutputLimit",
            .json_string = "123456789",
            .user_string = "123456789\n",
            .output_limit = 10,
        },
        ValueToUserFacingStringTestCase{
            .name = "ExceedsOutputLimit",
            .json_string = "123456789",
            .user_string = std::nullopt,
            .output_limit = 9,
        },
    }),
    [](const ::testing::TestParamInfo<ValueToUserFacingStringTestCase>& info) {
      return info.param.name;
    });

TEST_P(ScannerValueToUserFacingStringTest,
       ValueToUserFacingStringMatchesExpected) {
  base::Value value = base::test::ParseJson(GetParam().json_string);

  std::optional<std::string> user_string = ValueToUserFacingString(
      value, GetParam().depth_limit, GetParam().output_limit);

  EXPECT_EQ(GetParam().user_string, user_string);
}

TEST(ScannerFeedbackTest, ValueToUserFacingStringReturnsNulloptWithBinary) {
  base::Value binary_value(base::as_byte_span("binaryvalue"));

  std::optional<std::string> user_string = ValueToUserFacingString(
      binary_value, /*depth_limit=*/20, /*output_limit=*/1000);

  EXPECT_EQ(user_string, std::nullopt);
}

TEST(ScannerFeedbackTest,
     ValueToUserFacingStringReturnsNulloptWithNestedBinary) {
  base::Value::Dict nested_value;
  nested_value.EnsureDict("a")->EnsureList("b")->Append(
      base::Value(base::as_byte_span("binaryvalue")));

  std::optional<std::string> user_string = ValueToUserFacingString(
      nested_value, /*depth_limit=*/20, /*output_limit=*/1000);

  EXPECT_EQ(user_string, std::nullopt);
}

}  // namespace
}  // namespace ash
