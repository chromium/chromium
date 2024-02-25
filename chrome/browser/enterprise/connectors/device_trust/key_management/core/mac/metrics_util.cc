// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace enterprise_connectors {

namespace {

constexpr char kSecureEnclaveOperationHistogramFormat[] =
    "Enterprise.DeviceTrust.Mac.SecureEnclaveOperation.%s";
constexpr char kKeychainOSStatusHistogramFormat[] =
    "Enterprise.DeviceTrust.Mac.KeychainOSStatus.%s.%s";

std::optional<SecureEnclaveOperationStatus> ConvertOperationToStatus(
    KeychainOperation operation) {
  switch (operation) {
    case KeychainOperation::kCreate:
      return SecureEnclaveOperationStatus::kCreateSecureKeyFailed;
    case KeychainOperation::kCopy:
      return SecureEnclaveOperationStatus::
          kCopySecureKeyRefDataProtectionKeychainFailed;
    case KeychainOperation::kDelete:
      return SecureEnclaveOperationStatus::
          kDeleteSecureKeyDataProtectionKeychainFailed;
    case KeychainOperation::kUpdate:
      return SecureEnclaveOperationStatus::
          kUpdateSecureKeyLabelDataProtectionKeychainFailed;
    default:
      return std::nullopt;
  }
}

std::string KeyTypeToString(SecureEnclaveClient::KeyType type) {
  switch (type) {
    case SecureEnclaveClient::KeyType::kPermanent:
      return "Permanent";
    case SecureEnclaveClient::KeyType::kTemporary:
      return "Temporary";
  }
}

std::string OperationToString(KeychainOperation operation) {
  switch (operation) {
    case KeychainOperation::kCreate:
      return "Create";
    case KeychainOperation::kCopy:
      return "Copy";
    case KeychainOperation::kDelete:
      return "Delete";
    case KeychainOperation::kUpdate:
      return "Update";
    case KeychainOperation::kExportPublicKey:
      return "ExportPublicKey";
    case KeychainOperation::kSignPayload:
      return "SignPayload";
  }
}

}  // namespace

void RecordKeyOperationStatus(KeychainOperation operation,
                              SecureEnclaveClient::KeyType type,
                              OSStatus error_code) {
  auto type_string = KeyTypeToString(type);
  auto operation_status = ConvertOperationToStatus(operation);
  if (operation_status) {
    base::UmaHistogramEnumeration(
        base::StringPrintf(kSecureEnclaveOperationHistogramFormat,
                           type_string.c_str()),
        operation_status.value());
  }

  base::UmaHistogramSparse(
      base::StringPrintf(kKeychainOSStatusHistogramFormat, type_string.c_str(),
                         OperationToString(operation).c_str()),
      error_code);
}

}  // namespace enterprise_connectors
