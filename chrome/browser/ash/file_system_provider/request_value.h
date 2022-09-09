// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_

#include <memory>
#include <string>

#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash {
namespace file_system_provider {

// Holds a parsed value returned by a file system provider. Each accessor can
// return NULL in case the requested value type is not available. It is used
// to pass values of success callbacks.
class RequestValue {
 public:
  // Creates an empty value. Use static methods to create a value holding a
  // proper content.
  RequestValue();

  RequestValue(const RequestValue&) = delete;
  RequestValue& operator=(const RequestValue&) = delete;

  virtual ~RequestValue();

  static std::unique_ptr<RequestValue> CreateForUnmountSuccess(
      std::unique_ptr<extensions::api::file_system_provider_internal::
                          UnmountRequestedSuccess::Params> params);

  static std::unique_ptr<RequestValue> CreateForGetMetadataSuccess(
      std::unique_ptr<extensions::api::file_system_provider_internal::
                          GetMetadataRequestedSuccess::Params> params);

  static std::unique_ptr<RequestValue> CreateForGetActionsSuccess(
      std::unique_ptr<extensions::api::file_system_provider_internal::
                          GetActionsRequestedSuccess::Params> params);

  static std::unique_ptr<RequestValue> CreateForReadDirectorySuccess(
      std::unique_ptr<extensions::api::file_system_provider_internal::
                          ReadDirectoryRequestedSuccess::Params> params);

  static std::unique_ptr<RequestValue> CreateForReadFileSuccess(
      std::unique_ptr<extensions::api::file_system_provider_internal::
                          ReadFileRequestedSuccess::Params> params);

  static std::unique_ptr<RequestValue> CreateForOperationSuccess(
      std::unique_ptr<extensions::api::file_system_provider_internal::
                          OperationRequestedSuccess::Params> params);

  static std::unique_ptr<RequestValue> CreateForOperationError(
      std::unique_ptr<extensions::api::file_system_provider_internal::
                          OperationRequestedError::Params> params);

  static std::unique_ptr<RequestValue> CreateForTesting(
      const std::string& params);

  const extensions::api::file_system_provider_internal::
      UnmountRequestedSuccess::Params*
      unmount_success_params() const {
    return unmount_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      GetMetadataRequestedSuccess::Params*
      get_metadata_success_params() const {
    return get_metadata_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      GetActionsRequestedSuccess::Params*
      get_actions_success_params() const {
    return get_actions_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      ReadDirectoryRequestedSuccess::Params*
      read_directory_success_params() const {
    return read_directory_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      ReadFileRequestedSuccess::Params*
      read_file_success_params() const {
    return read_file_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      OperationRequestedSuccess::Params*
      operation_success_params() const {
    return operation_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      OperationRequestedError::Params*
      operation_error_params() const {
    return operation_error_params_.get();
  }

  const std::string* testing_params() const { return testing_params_.get(); }

 private:
  std::unique_ptr<extensions::api::file_system_provider_internal::
                      UnmountRequestedSuccess::Params>
      unmount_success_params_;
  std::unique_ptr<extensions::api::file_system_provider_internal::
                      GetMetadataRequestedSuccess::Params>
      get_metadata_success_params_;
  std::unique_ptr<extensions::api::file_system_provider_internal::
                      GetActionsRequestedSuccess::Params>
      get_actions_success_params_;
  std::unique_ptr<extensions::api::file_system_provider_internal::
                      ReadDirectoryRequestedSuccess::Params>
      read_directory_success_params_;
  std::unique_ptr<extensions::api::file_system_provider_internal::
                      ReadFileRequestedSuccess::Params>
      read_file_success_params_;
  std::unique_ptr<extensions::api::file_system_provider_internal::
                      OperationRequestedSuccess::Params>
      operation_success_params_;
  std::unique_ptr<extensions::api::file_system_provider_internal::
                      OperationRequestedError::Params>
      operation_error_params_;
  std::unique_ptr<std::string> testing_params_;
};

}  // namespace file_system_provider
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_
