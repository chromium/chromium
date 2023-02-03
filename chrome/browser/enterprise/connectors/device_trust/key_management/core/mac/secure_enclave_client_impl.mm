// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client_impl.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/metrics_util.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace enterprise_connectors {

namespace {

// Enum for the operation being performed on the Secure Enclave key. This is
// used for recording the key operation status.
enum Operation {
  CREATE,
  COPY,
  DELETE,
  UPDATE,
};

// Logs a SecureEnclaveOperationStatus metric for the type of `operation` being
// performed and the key storage `type`.
void LogKeyOperationFailure(Operation operation,
                            SecureEnclaveClient::KeyType type) {
  SecureEnclaveOperationStatus status;
  bool data_protection_keychain = false;
  if (@available(macOS 10.15, *))
    data_protection_keychain = true;

  switch (operation) {
    case Operation::CREATE:
      status = SecureEnclaveOperationStatus::kCreateSecureKeyFailed;
      break;
    case Operation::COPY:
      status = data_protection_keychain
                   ? SecureEnclaveOperationStatus::
                         kCopySecureKeyRefDataProtectionKeychainFailed
                   : SecureEnclaveOperationStatus::kCopySecureKeyRefFailed;
      break;
    case Operation::DELETE:
      status = data_protection_keychain
                   ? SecureEnclaveOperationStatus::
                         kDeleteSecureKeyDataProtectionKeychainFailed
                   : SecureEnclaveOperationStatus::kDeleteSecureKeyFailed;
      break;
    case Operation::UPDATE:
      status = data_protection_keychain
                   ? SecureEnclaveOperationStatus::
                         kUpdateSecureKeyLabelDataProtectionKeychainFailed
                   : SecureEnclaveOperationStatus::kUpdateSecureKeyLabelFailed;
      break;
  }

  RecordKeyOperationStatus(status, type);
}

// Returns the key label based on the key `type` if the key type is not
// supported an empty string is returned.
base::StringPiece GetLabelFromKeyType(SecureEnclaveClient::KeyType type) {
  if (type == SecureEnclaveClient::KeyType::kTemporary)
    return constants::kTemporaryDeviceTrustSigningKeyLabel;
  if (type == SecureEnclaveClient::KeyType::kPermanent)
    return constants::kDeviceTrustSigningKeyLabel;
  return base::StringPiece();
}

// Creates and returns the secure enclave private key attributes used
// for key creation. These key attributes represent the key created in
// the permanent key location.
base::ScopedCFTypeRef<CFMutableDictionaryRef> CreateAttributesForKey() {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> attributes(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));

  CFDictionarySetValue(
      attributes, kSecAttrAccessGroup,
      base::SysUTF8ToNSString(constants::kKeychainAccessGroup));
  CFDictionarySetValue(attributes, kSecAttrKeyType,
                       kSecAttrKeyTypeECSECPrimeRandom);
  CFDictionarySetValue(attributes, kSecAttrTokenID,
                       kSecAttrTokenIDSecureEnclave);
  CFDictionarySetValue(attributes, kSecAttrKeySizeInBits, @256);
  CFDictionarySetValue(
      attributes, kSecAttrLabel,
      base::SysUTF8ToCFStringRef(constants::kDeviceTrustSigningKeyLabel));

  base::ScopedCFTypeRef<CFMutableDictionaryRef> private_key_params(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(attributes, kSecPrivateKeyAttrs, private_key_params);
  CFDictionarySetValue(private_key_params, kSecAttrIsPermanent, @YES);
  CFDictionarySetValue(
      private_key_params, kSecAttrAccessControl,
      base::ScopedCFTypeRef<SecAccessControlRef>(
          SecAccessControlCreateWithFlags(
              kCFAllocatorDefault,
              // Private key can only be used when the device is unlocked.
              kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
              // Private key is available for signing.
              kSecAccessControlPrivateKeyUsage, nullptr)));
  return attributes;
}

// Creates the query used for querying the keychain for the secure key
// reference.
base::ScopedCFTypeRef<CFMutableDictionaryRef> CreateQueryForKey(
    SecureEnclaveClient::KeyType type) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  CFDictionarySetValue(query, kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom);
  CFDictionarySetValue(query, kSecAttrLabel,
                       base::SysUTF8ToCFStringRef(GetLabelFromKeyType(type)));
  CFDictionarySetValue(query, kSecReturnRef, @YES);

  // Specifying to query the data protection keychain is only available on
  // macOS 10.15 or newer. This forces a query to the correct keychain since
  // Secure Enclave keys are stored in the data protection keychain.
  if (@available(macOS 10.15, *)) {
    CFDictionarySetValue(query, kSecUseDataProtectionKeychain, @YES);
  }
  return query;
}

}  // namespace

