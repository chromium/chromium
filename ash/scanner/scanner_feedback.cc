// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_feedback.h"

#include <utility>
#include <variant>

#include "base/containers/to_value_list.h"
#include "base/values.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

namespace {

// Converts a `NewContactAction::EmailAddress` to a `base::Value::Dict`.
base::Value::Dict EmailAddressToDict(
    manta::proto::NewContactAction::EmailAddress&& email_address) {
  base::Value::Dict dict;

  if (!email_address.value().empty()) {
    dict.Set("value", std::move(*email_address.mutable_value()));
  }
  if (!email_address.type().empty()) {
    dict.Set("type", std::move(*email_address.mutable_type()));
  }

  return dict;
}

// Converts a `NewContactAction::PhoneNumber` to a `base::Value::Dict`.
base::Value::Dict PhoneNumberToDict(
    manta::proto::NewContactAction::PhoneNumber&& phone_number) {
  base::Value::Dict dict;

  if (!phone_number.value().empty()) {
    dict.Set("value", std::move(*phone_number.mutable_value()));
  }
  if (!phone_number.type().empty()) {
    dict.Set("type", std::move(*phone_number.mutable_type()));
  }

  return dict;
}

// Converts a `ScannerAction` variant into an "externally tagged"
// `base::Value::Dict`.
base::Value::Dict NewEventToDict(manta::proto::NewEventAction&& new_event) {
  base::Value::Dict dict;

  if (!new_event.title().empty()) {
    dict.Set("title", std::move(*new_event.mutable_title()));
  }
  if (!new_event.description().empty()) {
    dict.Set("description", std::move(*new_event.mutable_description()));
  }
  if (!new_event.dates().empty()) {
    dict.Set("dates", std::move(*new_event.mutable_dates()));
  }
  if (!new_event.location().empty()) {
    dict.Set("location", std::move(*new_event.mutable_location()));
  }

  return base::Value::Dict().Set("new_event", std::move(dict));
}

base::Value::Dict NewContactToDict(
    manta::proto::NewContactAction&& new_contact) {
  base::Value::Dict dict;

  if (!new_contact.given_name().empty()) {
    dict.Set("given_name", std::move(*new_contact.mutable_given_name()));
  }
  if (!new_contact.family_name().empty()) {
    dict.Set("family_name", std::move(*new_contact.mutable_family_name()));
  }
  if (!new_contact.email().empty()) {
    dict.Set("email", std::move(*new_contact.mutable_email()));
  }
  if (!new_contact.phone().empty()) {
    dict.Set("phone", std::move(*new_contact.mutable_phone()));
  }
  if (!new_contact.email_addresses().empty()) {
    dict.Set(
        "email_addresses",
        base::ToValueList(
            *new_contact.mutable_email_addresses(),
            [](manta::proto::NewContactAction::EmailAddress& email_address) {
              return EmailAddressToDict(std::move(email_address));
            }));
  }
  if (!new_contact.phone_numbers().empty()) {
    dict.Set("phone_numbers",
             base::ToValueList(
                 *new_contact.mutable_phone_numbers(),
                 [](manta::proto::NewContactAction::PhoneNumber& phone_number) {
                   return PhoneNumberToDict(std::move(phone_number));
                 }));
  }

  return base::Value::Dict().Set("new_contact", std::move(dict));
}

base::Value::Dict NewGoogleDocToDict(
    manta::proto::NewGoogleDocAction&& new_google_doc) {
  base::Value::Dict dict;

  if (!new_google_doc.title().empty()) {
    dict.Set("title", std::move(*new_google_doc.mutable_title()));
  }
  if (!new_google_doc.html_contents().empty()) {
    dict.Set("html_contents",
             std::move(*new_google_doc.mutable_html_contents()));
  }

  return base::Value::Dict().Set("new_google_doc", std::move(dict));
}

base::Value::Dict NewGoogleSheetToDict(
    manta::proto::NewGoogleSheetAction&& new_google_sheet) {
  base::Value::Dict dict;

  if (!new_google_sheet.title().empty()) {
    dict.Set("title", std::move(*new_google_sheet.mutable_title()));
  }
  if (!new_google_sheet.csv_contents().empty()) {
    dict.Set("csv_contents",
             std::move(*new_google_sheet.mutable_csv_contents()));
  }

  return base::Value::Dict().Set("new_google_sheet", std::move(dict));
}

base::Value::Dict CopyToClipboardToDict(
    manta::proto::CopyToClipboardAction&& copy_to_clipboard) {
  base::Value::Dict dict;

  if (!copy_to_clipboard.plain_text().empty()) {
    dict.Set("plain_text", std::move(*copy_to_clipboard.mutable_plain_text()));
  }
  if (!copy_to_clipboard.html_text().empty()) {
    dict.Set("html_text", std::move(*copy_to_clipboard.mutable_html_text()));
  }

  return base::Value::Dict().Set("copy_to_clipboard", std::move(dict));
}

}  // namespace

base::Value::Dict ScannerActionToDict(manta::proto::ScannerAction action) {
  switch (action.action_case()) {
    case manta::proto::ScannerAction::kNewEvent:
      return NewEventToDict(std::move(*action.mutable_new_event()));

    case manta::proto::ScannerAction::kNewContact:
      return NewContactToDict(std::move(*action.mutable_new_contact()));

    case manta::proto::ScannerAction::kNewGoogleDoc:
      return NewGoogleDocToDict(std::move(*action.mutable_new_google_doc()));

    case manta::proto::ScannerAction::kNewGoogleSheet:
      return NewGoogleSheetToDict(
          std::move(*action.mutable_new_google_sheet()));

    case manta::proto::ScannerAction::kCopyToClipboard:
      return CopyToClipboardToDict(
          std::move(*action.mutable_copy_to_clipboard()));

    case manta::proto::ScannerAction::ACTION_NOT_SET:
      return base::Value::Dict();
  }
}

}  // namespace ash
