// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_GOOGLE_KEYS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_GOOGLE_KEYS_H_

#include <array>
#include <string>

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_google_key.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_interface.pb.h"

namespace enterprise_connectors {

// A class that manages the public keys along with their key IDs the attestation
// service uses.
class GoogleKeys {
 public:
  GoogleKeys();
  explicit GoogleKeys(const DefaultGoogleRsaPublicKeySet& default_key_set);
  ~GoogleKeys();

  // Copyable and movable with the default behavior.
  GoogleKeys(const GoogleKeys&);
  GoogleKeys& operator=(const GoogleKeys&) = default;
  GoogleKeys(GoogleKeys&&);
  GoogleKeys& operator=(GoogleKeys&&) = default;

  const GoogleRsaPublicKey& va_signing_key(VAType va_type) const;
  const GoogleRsaPublicKey& va_encryption_key(VAType va_type) const;

 private:
  std::array<GoogleRsaPublicKey, VAType_ARRAYSIZE> va_signing_keys_;
  std::array<GoogleRsaPublicKey, VAType_ARRAYSIZE> va_encryption_keys_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_GOOGLE_KEYS_H_
