// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/request_value.h"

#include <utility>

namespace ash::file_system_provider {

RequestValue::RequestValue() noexcept = default;
RequestValue::RequestValue(RequestValue&& other) noexcept = default;
RequestValue& RequestValue::operator=(RequestValue&& other) noexcept = default;
RequestValue::~RequestValue() noexcept = default;

RequestValue RequestValue::CreateForUnmountSuccess(
    extensions::api::file_system_provider_internal::UnmountRequestedSuccess::
        Params params) {
  RequestValue result;
  result.data_.emplace<decltype(params)>(std::move(params));
  return result;
}

RequestValue RequestValue::CreateForGetMetadataSuccess(
    extensions::api::file_system_provider_internal::
        GetMetadataRequestedSuccess::Params params) {
  RequestValue result;
  result.data_.emplace<decltype(params)>(std::move(params));
  return result;
}

RequestValue RequestValue::CreateForGetActionsSuccess(
    extensions::api::file_system_provider_internal::GetActionsRequestedSuccess::
        Params params) {
  RequestValue result;
  result.data_.emplace<decltype(params)>(std::move(params));
  return result;
}

RequestValue RequestValue::CreateForReadDirectorySuccess(
    extensions::api::file_system_provider_internal::
        ReadDirectoryRequestedSuccess::Params params) {
  RequestValue result;
  result.data_.emplace<decltype(params)>(std::move(params));
  return result;
}

RequestValue RequestValue::CreateForReadFileSuccess(
    extensions::api::file_system_provider_internal::ReadFileRequestedSuccess::
        Params params) {
  RequestValue result;
  result.data_.emplace<decltype(params)>(std::move(params));
  return result;
}

RequestValue RequestValue::CreateForOpenFileSuccess(
    extensions::api::file_system_provider_internal::OpenFileRequestedSuccess::
        Params params) {
  RequestValue result;
  result.data_.emplace<decltype(params)>(std::move(params));
  return result;
}

RequestValue RequestValue::CreateForOperationSuccess(
    extensions::api::file_system_provider_internal::OperationRequestedSuccess::
        Params params) {
  RequestValue result;
  result.data_.emplace<decltype(params)>(std::move(params));
  return result;
}

RequestValue RequestValue::CreateForOperationError(
    extensions::api::file_system_provider_internal::OperationRequestedError::
        Params params) {
  RequestValue result;
  result.data_.emplace<decltype(params)>(std::move(params));
  return result;
}

RequestValue RequestValue::CreateForTesting(const std::string& params) {
  RequestValue result;
  result.data_.emplace<std::string>(params);
  return result;
}

}  // namespace ash::file_system_provider
