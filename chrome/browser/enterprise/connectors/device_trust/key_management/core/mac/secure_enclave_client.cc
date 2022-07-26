// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client_impl.h"

namespace enterprise_connectors {

namespace {

std::unique_ptr<SecureEnclaveClient>* GetTestInstanceStorage() {
  static base::NoDestructor<std::unique_ptr<SecureEnclaveClient>> storage;
  return storage.get();
}

}  // namespace

// static
std::unique_ptr<SecureEnclaveClient> SecureEnclaveClient::Create() {
  std::unique_ptr<SecureEnclaveClient>& test_instance =
      *GetTestInstanceStorage();
  if (test_instance)
    return std::move(test_instance);
  return std::make_unique<SecureEnclaveClientImpl>();
}

// static
void SecureEnclaveClient::SetInstanceForTesting(
    std::unique_ptr<SecureEnclaveClient> client) {
  DCHECK(client);
  *GetTestInstanceStorage() = std::move(client);
}

}  // namespace enterprise_connectors
