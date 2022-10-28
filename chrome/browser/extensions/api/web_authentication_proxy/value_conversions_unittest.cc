// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/value_conversions.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/time/time.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-shared.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace extensions::webauthn_proxy {
namespace {

using blink::mojom::CommonCredentialInfo;
using blink::mojom::CommonCredentialInfoPtr;
using blink::mojom::GetAssertionAuthenticatorResponse;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::MakeCredentialAuthenticatorResponse;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::RemoteDesktopClientOverride;
using blink::mojom::RemoteDesktopClientOverridePtr;

std::vector<uint8_t> ToByteVector(base::StringPiece in) {
  const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(in.data());
  return std::vector<uint8_t>(in_ptr, in_ptr + in.size());
}

constexpr char kAppId[] = "https://example.test/appid.json";
static const std::vector<uint8_t> kChallenge = ToByteVector("test challenge");
constexpr char kOrigin[] = "https://login.example.test/";
constexpr char kRpId[] = "example.test";
constexpr char kRpName[] = "Example LLC";
static const base::TimeDelta kTimeout = base::Seconds(30);
constexpr char kUserDisplayName[] = "Example User";
static const std::vector<uint8_t> kUserId = ToByteVector("test user id");
constexpr char kUserName[] = "user@example.test";

std::vector<device::PublicKeyCredentialParams::CredentialInfo>
GetPublicKeyCredentialParameters() {
  return {
      device::PublicKeyCredentialParams::CredentialInfo{
          device ::CredentialType::kPublicKey,
          base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256)},
      device::PublicKeyCredentialParams::CredentialInfo{
          device ::CredentialType::kPublicKey,
          base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kRs256)}};
}

std::vector<device::PublicKeyCredentialDescriptor> GetCredentialList() {
  return {device::PublicKeyCredentialDescriptor(
              device::CredentialType::kPublicKey, {20, 21, 22},
              {device::FidoTransportProtocol::kUsbHumanInterfaceDevice}),
          device::PublicKeyCredentialDescriptor(
              device::CredentialType::kPublicKey, {30, 31, 32}, {})};
}

