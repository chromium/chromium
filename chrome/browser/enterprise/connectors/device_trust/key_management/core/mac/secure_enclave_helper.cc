// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper_impl.h"

namespace enterprise_connectors {

namespace {

std::unique_ptr<SecureEnclaveHelper>* GetTestInstanceStorage() {
  static base::NoDestructor<std::unique_ptr<SecureEnclaveHelper>> storage;
  return storage.get();
}

}  // namespace

// static
std::unique_ptr<SecureEnclaveHelper> SecureEnclaveHelper::Create() {
  std::unique_ptr<SecureEnclaveHelper>& test_instance =
      *GetTestInstanceStorage();
  if (test_instance)
    return std::move(test_instance);
  return std::make_unique<SecureEnclaveHelperImpl>();
}

// static
void SecureEnclaveHelper::SetInstanceForTesting(
    std::unique_ptr<SecureEnclaveHelper> helper) {
  DCHECK(helper);
  *GetTestInstanceStorage() = std::move(helper);
}

}  // namespace enterprise_connectors
