// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_UPLOAD_REQUEST_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_UPLOAD_REQUEST_H_

#include <optional>
#include <string>

#include "url/gurl.h"

namespace enterprise_management {
class DeviceManagementRequest;
}

namespace enterprise_connectors {

class SigningKeyPair;

class KeyUploadRequest {
 public:
  // Creates a request object for the simple key creation upload scenario using
  // the given request url `dm_server_url` and `dm_token` as authentication.
  // Uses `key_pair` to generate the request body. Returns std::nullopt if any
  // parameter is invalid, or if serialization of the request fails.
  static std::optional<const KeyUploadRequest> Create(
      const GURL& dm_server_url,
      const std::string& dm_token,
      const SigningKeyPair& key_pair);

  // Creates a request object for the key rotation upload scenario using the
  // given request url `dm_server_url` and `dm_token` as authentication. Uses
  // `new_key_pair`, `old_key_pair` and nonce to generate the rotation request
  // body. `nonce` is an opaque binary blob and should not be treated as an
  // ASCII or UTF-8 string. Returns std::nullopt if any parameter is invalid, or
  // if serialization of the request fails.
  static std::optional<const KeyUploadRequest> Create(
      const GURL& dm_server_url,
      const std::string& dm_token,
      const SigningKeyPair& new_key_pair,
      const SigningKeyPair& old_key_pair,
      const std::string& nonce);

  // Builds the public key upload key request using using `new_key_pair`,
  // `old_key_pair` and nonce to generate the rotation request body. `nonce` is
  // an opaque binary blob and should not be treated as an ASCII or UTF-8
  // string. Returns std::nullopt if any parameter is invalid. Old key pair and
  // nonce are only needed for rotation, and not for creating a new public key.
  static std::optional<const enterprise_management::DeviceManagementRequest>
  BuildUploadPublicKeyRequest(const SigningKeyPair& new_key_pair,
                              const SigningKeyPair* old_key_pair = nullptr,
                              std::optional<std::string> nonce = std::nullopt);

  const GURL& dm_server_url() const { return dm_server_url_; }
  const std::string& dm_token() const { return dm_token_; }
  const std::string& request_body() const { return request_body_; }

 protected:
  KeyUploadRequest(const GURL& dm_server_url,
                   const std::string& dm_token,
                   const std::string& request_body);

 private:
  const GURL dm_server_url_;
  const std::string dm_token_;
  const std::string request_body_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_UPLOAD_REQUEST_H_
