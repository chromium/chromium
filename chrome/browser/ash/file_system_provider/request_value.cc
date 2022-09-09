// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/request_value.h"

#include <memory>
#include <utility>

namespace ash {
namespace file_system_provider {

RequestValue::RequestValue() {
}

RequestValue::~RequestValue() {
}

std::unique_ptr<RequestValue> RequestValue::CreateForUnmountSuccess(
    std::unique_ptr<extensions::api::file_system_provider_internal::
                        UnmountRequestedSuccess::Params> params) {
  std::unique_ptr<RequestValue> result(new RequestValue);
  result->unmount_success_params_ = std::move(params);
  return result;
}

std::unique_ptr<RequestValue> RequestValue::CreateForGetMetadataSuccess(
    std::unique_ptr<extensions::api::file_system_provider_internal::
                        GetMetadataRequestedSuccess::Params> params) {
  std::unique_ptr<RequestValue> result(new RequestValue);
  result->get_metadata_success_params_ = std::move(params);
  return result;
}

std::unique_ptr<RequestValue> RequestValue::CreateForGetActionsSuccess(
    std::unique_ptr<extensions::api::file_system_provider_internal::
                        GetActionsRequestedSuccess::Params> params) {
  std::unique_ptr<RequestValue> result(new RequestValue);
  result->get_actions_success_params_ = std::move(params);
  return result;
}

std::unique_ptr<RequestValue> RequestValue::CreateForReadDirectorySuccess(
    std::unique_ptr<extensions::api::file_system_provider_internal::
                        ReadDirectoryRequestedSuccess::Params> params) {
  std::unique_ptr<RequestValue> result(new RequestValue);
  result->read_directory_success_params_ = std::move(params);
  return result;
}

std::unique_ptr<RequestValue> RequestValue::CreateForReadFileSuccess(
    std::unique_ptr<extensions::api::file_system_provider_internal::
                        ReadFileRequestedSuccess::Params> params) {
  std::unique_ptr<RequestValue> result(new RequestValue);
  result->read_file_success_params_ = std::move(params);
  return result;
}

std::unique_ptr<RequestValue> RequestValue::CreateForOperationSuccess(
    std::unique_ptr<extensions::api::file_system_provider_internal::
                        OperationRequestedSuccess::Params> params) {
  std::unique_ptr<RequestValue> result(new RequestValue);
  result->operation_success_params_ = std::move(params);
  return result;
}

std::unique_ptr<RequestValue> RequestValue::CreateForOperationError(
    std::unique_ptr<extensions::api::file_system_provider_internal::
                        OperationRequestedError::Params> params) {
  std::unique_ptr<RequestValue> result(new RequestValue);
  result->operation_error_params_ = std::move(params);
  return result;
}

std::unique_ptr<RequestValue> RequestValue::CreateForTesting(
    const std::string& params) {
  std::unique_ptr<RequestValue> result(new RequestValue);
  result->testing_params_ = std::make_unique<std::string>(params);
  return result;
}

}  // namespace file_system_provider
}  // namespace ash
