// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client_impl.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
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

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace enterprise_connectors {

namespace {

bool IsSuccess(OSStatus status) {
  return status == errSecSuccess;
}

// Creates and returns the secure enclave private key attributes used
// for key creation. These key attributes represent the key created in
// the permanent key location.
NSDictionary* CreateAttributesForKey() {
  base::apple::ScopedCFTypeRef<SecAccessControlRef> access_control(
      SecAccessControlCreateWithFlags(
          kCFAllocatorDefault,
          // Private key can only be used if the device was unlocked at least
          // once.
          kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
          // Private key is available for signing.
          kSecAccessControlPrivateKeyUsage, /*error=*/nullptr));

  NSDictionary* private_key_params = @{
    CFToNSPtrCast(kSecAttrIsPermanent) : @YES,
    CFToNSPtrCast(kSecAttrAccessControl) : (__bridge id)access_control.get(),
  };

  NSDictionary* attributes = @{
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(constants::kKeychainAccessGroup),
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecAttrTokenID) :
        CFToNSPtrCast(kSecAttrTokenIDSecureEnclave),
    CFToNSPtrCast(kSecAttrKeySizeInBits) : @256,
    CFToNSPtrCast(kSecAttrLabel) :
        base::SysUTF8ToNSString(constants::kDeviceTrustSigningKeyLabel),
    CFToNSPtrCast(kSecPrivateKeyAttrs) : private_key_params,
  };

  return attributes;
}

// Creates the query used for querying the keychain for the secure key
// reference.
NSDictionary* CreateQueryForKey(SecureEnclaveClient::KeyType type) {
  return @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassKey),
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecAttrLabel) :
        base::SysUTF8ToNSString(SecureEnclaveClient::GetLabelFromKeyType(type)),
    CFToNSPtrCast(kSecReturnRef) : @YES,
    CFToNSPtrCast(kSecUseDataProtectionKeychain) : @YES,
  };
}

// Converts an external representation of an EC public key from ANSI X9.63
// standard (using a byte string of 04 || X || Y) to a DER-encoded SPKI
// structure.
bool ConvertPublicKey(CFDataRef data_ref, std::vector<uint8_t>& output) {
  if (!data_ref) {
    return false;
  }

  auto data = base::apple::CFDataToSpan(data_ref);
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

}  // namespace

SecureEnclaveClientImpl::SecureEnclaveClientImpl()
    : helper_(SecureEnclaveHelper::Create()) {
  DCHECK(helper_);
}

SecureEnclaveClientImpl::~SecureEnclaveClientImpl() = default;

base::apple::ScopedCFTypeRef<SecKeyRef>
SecureEnclaveClientImpl::CreatePermanentKey() {
  NSDictionary* attributes = CreateAttributesForKey();
  if (!attributes)
    return base::apple::ScopedCFTypeRef<SecKeyRef>();

  // Deletes a permanent Secure Enclave key if it exists from a previous
  // key rotation.
  DeleteKey(KeyType::kPermanent);

  OSStatus status;
  auto key = helper_->CreateSecureKey(NSToCFPtrCast(attributes), &status);
  if (!key) {
    RecordKeyOperationStatus(KeychainOperation::kCreate, KeyType::kPermanent,
                             status);
  }

  return key;
}

base::apple::ScopedCFTypeRef<SecKeyRef> SecureEnclaveClientImpl::CopyStoredKey(
    KeyType type,
    OSStatus* error) {
  OSStatus status;
  auto key_ref =
      helper_->CopyKey(NSToCFPtrCast(CreateQueryForKey(type)), &status);
  if (!key_ref) {
    RecordKeyOperationStatus(KeychainOperation::kCopy, type, status);
    if (error) {
      *error = status;
    }
  }

  return key_ref;
}

bool SecureEnclaveClientImpl::UpdateStoredKeyLabel(KeyType current_key_type,
                                                   KeyType new_key_type) {
  // Deletes the `new_key_type` label if it exists in the keychain.
  DeleteKey(new_key_type);

  auto label = SecureEnclaveClient::GetLabelFromKeyType(new_key_type);
  if (label.empty())
    return false;

  NSDictionary* attributes_to_update =
      @{CFToNSPtrCast(kSecAttrLabel) : base::SysUTF8ToNSString(label)};

  OSStatus status =
      helper_->Update(NSToCFPtrCast(CreateQueryForKey(current_key_type)),
                      NSToCFPtrCast(attributes_to_update));

  bool success = IsSuccess(status);
  if (!success) {
    RecordKeyOperationStatus(KeychainOperation::kUpdate, current_key_type,
                             status);
  }

  return success;
}

bool SecureEnclaveClientImpl::DeleteKey(KeyType type) {
  OSStatus status = helper_->Delete(NSToCFPtrCast(CreateQueryForKey(type)));

  bool success = IsSuccess(status);
  if (!success) {
    RecordKeyOperationStatus(KeychainOperation::kDelete, type, status);
  }

  return success;
}

bool SecureEnclaveClientImpl::ExportPublicKey(SecKeyRef key,
                                              std::vector<uint8_t>& output,
                                              OSStatus* error) {
  base::apple::ScopedCFTypeRef<SecKeyRef> public_key(SecKeyCopyPublicKey(key));
  if (!public_key) {
    if (error) {
      // The API doesn't return any OSStatus, but we'll use errSecInvalidItemRef
      // for tracking purposes.
      *error = errSecInvalidItemRef;
    }
    return false;
  }

  base::apple::ScopedCFTypeRef<CFErrorRef> error_ref;
  base::apple::ScopedCFTypeRef<CFDataRef> data_ref(
      SecKeyCopyExternalRepresentation(public_key.get(),
                                       error_ref.InitializeInto()));

  if (!data_ref) {
    if (error) {
      // In the odd chance that the API did not populate `error_ref`, fallback
      // to errSecCoreFoundationUnknown.
      *error = error_ref ? CFErrorGetCode(error_ref.get())
                         : errSecCoreFoundationUnknown;
    }
    return false;
  }

  if (!ConvertPublicKey(data_ref.get(), output)) {
    if (error) {
      // This arithmetic function doesn't really interact with any OS API, but
      // we'll use errSecConversionError for tracking purposes.
      *error = errSecConversionError;
    }
    return false;
  }
  return true;
}

bool SecureEnclaveClientImpl::SignDataWithKey(SecKeyRef key,
                                              base::span<const uint8_t> data,
                                              std::vector<uint8_t>& output,
                                              OSStatus* error) {
  base::apple::ScopedCFTypeRef<CFDataRef> data_ref(
      CFDataCreate(kCFAllocatorDefault, data.data(),
                   base::checked_cast<CFIndex>(data.size())));

  base::apple::ScopedCFTypeRef<CFErrorRef> error_ref;
  base::apple::ScopedCFTypeRef<CFDataRef> signature(SecKeyCreateSignature(
      key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, data_ref.get(),
      error_ref.InitializeInto()));

  if (!signature) {
    if (error) {
      // In the odd chance that the API did not populate `error_ref`, fallback
      // to errSecCoreFoundationUnknown.
      *error = error_ref ? CFErrorGetCode(error_ref.get())
                         : errSecCoreFoundationUnknown;
    }
    return false;
  }

  auto signature_span = base::apple::CFDataToSpan(signature.get());
  output.assign(signature_span.begin(), signature_span.end());
  return true;
}

bool SecureEnclaveClientImpl::VerifySecureEnclaveSupported() {
  return helper_->IsSecureEnclaveSupported();
}

}  // namespace enterprise_connectors
