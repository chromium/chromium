// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_feedback.h"

#include <utility>

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

}  // namespace
}  // namespace ash
