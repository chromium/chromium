// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/fake_document_scan_ash.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
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

void FakeDocumentScanAsh::GetScannerNames(GetScannerNamesCallback callback) {
  std::move(callback).Run(scanner_names_);
}

void FakeDocumentScanAsh::ScanFirstPage(const std::string& scanner_name,
                                        ScanFirstPageCallback callback) {
  if (scan_data_.has_value()) {
    std::move(callback).Run(crosapi::mojom::ScanFailureMode::kNoFailure,
                            scan_data_.value()[0]);
  } else {
    std::move(callback).Run(crosapi::mojom::ScanFailureMode::kDeviceBusy,
                            std::nullopt);
  }
}

void FakeDocumentScanAsh::GetScannerList(
    const std::string& client_id,
    crosapi::mojom::ScannerEnumFilterPtr filter,
    GetScannerListCallback callback) {
  auto response = crosapi::mojom::GetScannerListResponse::New();
  response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
  for (const auto& scanner : scanners_) {
    response->scanners.emplace_back(scanner.Clone());

    // Since this scanner will be listed, also create an entry that allows
    // callers to open it.
    auto open_response = crosapi::mojom::OpenScannerResponse::New();
    open_response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
    open_response->scanner_id = scanner->id;
    open_response->scanner_handle = scanner->id + "-handle-" + client_id;
    open_response->options.emplace();
    open_response->options.value()["option1"] =
        CreateTestScannerOption("option1", 5);
    open_responses_[scanner->id] = std::move(open_response);
  }
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::OpenScanner(const std::string& client_id,
                                      const std::string& scanner_id,
                                      OpenScannerCallback callback) {
  // If a response for scanner_id hasn't been set, this is the equivalent
  // of trying to open a device that has been unplugged or disappeared off the
  // network.
  if (!base::Contains(open_responses_, scanner_id)) {
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

void FakeDocumentScanAsh::GetOptionGroups(const std::string& scanner_handle,
                                          GetOptionGroupsCallback callback) {
  if (!base::Contains(open_scanners_, scanner_handle)) {
    auto response = crosapi::mojom::GetOptionGroupsResponse::New();
    response->scanner_handle = scanner_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    std::move(callback).Run(std::move(response));
    return;
  }

  // The API handler just passes through responses from this function, so always
  // returning a hardcoded value shouldn't matter.
  auto response = crosapi::mojom::GetOptionGroupsResponse::New();
  response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
  response->scanner_handle = scanner_handle;
  response->groups.emplace();
  auto option_group = crosapi::mojom::OptionGroup::New();
  option_group->title = "title";
  option_group->members.emplace_back("item1");
  option_group->members.emplace_back("item2");
  response->groups->emplace_back(std::move(option_group));
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::CloseScanner(const std::string& scanner_handle,
                                       CloseScannerCallback callback) {
  auto response = crosapi::mojom::CloseScannerResponse::New();
  response->scanner_handle = scanner_handle;
  if (base::Contains(open_scanners_, scanner_handle)) {
    response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
  } else {
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
  }
  open_scanners_.erase(scanner_handle);
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::StartPreparedScan(
    const std::string& scanner_handle,
    crosapi::mojom::StartScanOptionsPtr options,
    StartPreparedScanCallback callback) {
  if (!base::Contains(open_scanners_, scanner_handle)) {
    auto response = crosapi::mojom::StartPreparedScanResponse::New();
    response->scanner_handle = scanner_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    std::move(callback).Run(std::move(response));
    return;
  }

  auto response = crosapi::mojom::StartPreparedScanResponse::New();
  response->scanner_handle = scanner_handle;
  if (options->max_read_size.has_value() &&
      options->max_read_size.value() < smallest_max_read_) {
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    std::move(callback).Run(std::move(response));
    return;
  }

  response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
  response->job_handle = base::StringPrintf(
      "%s-job-%03zu", scanner_handle.c_str(), ++handle_count_);
  open_scanners_.at(scanner_handle).job_handle = response->job_handle;
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::ReadScanData(const std::string& job_handle,
                                       ReadScanDataCallback callback) {
  // The API handler just passes through responses from this function, so always
  // returning a hardcoded value for valid job handles shouldn't matter.  For
  // invalid job handles, report them as cancelled.
  auto response = crosapi::mojom::ReadScanDataResponse::New();
  response->job_handle = job_handle;
  response->result = crosapi::mojom::ScannerOperationResult::kCancelled;
  for (auto& [scanner_handle, state] : open_scanners_) {
    if (state.job_handle.value_or("") == job_handle) {
      response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
      response->data.emplace(std::vector<int8_t>{'i', 'm', 'g'});
      response->estimated_completion = 12;
      break;
    }
  }
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::SetOptions(
    const std::string& scanner_handle,
    std::vector<crosapi::mojom::OptionSettingPtr> options,
    SetOptionsCallback callback) {
  auto response = crosapi::mojom::SetOptionsResponse::New();
  response->scanner_handle = scanner_handle;
  response->results.reserve(options.size());

  if (!base::Contains(open_scanners_, scanner_handle)) {
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
    if (!base::Contains(response->options.value(), setting->name)) {
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

void FakeDocumentScanAsh::CancelScan(const std::string& job_handle,
                                     CancelScanCallback callback) {
  auto response = crosapi::mojom::CancelScanResponse::New();
  response->job_handle = job_handle;
  // Explicitly set this to kAdfJammed instead of kInvalid since kAdfJammed is
  // not used in the DocumentScanAPIHandler cancel methods.  If this was
  // kInvalid the tests may not know if the kInvalid was returned from this fake
  // (in which case, the test may not be testing what it is intended to test) or
  // was returned from the DocumentScanAPIHandler object (as expected).
  response->result = crosapi::mojom::ScannerOperationResult::kAdfJammed;

  // Check all of our open scanners.  If any has this job, cancel it and return
  // a success result.  If not, return a failure result.
  for (auto& [scanner_handle, state] : open_scanners_) {
    if (state.job_handle.value_or("") == job_handle) {
      response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
      state.job_handle.reset();
      break;
    }
  }

  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::SetGetScannerNamesResponse(
    std::vector<std::string> scanner_names) {
  scanner_names_ = std::move(scanner_names);
}

void FakeDocumentScanAsh::SetScanResponse(
    const std::optional<std::vector<std::string>>& scan_data) {
  if (scan_data.has_value()) {
    DCHECK(!scan_data.value().empty());
  }
  scan_data_ = scan_data;
}

void FakeDocumentScanAsh::AddScanner(crosapi::mojom::ScannerInfoPtr scanner) {
  scanners_.emplace_back(std::move(scanner));
}

void FakeDocumentScanAsh::SetOpenScannerResponse(
    const std::string& connection_string,
    crosapi::mojom::OpenScannerResponsePtr response) {
  open_responses_[connection_string] = std::move(response);
}

void FakeDocumentScanAsh::SetSmallestMaxReadSize(size_t max_size) {
  smallest_max_read_ = max_size;
}

}  // namespace extensions
