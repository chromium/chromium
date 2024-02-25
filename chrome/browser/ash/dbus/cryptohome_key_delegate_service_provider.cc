// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/cryptohome_key_delegate_service_provider.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/common_types.h"
#include "components/user_manager/known_user.h"
#include "dbus/message.h"
#include "extensions/common/extension_id.h"
#include "net/base/net_errors.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {

namespace {

// Converts the cryptohome challenge algorithm enum into the TLS 1.3
// SignatureScheme.
bool ChallengeSignatureAlgorithmToSslAlgorithm(
    cryptohome::ChallengeSignatureAlgorithm challenge_algorithm,
    uint16_t* ssl_algorithm) {
  switch (challenge_algorithm) {
    case cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA1:
      *ssl_algorithm = SSL_SIGN_RSA_PKCS1_SHA1;
      return true;
    case cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256:
      *ssl_algorithm = SSL_SIGN_RSA_PKCS1_SHA256;
      return true;
    case cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA384:
      *ssl_algorithm = SSL_SIGN_RSA_PKCS1_SHA384;
      return true;
    case cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA512:
      *ssl_algorithm = SSL_SIGN_RSA_PKCS1_SHA512;
      return true;
    default:
      LOG(ERROR) << "Unknown cryptohome key challenge algorithm: "
                 << challenge_algorithm;
      return false;
  }
}

// Completes the "ChallengeKey" D-Bus call of the |CHALLENGE_TYPE_SIGNATURE|
// type with the given signature, or with an error if the signature wasn't
// successfully generated.
void CompleteSignatureKeyChallenge(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    net::Error error,
    const std::vector<uint8_t>& signature) {
  if (error != net::OK || signature.empty()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED,
            "Failed to generate the signature"));
    return;
  }

  cryptohome::KeyChallengeResponse challenge_response;
  challenge_response.mutable_signature_response_data()->set_signature(
      signature.data(), signature.size());

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(challenge_response);

  std::move(response_sender).Run(std::move(response));
}

// Handles the "ChallengeKey" D-Bus call for the request of the
// |CHALLENGE_TYPE_SIGNATURE| type.
void HandleSignatureKeyChallenge(
    dbus::MethodCall* method_call,
    const cryptohome::SignatureKeyChallengeRequestData& challenge_request_data,
    const AccountId& account_id,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (challenge_request_data.data_to_sign().empty()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Missing data to sign"));
    return;
  }
  if (challenge_request_data.public_key_spki_der().empty()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Missing public key"));
    return;
  }
  if (!challenge_request_data.has_signature_algorithm()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Missing signature algorithm"));
    return;
  }

  uint16_t ssl_algorithm = 0;
  if (!ChallengeSignatureAlgorithmToSslAlgorithm(
          challenge_request_data.signature_algorithm(), &ssl_algorithm)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED, "Unknown signature algorithm"));
    return;
  }

  // Handle the challenge request by delivering it to one of the sign-in
  // certificateProvider subscribers (e.g., smart card middleware extensions).
  // The sign-in profile is used since it's where the needed extensions are
  // installed (e.g., for the smart card based login they are force-installed
  // via the DeviceLoginScreenExtensions admin policy).
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  chromeos::CertificateProviderService* certificate_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          signin_profile);
  if (!certificate_provider_service) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED,
            "Missing certificate provider service"));
    return;
  }

  std::vector<uint16_t> supported_ssl_algorithms;
  extensions::ExtensionId extension_id_ignored;
  if (!certificate_provider_service->LookUpSpki(
          challenge_request_data.public_key_spki_der(),
          &supported_ssl_algorithms, &extension_id_ignored)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "Key is unavailable"));
    return;
  }
  if (!base::Contains(supported_ssl_algorithms, ssl_algorithm)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "Unsupported algorithm"));
    return;
  }

  certificate_provider_service->RequestSignatureBySpki(
      challenge_request_data.public_key_spki_der(), ssl_algorithm,
      base::as_byte_span(challenge_request_data.data_to_sign()), account_id,
      base::BindOnce(&CompleteSignatureKeyChallenge,
                     base::Unretained(method_call),
                     std::move(response_sender)));
}

}  // namespace

CryptohomeKeyDelegateServiceProvider::CryptohomeKeyDelegateServiceProvider() =
    default;

CryptohomeKeyDelegateServiceProvider::~CryptohomeKeyDelegateServiceProvider() =
    default;

void CryptohomeKeyDelegateServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      cryptohome::kCryptohomeKeyDelegateInterface,
      cryptohome::kCryptohomeKeyDelegateChallengeKey,
      base::BindRepeating(
          &CryptohomeKeyDelegateServiceProvider::HandleChallengeKey,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce([](const std::string& interface_name,
                        const std::string& method_name, bool success) {
        LOG_IF(ERROR, !success)
            << "Failed to export " << interface_name << "." << method_name;
      }));
}

void CryptohomeKeyDelegateServiceProvider::HandleChallengeKey(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  cryptohome::AccountIdentifier account_identifier;
  if (!reader.PopArrayOfBytesAsProto(&account_identifier)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse AccountIdentifier from request"));
    return;
  }
  user_manager::KnownUser known_user(g_browser_process->local_state());
  user_manager::CryptohomeId cryptohome_id(account_identifier.account_id());
  const AccountId account_id =
      known_user.GetAccountIdByCryptohomeId(cryptohome_id);
  if (!account_id.is_valid() ||
      account_id.GetAccountType() == AccountType::UNKNOWN) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "Unknown account"));
    return;
  }

  cryptohome::KeyChallengeRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "Unable to parse KeyChallengeRequest from request"));
    return;
  }

  if (!request.has_challenge_type()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Missing challenge type"));
    return;
  }

  if (request.challenge_type() ==
      cryptohome::KeyChallengeRequest::CHALLENGE_TYPE_SIGNATURE) {
    if (!request.has_signature_request_data()) {
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, DBUS_ERROR_INVALID_ARGS,
              "Missing signature request data"));
      return;
    }
    HandleSignatureKeyChallenge(method_call, request.signature_request_data(),
                                account_id, std::move(response_sender));
    return;
  }

  std::move(response_sender)
      .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                               "Unknown challenge type"));
}

}  // namespace ash
