// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_

#include <string>

#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash::file_system_provider {

// Holds a parsed value returned by a file system provider. Each accessor can
// return nullptr in case the requested value type is not available. It is used
// to pass values of success callbacks.
class RequestValue {
 public:
  // Creates an empty value. Use static methods to create a value holding a
  // proper content.
  RequestValue() noexcept;
  RequestValue(RequestValue&& other) noexcept;
  RequestValue& operator=(RequestValue&& other) noexcept;
  ~RequestValue() noexcept;

  static RequestValue CreateForUnmountSuccess(
      extensions::api::file_system_provider_internal::UnmountRequestedSuccess::
          Params params);

  static RequestValue CreateForGetMetadataSuccess(
      extensions::api::file_system_provider_internal::
          GetMetadataRequestedSuccess::Params params);

  static RequestValue CreateForGetActionsSuccess(
      extensions::api::file_system_provider_internal::
          GetActionsRequestedSuccess::Params params);

  static RequestValue CreateForReadDirectorySuccess(
      extensions::api::file_system_provider_internal::
          ReadDirectoryRequestedSuccess::Params params);

  static RequestValue CreateForReadFileSuccess(
      extensions::api::file_system_provider_internal::ReadFileRequestedSuccess::
          Params params);

  static RequestValue CreateForOpenFileSuccess(
      extensions::api::file_system_provider_internal::OpenFileRequestedSuccess::
          Params params);

  static RequestValue CreateForOperationSuccess(
      extensions::api::file_system_provider_internal::
          OperationRequestedSuccess::Params params);

  static RequestValue CreateForOperationError(
      extensions::api::file_system_provider_internal::OperationRequestedError::
          Params params);

  static RequestValue CreateForTesting(const std::string& params);

  const extensions::api::file_system_provider_internal::
      UnmountRequestedSuccess::Params*
      unmount_success_params() const {
    return absl::get_if<extensions::api::file_system_provider_internal::
                            UnmountRequestedSuccess::Params>(&data_);
  }

  const extensions::api::file_system_provider_internal::
      GetMetadataRequestedSuccess::Params*
      get_metadata_success_params() const {
    return absl::get_if<extensions::api::file_system_provider_internal::
                            GetMetadataRequestedSuccess::Params>(&data_);
  }

  const extensions::api::file_system_provider_internal::
      GetActionsRequestedSuccess::Params*
      get_actions_success_params() const {
    return absl::get_if<extensions::api::file_system_provider_internal::
                            GetActionsRequestedSuccess::Params>(&data_);
  }

  const extensions::api::file_system_provider_internal::
      ReadDirectoryRequestedSuccess::Params*
      read_directory_success_params() const {
    return absl::get_if<extensions::api::file_system_provider_internal::
                            ReadDirectoryRequestedSuccess::Params>(&data_);
  }

  const extensions::api::file_system_provider_internal::
      ReadFileRequestedSuccess::Params*
      read_file_success_params() const {
    return absl::get_if<extensions::api::file_system_provider_internal::
                            ReadFileRequestedSuccess::Params>(&data_);
  }

  const extensions::api::file_system_provider_internal::
      OpenFileRequestedSuccess::Params*
      open_file_success_params() const {
    return absl::get_if<extensions::api::file_system_provider_internal::
                            OpenFileRequestedSuccess::Params>(&data_);
  }

  const extensions::api::file_system_provider_internal::
      OperationRequestedSuccess::Params*
      operation_success_params() const {
    return absl::get_if<extensions::api::file_system_provider_internal::
                            OperationRequestedSuccess::Params>(&data_);
  }

  const extensions::api::file_system_provider_internal::
      OperationRequestedError::Params*
      operation_error_params() const {
    return absl::get_if<extensions::api::file_system_provider_internal::
                            OperationRequestedError::Params>(&data_);
  }

  const std::string* testing_params() const {
    return absl::get_if<std::string>(&data_);
  }

  bool is_valid() const {
    return !absl::holds_alternative<absl::monostate>(data_);
  }

 private:
  // A variant holding the possible types of data held as a value.
  absl::variant<absl::monostate,
                extensions::api::file_system_provider_internal::
                    UnmountRequestedSuccess::Params,
                extensions::api::file_system_provider_internal::
                    GetMetadataRequestedSuccess::Params,
                extensions::api::file_system_provider_internal::
                    GetActionsRequestedSuccess::Params,
                extensions::api::file_system_provider_internal::
                    ReadDirectoryRequestedSuccess::Params,
                extensions::api::file_system_provider_internal::
                    ReadFileRequestedSuccess::Params,
                extensions::api::file_system_provider_internal::
                    OpenFileRequestedSuccess::Params,
                extensions::api::file_system_provider_internal::
                    OperationRequestedSuccess::Params,
                extensions::api::file_system_provider_internal::
                    OperationRequestedError::Params,
                std::string>
      data_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_
