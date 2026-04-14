// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/fake_document_scan_ash.h"

#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"

namespace extensions {

FakeDocumentScanAsh::FakeDocumentScanAsh() = default;
FakeDocumentScanAsh::~FakeDocumentScanAsh() = default;

FakeDocumentScanAsh::OpenScannerState::OpenScannerState() = default;
FakeDocumentScanAsh::OpenScannerState::~OpenScannerState() = default;

FakeDocumentScanAsh::OpenScannerState::OpenScannerState(
    const std::string& client_id,
    const std::string& connection_string)
    : client_id(client_id), connection_string(connection_string) {}

void FakeDocumentScanAsh::OpenScanner(const std::string& client_id,
                                      const std::string& scanner_id,
                                      OpenScannerCallback callback) {
  // If a response for scanner_id hasn't been set, this is the equivalent
  // of trying to open a device that has been unplugged or disappeared off the
  // network.
  if (!open_responses_.contains(scanner_id)) {
    auto response = crosapi::mojom::OpenScannerResponse::New();
    response->scanner_id = scanner_id;
    response->result = crosapi::mojom::ScannerOperationResult::kDeviceMissing;
    std::move(callback).Run(std::move(response));
    return;
  }
  // If the scanner is already open by a different client, the real backend will
  // report DEVICE_BUSY to any other clients trying to open it.  Do the same
  // here.
  for (const auto& [handle, original] : open_scanners_) {
    if (original.connection_string == scanner_id &&
        original.client_id != client_id) {
      auto response = crosapi::mojom::OpenScannerResponse::New();
      response->scanner_id = scanner_id;
      response->result = crosapi::mojom::ScannerOperationResult::kDeviceBusy;
      std::move(callback).Run(std::move(response));
      return;
    }
  }

  crosapi::mojom::OpenScannerResponsePtr response =
      open_responses_[scanner_id].Clone();
  response->scanner_handle =
      response->scanner_handle.value_or(scanner_id + "-handle") +
      base::StringPrintf("%03zu", ++handle_count_);
  open_scanners_[response->scanner_handle.value()] =
      OpenScannerState(client_id, scanner_id);
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::CloseScanner(const std::string& scanner_handle) {
  open_scanners_.erase(scanner_handle);
}

void FakeDocumentScanAsh::SetOptions(
    const std::string& scanner_handle,
    std::vector<crosapi::mojom::OptionSettingPtr> options,
    SetOptionsCallback callback) {
  auto response = crosapi::mojom::SetOptionsResponse::New();
  response->scanner_handle = scanner_handle;
  response->results.reserve(options.size());

  if (!open_scanners_.contains(scanner_handle)) {
    for (const auto& setting : options) {
      response->results.emplace_back(crosapi::mojom::SetOptionResult::New(
          setting->name,
          crosapi::mojom::ScannerOperationResult::kDeviceMissing));
    }
    std::move(callback).Run(std::move(response));
    return;
  }

  // Fake setting options by copying and overriding the original config that
  // would have been returned for this scanner.
  const auto& open_response =
      open_responses_[open_scanners_[scanner_handle].connection_string];
  if (!open_response->options.has_value()) {
    for (const auto& setting : options) {
      response->results.emplace_back(crosapi::mojom::SetOptionResult::New(
          setting->name,
          crosapi::mojom::ScannerOperationResult::kInternalError));
    }
    std::move(callback).Run(std::move(response));
    return;
  }
  response->options.emplace();
  response->options->reserve(open_response->options->size());
  for (const auto& [name, option] : open_response->options.value()) {
    response->options->try_emplace(name, option.Clone());
  }

  for (const auto& setting : options) {
    auto result = crosapi::mojom::SetOptionResult::New();
    result->name = setting->name;

    // Ensure the returned options contains the requested option so that callers
    // can look up the value.  The real backend doesn't behave this way, but
    // this avoids a ton of boilerplate in tests without changing the handler
    // code coverage that can be achieved with the fake.
    if (!response->options.value().contains(setting->name)) {
      auto option = crosapi::mojom::ScannerOption::New();
      option->name = setting->name;
      option->type = setting->type;
      response->options->try_emplace(setting->name, std::move(option));
    }

    if (setting->value.is_null()) {
      result->result = crosapi::mojom::ScannerOperationResult::kSuccess;
    } else {
      // If there's a value, make sure the value type matches the option type.
      // The real backend does a lot more validation, but other cases are
      // handled as pass-through, so there's no need to implement everything in
      // this fake.
      switch (setting->type) {
        case crosapi::mojom::OptionType::kBool:
          if (setting->value->is_bool_value()) {
            result->result = crosapi::mojom::ScannerOperationResult::kSuccess;
            response->options->at(setting->name)->value =
                crosapi::mojom::OptionValue::NewBoolValue(
                    setting->value->get_bool_value());
          } else {
            result->result = crosapi::mojom::ScannerOperationResult::kWrongType;
          }
          break;
        case crosapi::mojom::OptionType::kInt:
          if (setting->value->is_int_value()) {
            result->result = crosapi::mojom::ScannerOperationResult::kSuccess;
            response->options->at(setting->name)->value =
                crosapi::mojom::OptionValue::NewIntValue(
                    setting->value->get_int_value());
          } else if (setting->value->is_int_list()) {
            result->result = crosapi::mojom::ScannerOperationResult::kSuccess;
            response->options->at(setting->name)->value =
                crosapi::mojom::OptionValue::NewIntList(
                    {setting->value->get_int_list().begin(),
                     setting->value->get_int_list().end()});
          } else {
            result->result = crosapi::mojom::ScannerOperationResult::kWrongType;
          }
          break;
        case crosapi::mojom::OptionType::kFixed:
          if (setting->value->is_fixed_value()) {
            result->result = crosapi::mojom::ScannerOperationResult::kSuccess;
            response->options->at(setting->name)->value =
                crosapi::mojom::OptionValue::NewFixedValue(
                    setting->value->get_fixed_value());
          } else if (setting->value->is_fixed_list()) {
            result->result = crosapi::mojom::ScannerOperationResult::kSuccess;
            response->options->at(setting->name)->value =
                crosapi::mojom::OptionValue::NewFixedList(
                    {setting->value->get_fixed_list().begin(),
                     setting->value->get_fixed_list().end()});
          } else {
            result->result = crosapi::mojom::ScannerOperationResult::kWrongType;
          }
          break;
        case crosapi::mojom::OptionType::kString:
          if (setting->value->is_string_value()) {
            result->result = crosapi::mojom::ScannerOperationResult::kSuccess;
            response->options->at(setting->name)->value =
                crosapi::mojom::OptionValue::NewStringValue(
                    setting->value->get_string_value());
          } else {
            result->result = crosapi::mojom::ScannerOperationResult::kWrongType;
          }
          break;
        default:
          // Claim it succeeded, but don't update the returned option value.
          // This is a valid outcome for a real scanner, so the frontend has to
          // account for it, anyway.
          result->result = crosapi::mojom::ScannerOperationResult::kSuccess;
          break;
      }
    }
    response->results.emplace_back(std::move(result));
  }

  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::SetOpenScannerResponse(
    const std::string& connection_string,
    crosapi::mojom::OpenScannerResponsePtr response) {
  open_responses_[connection_string] = std::move(response);
}

}  // namespace extensions
