// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_feedback.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/to_value_list.h"
#include "base/strings/string_number_conversions.h"
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

class UserFacingValueWriter {
 public:
  explicit UserFacingValueWriter(size_t depth_limit, size_t output_limit)
      : user_facing_string_(""),
        depth_limit_(depth_limit),
        output_limit_(output_limit) {}

  void BuildString(std::monostate node) { OutputValue("null"); }
  void BuildString(bool node) { OutputValue(node ? "true" : "false"); }
  void BuildString(int node) { OutputValue(base::NumberToString(node)); }
  void BuildString(double node) { OutputValue(base::NumberToString(node)); }
  void BuildString(std::string_view node) { OutputValue(node); }
  void BuildString(const base::Value::BlobStorage& node) {
    user_facing_string_.reset();
  }
  void BuildString(const base::Value::Dict& node) {
    if (path_.size() >= depth_limit_) {
      user_facing_string_.reset();
      return;
    }

    for (auto [key, value] : node) {
      path_.push_back(key);
      value.Visit([this](const auto& member) { BuildString(member); });
      path_.pop_back();
    }
  }
  void BuildString(const base::Value::List& node) {
    if (path_.size() >= depth_limit_) {
      user_facing_string_.reset();
      return;
    }

    size_t i = 0;
    for (const base::Value& value : node) {
      path_.push_back(base::NumberToString(i));
      value.Visit([this](const auto& member) { BuildString(member); });
      path_.pop_back();
      ++i;
    }
  }

  std::optional<std::string>&& user_facing_string() && {
    return std::move(user_facing_string_);
  }

 private:
  void OutputValue(std::string_view value) {
    if (!user_facing_string_.has_value()) {
      return;
    }

    if (!path_.empty()) {
      user_facing_string_->append(path_[0]);
      for (size_t i = 1; i < path_.size(); ++i) {
        user_facing_string_->push_back('.');
        user_facing_string_->append(path_[i]);
      }
      user_facing_string_->append(": ");
    }
    user_facing_string_->append(value);
    user_facing_string_->push_back('\n');

    if (user_facing_string_->size() > output_limit_) {
      user_facing_string_.reset();
      return;
    }
  }

  std::vector<std::string> path_;
  std::optional<std::string> user_facing_string_;
  size_t depth_limit_;
  size_t output_limit_;
};

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

std::optional<std::string> ValueToUserFacingString(base::ValueView value,
                                                   size_t depth_limit,
                                                   size_t output_limit) {
  UserFacingValueWriter writer(depth_limit, output_limit);
  value.Visit([&writer](const auto& value) { writer.BuildString(value); });
  return std::move(writer).user_facing_string();
}

}  // namespace ash