TEST(WebAuthenticationProxyValueConversionsTest,
     PublicKeyCredentialCreationOptionsToValue) {
  // Exercise all supported fields.
  auto options = PublicKeyCredentialCreationOptions::New(
      device::PublicKeyCredentialRpEntity(kRpId, kRpName),
      device::PublicKeyCredentialUserEntity(kUserId, kUserName,
                                            kUserDisplayName),
      kChallenge, GetPublicKeyCredentialParameters(), kTimeout,
      GetCredentialList(),
      device::AuthenticatorSelectionCriteria(
          device::AuthenticatorAttachment::kPlatform,
          device::ResidentKeyRequirement::kRequired,
          device::UserVerificationRequirement::kRequired),
      device::AttestationConveyancePreference::kDirect,
      /*cable_registration_data=*/nullptr, /*hmac_create_secret=*/true,
      /*prf_enable=*/false, blink::mojom::ProtectionPolicy::UV_REQUIRED,
      /*enforce_protection_policy=*/true,
      /*appid_exclude=*/kAppId,
      /*cred_props=*/true, device::LargeBlobSupport::kRequired,
      /*is_payment_credential_creation=*/false,
      /*cred_blob=*/ToByteVector("test cred blob"),
      /*min_pin_length_requested=*/true,
      blink::mojom::RemoteDesktopClientOverride::New(
          url::Origin::Create(GURL(kOrigin)),
          /*same_origin_with_ancestors=*/true),
      // TODO(crbug.com/1356340): support devicePubKey in JSON when it's stable.
      /*device_public_key=*/nullptr);

  base::Value value = ToValue(options);
  std::string json;
  JSONStringValueSerializer serializer(&json);
  ASSERT_TRUE(serializer.Serialize(value));
  EXPECT_EQ(
      json,
      R"({"attestation":"direct","authenticatorSelection":{"authenticatorAttachment":"platform","residentKey":"required","userVerification":"required"},"challenge":"dGVzdCBjaGFsbGVuZ2U","excludeCredentials":[{"id":"FBUW","transports":["usb"],"type":"public-key"},{"id":"Hh8g","type":"public-key"}],"extensions":{"appIdExclude":"https://example.test/appid.json","credBlob":"dGVzdCBjcmVkIGJsb2I","credProps":true,"credentialProtectionPolicy":"userVerificationRequired","enforceCredentialProtectionPolicy":true,"hmacCreateSecret":true,"largeBlob":{"support":"required"},"minPinLength":true,"remoteDesktopClientOverride":{"origin":"https://login.example.test","sameOriginWithAncestors":true}},"pubKeyCredParams":[{"alg":-7,"type":"public-key"},{"alg":-257,"type":"public-key"}],"rp":{"id":"example.test","name":"Example LLC"},"user":{"displayName":"Example User","id":"dGVzdCB1c2VyIGlk","name":"user@example.test"}})");
}

TEST(WebAuthenticationProxyValueConversionsTest,
     PublicKeyCredentialRequestOptionsToValue) {
  // Exercise all supported fields.
  auto options = PublicKeyCredentialRequestOptions::New(
      /*is_conditional=*/false, kChallenge, kTimeout, kRpId,
      GetCredentialList(), device::UserVerificationRequirement::kRequired,
      kAppId,
      std::vector<device::CableDiscoveryData>{
          {device::CableDiscoveryData::Version::V1, device::CableEidArray{},
           device::CableEidArray{}, device::CableSessionPreKeyArray{}}},
      /*prf=*/false, std::vector<blink::mojom::PRFValuesPtr>(),
      /*large_blob_read=*/true,
      /*large_blob_write=*/std::vector<uint8_t>{8, 9, 10},
      /*get_cred_blob=*/true,
      blink::mojom::RemoteDesktopClientOverride::New(
          url::Origin::Create(GURL(kOrigin)),
          /*same_origin_with_ancestors=*/true),
      // TODO: support devicePubKey in JSON when it's stable.
      /*device_public_key=*/nullptr);

  base::Value value = ToValue(options);
  std::string json;
  JSONStringValueSerializer serializer(&json);
  ASSERT_TRUE(serializer.Serialize(value));
  EXPECT_EQ(
      json,
      R"({"allowCredentials":[{"id":"FBUW","transports":["usb"],"type":"public-key"},{"id":"Hh8g","type":"public-key"}],"challenge":"dGVzdCBjaGFsbGVuZ2U","extensions":{"appid":"https://example.test/appid.json","cableAuthentication":[{"authenticatorEid":"AAAAAAAAAAAAAAAAAAAAAA","clientEid":"AAAAAAAAAAAAAAAAAAAAAA","sessionPreKey":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA","version":1}],"getCredBlob":true,"largeBlob":{"read":true,"write":"CAkK"},"remoteDesktopClientOverride":{"origin":"https://login.example.test","sameOriginWithAncestors":true}},"rpId":"example.test","userVerification":"required"})");
}

TEST(WebAuthenticationProxyValueConversionsTest,
     AuthenticatorAttestationResponseFromValue) {
  // The following values appear Base64URL-encoded in `json`.
  static const std::vector<uint8_t> kAttestationObject =
      ToByteVector("test attestation object");
  static const std::vector<uint8_t> kAuthenticatorData =
      ToByteVector("test authenticator data");
  static const std::vector<uint8_t> kId = ToByteVector("test id");
  constexpr char kIdB64Url[] = "dGVzdCBpZA";  // base64url("test")
  static const std::vector<uint8_t> kClientDataJson =
      ToByteVector("test client data json");
  static const std::vector<uint8_t> kPublicKey =
      ToByteVector("test public key");

  // Exercise every possible field in the result struct.
  constexpr char kJson[] = R"({
  "authenticatorAttachment": "platform",
  "clientExtensionResults": {
    "credBlob": true,
    "credProps": { "rk": true },
    "hmacCreateSecret": true,
    "largeBlob": { "supported": true }
  },
  "id": "dGVzdCBpZA",
  "rawId": "dGVzdCBpZA",
  "response": {
    "authenticatorData": "dGVzdCBhdXRoZW50aWNhdG9yIGRhdGE",
    "attestationObject": "dGVzdCBhdHRlc3RhdGlvbiBvYmplY3Q",
    "clientDataJSON": "dGVzdCBjbGllbnQgZGF0YSBqc29u",
    "publicKey": "dGVzdCBwdWJsaWMga2V5",
    "publicKeyAlgorithm": -7,
    "transports": [ "usb" ]
  },
  "type": "public-key"
})";

  JSONStringValueDeserializer deserializer(kJson);
  std::string deserialize_error;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  ASSERT_TRUE(value) << deserialize_error;

  auto [response, error] = MakeCredentialResponseFromValue(*value);
  ASSERT_TRUE(response) << error;

  auto expected = MakeCredentialAuthenticatorResponse::New(
      CommonCredentialInfo::New(kIdB64Url, kId, kClientDataJson,
                                kAuthenticatorData),
      device::AuthenticatorAttachment::kPlatform, kAttestationObject,
      std::vector<device::FidoTransportProtocol>{
          device::FidoTransportProtocol::kUsbHumanInterfaceDevice},
      /*echo_hmac_create_secret=*/true, /*hmac_create_secret=*/true,
      /*echo_prf=*/false, /*prf=*/false, /*echo_cred_blob=*/true,
      /*cred_blob=*/true, /*public_key_der=*/kPublicKey,
      /*public_key_algo=*/-7,
      /*echo_cred_props=*/true, /*has_cred_props_rk=*/true,
      /*cred_props_rk=*/true, /*echo_large_blob=*/true,
      /*supports_large_blob=*/true,
      // TODO: support devicePubKey in JSON when it's stable.
      /*device_public_key=*/nullptr);

  EXPECT_EQ(response->info, expected->info);
  EXPECT_EQ(response->authenticator_attachment,
            expected->authenticator_attachment);
  EXPECT_EQ(response->attestation_object, expected->attestation_object);
  EXPECT_EQ(response->transports, expected->transports);
  EXPECT_EQ(response->echo_hmac_create_secret,
            expected->echo_hmac_create_secret);
  EXPECT_EQ(response->hmac_create_secret, expected->hmac_create_secret);
  EXPECT_EQ(response->echo_prf, expected->prf);
  EXPECT_EQ(response->echo_cred_blob, expected->echo_cred_blob);
  EXPECT_EQ(response->cred_blob, expected->cred_blob);
  EXPECT_EQ(response->public_key_der, expected->public_key_der);
  EXPECT_EQ(response->public_key_algo, expected->public_key_algo);
  EXPECT_EQ(response->echo_cred_props, expected->echo_cred_props);
  EXPECT_EQ(response->has_cred_props_rk, expected->has_cred_props_rk);
  EXPECT_EQ(response->cred_props_rk, expected->cred_props_rk);
  EXPECT_EQ(response->echo_large_blob, expected->echo_large_blob);
  EXPECT_EQ(response->supports_large_blob, expected->supports_large_blob);
  // Produce a failure even if the list above is missing any fields. But this
  // will not print any meaningful error.
  EXPECT_EQ(response, expected);
}

