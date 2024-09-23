// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_upload_request.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/unexportable_key.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

constexpr int kMaxDMTokenLength = 4096;

BPKUR::KeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return BPKUR::RSA_KEY;
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return BPKUR::EC_KEY;
  }
}

bool AreParametersValid(const GURL& dm_server_url,
                        const std::string& dm_token,
                        const SigningKeyPair& key_pair) {
  return dm_server_url.is_valid() && !dm_token.empty() &&
         dm_token.size() <= kMaxDMTokenLength && !key_pair.is_empty();
}

}  // namespace

KeyUploadRequest::KeyUploadRequest(const GURL& dm_server_url,
                                   const std::string& dm_token,
                                   const std::string& request_body)
    : dm_server_url_(dm_server_url),
      dm_token_(dm_token),
      request_body_(request_body) {
  DCHECK(dm_server_url_.is_valid());
  DCHECK(!dm_token_.empty());
  DCHECK(!request_body_.empty());
}

// static
std::optional<const enterprise_management::DeviceManagementRequest>
KeyUploadRequest::BuildUploadPublicKeyRequest(
    const SigningKeyPair& new_key_pair,
    const SigningKeyPair* old_key_pair,
    std::optional<std::string> nonce) {
  // A nonce is only needed in key rotation scenarios.
  DCHECK_EQ(!!old_key_pair, nonce.has_value());

  std::vector<uint8_t> pubkey = new_key_pair.key()->GetSubjectPublicKeyInfo();

  // Build the buffer to sign. It consists of the public key of the new key
  // pair followed by the nonce. The nonce vector may be empty.
  std::vector<uint8_t> buffer = pubkey;
  if (nonce) {
    buffer.insert(buffer.end(), nonce->begin(), nonce->end());
  }

  // If there is an existing key and the nonce is not empty, sign the new
  // pubkey with it. Otherwise sign it with the new key itself (i.e. the
  // public key is self-signed). This is done to handle the case of a device
  // that is enabled for device trust and then un-enrolled server side. When
  // the user re-enrolls this device, the first key rotation attempt will use
  // an empty nonce to signal this is the first public key being uploaded to
  // DM server. DM server expects the public key to be self signed.
  std::optional<std::vector<uint8_t>> signature =
      old_key_pair ? old_key_pair->key()->SignSlowly(buffer)
                   : new_key_pair.key()->SignSlowly(buffer);
  if (!signature.has_value()) {
    return std::nullopt;
  }

  enterprise_management::DeviceManagementRequest overall_request;
  auto* request = overall_request.mutable_browser_public_key_upload_request();

  request->set_public_key(pubkey.data(), pubkey.size());
  request->set_signature(signature->data(), signature->size());
  request->set_key_trust_level(new_key_pair.trust_level());
  request->set_key_type(AlgorithmToType(new_key_pair.key()->Algorithm()));

  return overall_request;
}

// static
std::optional<const KeyUploadRequest> KeyUploadRequest::Create(
    const GURL& dm_server_url,
    const std::string& dm_token,
    const SigningKeyPair& key_pair) {
  if (!AreParametersValid(dm_server_url, dm_token, key_pair)) {
    return std::nullopt;
  }

  std::optional<enterprise_management::DeviceManagementRequest>
      overall_request = BuildUploadPublicKeyRequest(key_pair);
  std::string request_body;
  if (overall_request && overall_request->SerializeToString(&request_body) &&
      !request_body.empty()) {
    return KeyUploadRequest(dm_server_url, dm_token, request_body);
  }
  return std::nullopt;
}

// static
std::optional<const KeyUploadRequest> KeyUploadRequest::Create(
    const GURL& dm_server_url,
    const std::string& dm_token,
    const SigningKeyPair& new_key_pair,
    const SigningKeyPair& old_key_pair,
    const std::string& nonce) {
  if (!AreParametersValid(dm_server_url, dm_token, new_key_pair) ||
      old_key_pair.is_empty() || nonce.empty()) {
    return std::nullopt;
  }

  std::optional<enterprise_management::DeviceManagementRequest>
      overall_request =
          BuildUploadPublicKeyRequest(new_key_pair, &old_key_pair, nonce);

  std::string request_body;
  if (overall_request && overall_request->SerializeToString(&request_body) &&
      !request_body.empty()) {
    return KeyUploadRequest(dm_server_url, dm_token, request_body);
  }

  return std::nullopt;
}

}  // namespace enterprise_connectors
