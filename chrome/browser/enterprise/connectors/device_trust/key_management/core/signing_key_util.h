// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"

namespace enterprise_connectors {

// Loads the signing key pair from disk and initializes it. Returns nullptr if
// no key was found. Uses the KeyPersistenceDelegateFactory's default delegate
// to load the key from persistence.
//
// This function does IO and heavy cryptographic calculations, do not call
// on the main thread.
scoped_refptr<SigningKeyPair> LoadPersistedKey();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_UTIL_H_