TEST(WebAuthenticationProxyValueConversionsTest,
     AuthenticatorAssertionResponseFromValue) {
  // The following values appear Base64URL-encoded in `json`.
  static const std::vector<uint8_t> kAuthenticatorData =
      ToByteVector("test authenticator data");
  static const std::vector<uint8_t> kId = ToByteVector("test id");
  constexpr char kIdB64Url[] = "dGVzdCBpZA";  // base64url(kId)
  static const std::vector<uint8_t> kClientDataJson =
      ToByteVector("test client data json");
  static const std::vector<uint8_t> kCredBlob = ToByteVector("test cred blob");
  static const std::vector<uint8_t> kLargeBlob =
      ToByteVector("test large blob");
  static const std::vector<uint8_t> kSignature = ToByteVector("test signature");
  static const std::vector<uint8_t> kUserHandle =
      ToByteVector("test user handle");

  // Exercise every possible field in the result struct.
  constexpr char kJson[] = R"({
  "authenticatorAttachment": "cross-platform",
  "clientExtensionResults": {
    "appid": true,
    "getCredBlob": "dGVzdCBjcmVkIGJsb2I",
    "largeBlob": {
      "blob": "dGVzdCBsYXJnZSBibG9i",
      "written": true
    }
  },
  "id": "dGVzdCBpZA",
  "rawId": "dGVzdCBpZA",
  "response": {
    "authenticatorData": "dGVzdCBhdXRoZW50aWNhdG9yIGRhdGE",
    "clientDataJSON": "dGVzdCBjbGllbnQgZGF0YSBqc29u",
    "signature": "dGVzdCBzaWduYXR1cmU",
    "userHandle": "dGVzdCB1c2VyIGhhbmRsZQ"
  },
  "type": "public-key"
})";

  JSONStringValueDeserializer deserializer(kJson);
  std::string deserialize_error;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  ASSERT_TRUE(value) << deserialize_error;

  auto [response, error] = GetAssertionResponseFromValue(*value);
  ASSERT_TRUE(response) << error;

  auto expected = GetAssertionAuthenticatorResponse::New(
      CommonCredentialInfo::New(kIdB64Url, kId, kClientDataJson,
                                kAuthenticatorData),
      device::AuthenticatorAttachment::kCrossPlatform, kSignature, kUserHandle,
      /*echo_appid_extension=*/true, /*appid_extension=*/true,
      /*echo_prf=*/false, /*prf_results=*/nullptr, /*prf_not_evaluated=*/false,
      /*echo_large_blob=*/true,
      /*large_blob=*/kLargeBlob, /*echo_large_blob_written=*/true,
      /*large_blob_written=*/true,
      /*get_cred_blob=*/kCredBlob,
      // TODO: support devicePubKey in JSON when it's stable.
      /*device_public_key=*/nullptr);

  EXPECT_EQ(response->info, expected->info);
  EXPECT_EQ(response->authenticator_attachment,
            expected->authenticator_attachment);
  EXPECT_EQ(response->signature, expected->signature);
  EXPECT_EQ(response->user_handle, expected->user_handle);
  EXPECT_EQ(response->echo_appid_extension, expected->echo_appid_extension);
  EXPECT_EQ(response->appid_extension, expected->appid_extension);
  EXPECT_EQ(response->echo_prf, expected->echo_prf);
  EXPECT_EQ(response->prf_results, expected->prf_results);
  EXPECT_EQ(response->prf_not_evaluated, expected->prf_not_evaluated);
  EXPECT_EQ(response->echo_large_blob, expected->echo_large_blob);
  EXPECT_EQ(response->large_blob, expected->large_blob);
  EXPECT_EQ(response->echo_large_blob_written,
            expected->echo_large_blob_written);
  EXPECT_EQ(response->large_blob_written, expected->large_blob_written);
  EXPECT_EQ(response->get_cred_blob, expected->get_cred_blob);
  // Produce a failure even if the list above is missing any fields. But this
  // will not print any meaningful error.
  EXPECT_EQ(response, expected);
}

}  // namespace
}  // namespace extensions::webauthn_proxy
