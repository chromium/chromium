// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_cast_private/chrome_networking_cast_private_delegate.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_crypto.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

namespace {

const char kErrorEncryptionError[] = "Error.EncryptionError";

ChromeNetworkingCastPrivateDelegate::FactoryCallback* g_factory_callback =
    nullptr;

enum class VerificationResult { SUCCESS, VERIFY_FAILURE, DECODE_FAILURE };

// Called from a blocking pool task runner. Tries to decode and verify the
// provided credentials.
VerificationResult DecodeAndVerifyCredentials(
    const NetworkingCastPrivateDelegate::Credentials& credentials) {
  std::string decoded_signed_data;
  if (!base::Base64Decode(credentials.signed_data(), &decoded_signed_data)) {
    LOG(ERROR) << "Failed to decode signed data";
    return VerificationResult::DECODE_FAILURE;
  }

  if (networking_private_crypto::VerifyCredentials(
          credentials.certificate(), credentials.intermediate_certificates(),
          decoded_signed_data, credentials.unsigned_data(),
          credentials.device_bssid())) {
    return VerificationResult::SUCCESS;
  }

  return VerificationResult::VERIFY_FAILURE;
}

VerificationResult RunDecodeAndVerifyCredentials(
    std::unique_ptr<NetworkingCastPrivateDelegate::Credentials> credentials) {
  return DecodeAndVerifyCredentials(*credentials);
}

void VerifyDestinationCompleted(
    const NetworkingCastPrivateDelegate::VerifiedCallback& success_callback,
    const NetworkingCastPrivateDelegate::FailureCallback& failure_callback,
    VerificationResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result == VerificationResult::DECODE_FAILURE) {
    failure_callback.Run(kErrorEncryptionError);
    return;
  }

  success_callback.Run(result == VerificationResult::SUCCESS);
}

// Called from a blocking pool task runner. Returns |data| encoded using
// |credentials| on success, or an empty string on failure.
std::string RunVerifyAndEncryptData(
    const std::string& data,
    std::unique_ptr<NetworkingCastPrivateDelegate::Credentials> credentials) {
  if (DecodeAndVerifyCredentials(*credentials) != VerificationResult::SUCCESS)
    return std::string();

  std::string decoded_public_key;
  if (!base::Base64Decode(credentials->public_key(), &decoded_public_key)) {
    LOG(ERROR) << "Failed to decode public key";
    return std::string();
  }

  std::vector<uint8_t> public_key_data(decoded_public_key.begin(),
                                       decoded_public_key.end());
  std::vector<uint8_t> ciphertext;
  if (!networking_private_crypto::EncryptByteString(public_key_data, data,
                                                    &ciphertext)) {
    LOG(ERROR) << "Failed to encrypt data";
    return std::string();
  }

  std::string base64_encoded_ciphertext;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(ciphertext.data()),
                        ciphertext.size()),
      &base64_encoded_ciphertext);
  return base64_encoded_ciphertext;
}

void VerifyAndEncryptDataCompleted(
    const NetworkingCastPrivateDelegate::DataCallback& success_callback,
    const NetworkingCastPrivateDelegate::FailureCallback& failure_callback,
    const std::string& encrypted_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (encrypted_data.empty())
    failure_callback.Run(kErrorEncryptionError);
  else
    success_callback.Run(encrypted_data);
}

// Called when NetworkingPrivateCredentialsGetter completes (from an arbitrary
// thread). Posts the result to the UI thread.
void CredentialsGetterCompleted(
    const NetworkingCastPrivateDelegate::DataCallback& success_callback,
    const NetworkingCastPrivateDelegate::FailureCallback& failure_callback,
    const std::string& key_data,
    const std::string& error) {
  if (!error.empty()) {
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindOnce(failure_callback, error));
  } else {
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindOnce(success_callback, key_data));
  }
}

// Called from a blocking pool task runner. Returns true if
// NetworkingPrivateCredentialsGetter is successfully started (which will
// invoke the appropriate callback when completed), or false if unable
// to start the getter (credentials or public key decode failed).
bool RunVerifyAndEncryptCredentials(
    const std::string& guid,
    std::unique_ptr<NetworkingCastPrivateDelegate::Credentials> credentials,
    const NetworkingCastPrivateDelegate::DataCallback& success_callback,
    const NetworkingCastPrivateDelegate::FailureCallback& failure_callback) {
  if (DecodeAndVerifyCredentials(*credentials) != VerificationResult::SUCCESS)
    return false;

  std::string decoded_public_key;
  if (!base::Base64Decode(credentials->public_key(), &decoded_public_key)) {
    LOG(ERROR) << "Failed to decode public key";
    return false;
  }

  // Start getting credentials. CredentialsGetterCompleted will be called on
  // completion. On Windows it will be called from a different thread after
  // |credentials_getter| is deleted.
  std::unique_ptr<NetworkingPrivateCredentialsGetter> credentials_getter(
      NetworkingPrivateCredentialsGetter::Create());
  credentials_getter->Start(guid, decoded_public_key,
                            base::Bind(&CredentialsGetterCompleted,
                                       success_callback, failure_callback));
  return true;
}

void VerifyAndEncryptCredentialsCompleted(
    const NetworkingCastPrivateDelegate::FailureCallback& failure_callback,
    bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If VerifyAndEncryptCredentials succeeded, then the appropriate callback
  // will be triggered from CredentialsGetterCompleted.
  if (succeeded)
    return;
  failure_callback.Run(kErrorEncryptionError);
}

}  // namespace

std::unique_ptr<ChromeNetworkingCastPrivateDelegate>
ChromeNetworkingCastPrivateDelegate::Create() {
  if (g_factory_callback)
    return g_factory_callback->Run();
  return std::unique_ptr<ChromeNetworkingCastPrivateDelegate>(
      new ChromeNetworkingCastPrivateDelegate);
}

void ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(
    FactoryCallback* factory_callback) {
  g_factory_callback = factory_callback;
}

ChromeNetworkingCastPrivateDelegate::ChromeNetworkingCastPrivateDelegate() {}

ChromeNetworkingCastPrivateDelegate::~ChromeNetworkingCastPrivateDelegate() {}

void ChromeNetworkingCastPrivateDelegate::VerifyDestination(
    std::unique_ptr<Credentials> credentials,
    const VerifiedCallback& success_callback,
    const FailureCallback& failure_callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&RunDecodeAndVerifyCredentials, base::Passed(&credentials)),
      base::Bind(&VerifyDestinationCompleted, success_callback,
                 failure_callback));
}

void ChromeNetworkingCastPrivateDelegate::VerifyAndEncryptCredentials(
    const std::string& guid,
    std::unique_ptr<Credentials> credentials,
    const DataCallback& success_callback,
    const FailureCallback& failure_callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&RunVerifyAndEncryptCredentials, guid,
                 base::Passed(&credentials), success_callback,
                 failure_callback),
      base::Bind(&VerifyAndEncryptCredentialsCompleted, failure_callback));
}

void ChromeNetworkingCastPrivateDelegate::VerifyAndEncryptData(
    const std::string& data,
    std::unique_ptr<Credentials> credentials,
    const DataCallback& success_callback,
    const FailureCallback& failure_callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&RunVerifyAndEncryptData, data, base::Passed(&credentials)),
      base::Bind(&VerifyAndEncryptDataCompleted, success_callback,
                 failure_callback));
}

}  // namespace extensions
