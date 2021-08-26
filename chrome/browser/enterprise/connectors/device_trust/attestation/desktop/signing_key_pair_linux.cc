// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

namespace enterprise_connectors {

// static
std::unique_ptr<SigningKeyPair> SigningKeyPair::CreatePlatformKeyPair() {
  // TODO(b/194891140)
  return nullptr;
}

}  // namespace enterprise_connectors
