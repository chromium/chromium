// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace enterprise_connectors {

namespace {

constexpr char kErrorHistogramFormat[] =
    "Enterprise.DeviceTrust.Persistence.%s.Error";

std::string OperationToVariant(KeyPersistenceOperation operation) {
  switch (operation) {
    case KeyPersistenceOperation::kCheckPermissions:
      return "CheckPermissions";
    case KeyPersistenceOperation::kStoreKeyPair:
      return "StoreKeyPair";
    case KeyPersistenceOperation::kLoadKeyPair:
      return "LoadKeyPair";
    case KeyPersistenceOperation::kCreateKeyPair:
      return "CreateKeyPair";
  }
}

}  // namespace

void RecordError(KeyPersistenceOperation operation, KeyPersistenceError error) {
  base::UmaHistogramEnumeration(
      base::StringPrintf(kErrorHistogramFormat,
                         OperationToVariant(operation).c_str()),
      error);
}

}  // namespace enterprise_connectors
