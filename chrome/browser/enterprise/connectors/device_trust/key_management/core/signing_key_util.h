// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"

namespace enterprise_connectors {

struct LoadedKey {
  LoadedKey(scoped_refptr<enterprise_connectors::SigningKeyPair> key_pair,
            LoadPersistedKeyResult result);
  ~LoadedKey();

  LoadedKey(LoadedKey&&);
  LoadedKey& operator=(LoadedKey&&);

  LoadedKey& operator=(const LoadedKey& other) = delete;
  LoadedKey(const LoadedKey& other) = delete;

  scoped_refptr<SigningKeyPair> key_pair;
  LoadPersistedKeyResult result;
};

// Loads the signing key pair from disk and initializes it. Returns a struct
// containing the operation result as well as the key pair pointer, if it was
// successfully loaded. Uses the KeyPersistenceDelegateFactory's default
// delegate to load the key from persistence.
//
// This function does IO and heavy cryptographic calculations, do not call
// on the main thread.
LoadedKey LoadPersistedKey();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_UTIL_H_
