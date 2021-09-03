// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/os_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/crypto_utility.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/common/proto/device_trust_report_event.pb.h"
#include "crypto/random.h"
#include "crypto/unexportable_key.h"

#if BUILDFLAG(IS_WIN)

#include <comutil.h>
#include <objbase.h>
#include <oleauto.h>
#include <unknwn.h>
#include <windows.h>
#include <winerror.h>
#include <wrl/client.h>

#include "base/win/scoped_bstr.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "google_update/google_update_idl.h"

#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_connectors {

namespace {

// Size of nonce for challenge response.
const size_t kChallengResponseNonceBytesSize = 32;

#if BUILDFLAG(IS_WIN)

// Explicitly allow impersonating the client since some COM code
// elsewhere in the browser process may have previously used
// CoInitializeSecurity to set the impersonation level to something other than
// the default. Ignore errors since an attempt to use Google Update may succeed
// regardless.
void ConfigureProxyBlanket(IUnknown* interface_pointer) {
  ::CoSetProxyBlanket(
      interface_pointer, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
}

#endif

}  // namespace

DesktopAttestationService::DesktopAttestationService(
    std::unique_ptr<SigningKeyPair> key_pair)
    : key_pair_(std::move(key_pair)) {
  DCHECK(key_pair_);
}

DesktopAttestationService::~DesktopAttestationService() = default;

bool DesktopAttestationService::ChallengeComesFromVerifiedAccess(
    const std::string& serialized_signed_data,
    const std::string& public_key_modulus_hex) {
  SignedData signed_challenge;
  signed_challenge.ParseFromString(serialized_signed_data);
  // Verify challenge signature.
  return CryptoUtility::VerifySignatureUsingHexKey(
      public_key_modulus_hex, signed_challenge.data(),
      signed_challenge.signature());
}

std::string DesktopAttestationService::ExportPublicKey() {
  std::vector<uint8_t> public_key_info;
  if (!key_pair_->ExportPublicKey(&public_key_info))
    return std::string();
  return std::string(public_key_info.begin(), public_key_info.end());
}

void DesktopAttestationService::BuildChallengeResponseForVAChallenge(
    const std::string& challenge,
    std::unique_ptr<DeviceTrustSignals> signals,
    AttestationCallback callback) {
  DCHECK(!ExportPublicKey().empty());
  DCHECK(signals);
  DCHECK(signals->has_device_id() && !signals->device_id().empty());
  DCHECK(signals->has_obfuscated_customer_id() &&
         !signals->obfuscated_customer_id().empty());

  AttestationCallback reply = base::BindOnce(
      &DesktopAttestationService::ParseChallengeResponseAndRunCallback,
      weak_factory_.GetWeakPtr(), challenge, std::move(callback));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          &DesktopAttestationService::
              VerifyChallengeAndMaybeCreateChallengeResponse,
          base::Unretained(this), JsonChallengeToProtobufChallenge(challenge),
          google_keys_.va_signing_key(VAType::DEFAULT_VA).modulus_in_hex(),
          std::move(signals)),
      std::move(reply));
}

bool DesktopAttestationService::RotateSigningKey() {
#if BUILDFLAG(IS_WIN)
  // Get the CBCM DM token.  This will be needed later to send the new key's
  // public part to the server.
  auto dm_token = policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  if (!dm_token.is_valid())
    return false;

  Microsoft::WRL::ComPtr<IGoogleUpdate3Web> google_update;
  HRESULT hr = ::CoCreateInstance(CLSID_GoogleUpdate3WebServiceClass, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&google_update));
  if (FAILED(hr))
    return false;

  ConfigureProxyBlanket(google_update.Get());
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  hr = google_update->createAppBundleWeb(&dispatch);
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
  hr = dispatch.As(&app_bundle);
  if (FAILED(hr))
    return false;

  dispatch.Reset();
  ConfigureProxyBlanket(app_bundle.Get());
  app_bundle->initialize();
  const wchar_t* app_guid = install_static::GetAppGuid();
  hr = app_bundle->createInstalledApp(base::win::ScopedBstr(app_guid).Get());
  if (FAILED(hr))
    return false;

  hr = app_bundle->get_appWeb(0, &dispatch);
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<IAppWeb> app;
  hr = dispatch.As(&app);
  if (FAILED(hr))
    return false;

  dispatch.Reset();
  ConfigureProxyBlanket(app.Get());
  hr = app->get_command(
      base::win::ScopedBstr(installer::kCmdRotateDeviceTrustKey).Get(),
      &dispatch);
  if (FAILED(hr) || !dispatch)
    return false;

  Microsoft::WRL::ComPtr<IAppCommandWeb> app_command;
  hr = dispatch.As(&app_command);
  if (FAILED(hr))
    return false;

  ConfigureProxyBlanket(app_command.Get());
  std::string token_base64;
  base::Base64Encode(dm_token.value(), &token_base64);
  VARIANT var;
  VariantInit(&var);
  _variant_t token_var = token_base64.c_str();
  hr = app_command->execute(token_var, var, var, var, var, var, var, var, var);
  if (FAILED(hr))
    return false;

  // TODO(crbug.com/823515): Get the status of the app command execution and
  // return a corresponding value for |success|. For now, assume that the call
  // to setup.exe succeeds.
  return true;