SecureEnclaveClientImpl::SecureEnclaveClientImpl()
    : helper_(SecureEnclaveHelper::Create()) {
  DCHECK(helper_);
}

SecureEnclaveClientImpl::~SecureEnclaveClientImpl() = default;

base::ScopedCFTypeRef<SecKeyRef> SecureEnclaveClientImpl::CreatePermanentKey() {
  auto attributes = CreateAttributesForKey();
  if (!attributes)
    return base::ScopedCFTypeRef<SecKeyRef>();

  // Deletes a permanent Secure Enclave key if it exists from a previous
  // key rotation.
  DeleteKey(KeyType::kPermanent);
  auto key = helper_->CreateSecureKey(attributes);
  if (!key)
    LogKeyOperationFailure(Operation::CREATE, KeyType::kPermanent);

  return key;
}

base::ScopedCFTypeRef<SecKeyRef> SecureEnclaveClientImpl::CopyStoredKey(
    KeyType type) {
  auto key_ref = helper_->CopyKey(CreateQueryForKey(type));
  if (!key_ref)
    LogKeyOperationFailure(Operation::COPY, type);

  return key_ref;
}

bool SecureEnclaveClientImpl::UpdateStoredKeyLabel(KeyType current_key_type,
                                                   KeyType new_key_type) {
  // Deletes the `new_key_type` label if it exists in the keychain.
  DeleteKey(new_key_type);

  base::ScopedCFTypeRef<CFMutableDictionaryRef> attributes_to_update(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  auto label = GetLabelFromKeyType(new_key_type);
  if (label.empty())
    return false;

  CFDictionarySetValue(attributes_to_update, kSecAttrLabel,
                       base::SysUTF8ToCFStringRef(label));

  bool success = helper_->Update(CreateQueryForKey(current_key_type),
                                 attributes_to_update);
  if (!success)
    LogKeyOperationFailure(Operation::UPDATE, current_key_type);

  return success;
}

bool SecureEnclaveClientImpl::DeleteKey(KeyType type) {
  bool success = helper_->Delete(CreateQueryForKey(type));
  if (!success)
    LogKeyOperationFailure(Operation::DELETE, type);

  return success;
}

bool SecureEnclaveClientImpl::GetStoredKeyLabel(KeyType type,
                                                std::vector<uint8_t>& output) {
  if (!helper_->CopyKey(CreateQueryForKey(type))) {
    LogKeyOperationFailure(Operation::COPY, type);
    return false;
  }

  auto label = GetLabelFromKeyType(type);
  output.assign(label.begin(), label.end());
  return true;
}

bool SecureEnclaveClientImpl::ExportPublicKey(SecKeyRef key,
                                              std::vector<uint8_t>& output) {
  base::ScopedCFTypeRef<SecKeyRef> public_key =
      base::ScopedCFTypeRef<SecKeyRef>(SecKeyCopyPublicKey(key));
  base::ScopedCFTypeRef<CFErrorRef> error;
  base::ScopedCFTypeRef<CFDataRef> data_ref(
      SecKeyCopyExternalRepresentation(public_key, error.InitializeInto()));

  if (!data_ref)
    return false;

  auto data =
      base::make_span(CFDataGetBytePtr(data_ref),
                      base::checked_cast<size_t>(CFDataGetLength(data_ref)));
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), data.data(), data.size(),
                          /*ctx=*/nullptr)) {
    return false;
  }
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  EC_KEY_set_public_key(ec_key.get(), point.get());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get());

  uint8_t* der;
  size_t der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), pkey.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }

  output.assign(der, der + der_len);
  bssl::UniquePtr<uint8_t> delete_signed_cert_bytes(der);
  return true;
}

bool SecureEnclaveClientImpl::SignDataWithKey(SecKeyRef key,
                                              base::span<const uint8_t> data,
                                              std::vector<uint8_t>& output) {
  base::ScopedCFTypeRef<CFDataRef> data_ref(
      CFDataCreate(kCFAllocatorDefault, data.data(),
                   base::checked_cast<CFIndex>(data.size())));

  base::ScopedCFTypeRef<CFErrorRef> error;
  base::ScopedCFTypeRef<CFDataRef> signature(SecKeyCreateSignature(
      key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, data_ref,
      error.InitializeInto()));

  if (!signature)
    return false;

  output.assign(CFDataGetBytePtr(signature),
                CFDataGetBytePtr(signature) + CFDataGetLength(signature));
  return true;
}

bool SecureEnclaveClientImpl::VerifySecureEnclaveSupported() {
  return helper_->IsSecureEnclaveSupported();
}

}  // namespace enterprise_connectors