#else   // BUILDFLAG(IS_WIN)
  // TODO(b/194385359): Implement for mac.
  // TODO(b/194385515): Implement for linux.
  return false;
#endif  // BUILDFLAG(IS_WIN)
}

void DesktopAttestationService::StampReport(DeviceTrustReportEvent& report) {
  auto* credential = report.mutable_attestation_credential();
  credential->set_format(
      DeviceTrustReportEvent::Credential::EC_NID_X9_62_PRIME256V1_PUBLIC_DER);
  credential->set_credential(ExportPublicKey());
}

std::string
DesktopAttestationService::VerifyChallengeAndMaybeCreateChallengeResponse(
    const std::string& serialized_signed_data,
    const std::string& public_key_modulus_hex,
    std::unique_ptr<DeviceTrustSignals> signals) {
  if (!ChallengeComesFromVerifiedAccess(serialized_signed_data,
                                        public_key_modulus_hex)) {
    LOG(ERROR) << "Challenge signature verification did not succeed.";
    return std::string();
  }
  // If the verification that the challenge comes from Verified Access succeed,
  // generate the challenge response.
  SignEnterpriseChallengeRequest request;
  SignEnterpriseChallengeReply result;
  request.set_challenge(serialized_signed_data);
  request.set_va_type(VAType::DEFAULT_VA);
  SignEnterpriseChallenge(request, std::move(signals), &result);
  return result.challenge_response();
}

void DesktopAttestationService::ParseChallengeResponseAndRunCallback(
    const std::string& challenge,
    AttestationCallback callback,
    const std::string& challenge_response_proto) {
  if (challenge_response_proto != std::string()) {
    // Return to callback (throttle with the challenge response) with empty
    // challenge response.
    std::move(callback).Run(
        ProtobufChallengeToJsonChallenge(challenge_response_proto));
  } else {
    // Make challenge response
    std::move(callback).Run("");
  }
}

void DesktopAttestationService::SignEnterpriseChallenge(
    const SignEnterpriseChallengeRequest& request,
    std::unique_ptr<DeviceTrustSignals> signals,
    SignEnterpriseChallengeReply* result) {
  // Validate that the challenge is coming from the expected source.
  SignedData signed_challenge;
  if (!signed_challenge.ParseFromString(request.challenge())) {
    LOG(ERROR) << __func__ << ": Failed to parse signed challenge.";
    result->set_status(STATUS_INVALID_PARAMETER_ERROR);
    return;
  }
  KeyInfo key_info;
  // Fill `key_info` out for Chrome Browser.
  // TODO(crbug.com/1241870): Remove public key from signals.
  key_info.set_key_type(CBCM);
  key_info.set_browser_instance_public_key(ExportPublicKey());
  key_info.set_device_id(signals->device_id());
  key_info.set_customer_id(signals->obfuscated_customer_id());

  key_info.set_allocated_device_trust_signals(signals.release());

  ChallengeResponse response_pb;
  *response_pb.mutable_challenge() = signed_challenge;

  crypto::RandBytes(base::WriteInto(response_pb.mutable_nonce(),
                                    kChallengResponseNonceBytesSize + 1),
                    kChallengResponseNonceBytesSize);
  if (!EncryptEnterpriseKeyInfo(request.va_type(), key_info,
                                response_pb.mutable_encrypted_key_info())) {
    LOG(ERROR) << __func__ << ": Failed to encrypt KeyInfo.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }

  // Serialize and sign the response protobuf.
  std::string serialized;
  if (!response_pb.SerializeToString(&serialized)) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  // Sign data using the client generated key pair.
  if (!SignChallengeData(serialized, result->mutable_challenge_response())) {
    result->clear_challenge_response();
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
}

bool DesktopAttestationService::EncryptEnterpriseKeyInfo(
    VAType va_type,
    const KeyInfo& key_info,
    EncryptedData* encrypted_data) {
  std::string serialized;
  if (!key_info.SerializeToString(&serialized)) {
    LOG(ERROR) << "Failed to serialize key info.";
    return false;
  }

  std::string key;
  if (!CryptoUtility::EncryptWithSeed(serialized, encrypted_data, key)) {
    LOG(ERROR) << "EncryptWithSeed failed.";
    return false;
  }
  bssl::UniquePtr<RSA> rsa(CryptoUtility::GetRSA(
      google_keys_.va_encryption_key(va_type).modulus_in_hex()));
  if (!rsa)
    return false;
  if (!CryptoUtility::WrapKeyOAEP(
          key, rsa.get(), google_keys_.va_encryption_key(va_type).key_id(),
          encrypted_data)) {
    encrypted_data->Clear();
    return false;
  }
  return true;
}

bool DesktopAttestationService::SignChallengeData(const std::string& data,
                                                  std::string* response) {
  SignedData signed_data;
  signed_data.set_data(data);
  std::string signature;
  if (!key_pair_->SignMessage(data, signed_data.mutable_signature())) {
    LOG(ERROR) << __func__ << ": Failed to sign data.";
    return false;
  }
  if (!signed_data.SerializeToString(response)) {
    LOG(ERROR) << __func__ << ": Failed to serialize signed data.";
    return false;
  }
  return true;
}

}  // namespace enterprise_connectors
