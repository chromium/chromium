// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/value_conversions.h"

#include "base/base64url.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"

namespace extensions::webauthn_proxy {

namespace {

std::string Base64UrlEncode(base::span<const uint8_t> input) {
  // Byte strings, which appear in the WebAuthn IDL as ArrayBuffer or
  // ByteSource, are base64url-encoded without trailing '=' padding.
  std::string output;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(input.data()),
                        input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &output);
  return output;
}

bool Base64UrlDecode(base::StringPiece input, std::string* output) {
  return base::Base64UrlDecode(
      input, base::Base64UrlDecodePolicy::DISALLOW_PADDING, output);
}

// Base64url-decodes the value of `key` from `dict`. Returns `nullopt` if the
// key isn't present or decoding failed.
absl::optional<std::string> Base64UrlDecodeStringKey(
    const base::Value::Dict& dict,
    const std::string& key) {
  const std::string* b64url_data = dict.FindString(key);
  if (!b64url_data) {
    return absl::nullopt;
  }
  std::string decoded;
  if (!Base64UrlDecode(*b64url_data, &decoded)) {
    return absl::nullopt;
  }
  return decoded;
}

// Like `Base64UrlDecodeStringKey()` attempts to find and base64-decode the
// value of `key` in `dict`. However, the value may also be of
// `base::Value::Type::NONE`. Returns true on success and the decoded result if
// the value was a string. Returns `{false, absl::nullopt}` if the key wasn't
// found or if decoding the string failed.
//
// This is useful for extracting attributes that are defined as nullable
// ArrayBuffers in the WebIDL since the JS `null` value maps to
// `base::Value::Type::NONE`.
std::tuple<bool, absl::optional<std::string>> Base64UrlDecodeNullableStringKey(
    const base::Value::Dict& dict,
    const std::string& key) {
  const base::Value* value = dict.Find(key);
  if (!value || (!value->is_string() && !value->is_none())) {
    return {false, absl::nullopt};
  }
  if (value->is_none()) {
    return {true, absl::nullopt};
  }
  std::string decoded;
  if (!Base64UrlDecode(value->GetString(), &decoded)) {
    return {false, absl::nullopt};
  }
  return {true, decoded};
}

std::vector<uint8_t> ToByteVector(const std::string& in) {
  const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(in.data());
  return std::vector<uint8_t>(in_ptr, in_ptr + in.size());
}

base::Value ToValue(const device::PublicKeyCredentialRpEntity& relying_party) {
  base::Value::Dict value;
  value.Set("id", relying_party.id);
  // `PublicKeyCredentialEntity.name` is required in the IDL but optional on the
  // mojo struct.
  value.Set("name", relying_party.name.value_or(""));
  return base::Value(std::move(value));
}

base::Value ToValue(const device::PublicKeyCredentialUserEntity& user) {
  base::Value::Dict value;
  value.Set("id", Base64UrlEncode(user.id));
  // `PublicKeyCredentialEntity.name` is required in the IDL but optional on the
  // mojo struct.
  value.Set("name", user.name.value_or(""));
  if (user.display_name) {
    value.Set("displayName", *user.display_name);
  }
  return base::Value(std::move(value));
}

base::Value ToValue(
    const device::PublicKeyCredentialParams::CredentialInfo& params) {
  base::Value::Dict value;
  switch (params.type) {
    case device::CredentialType::kPublicKey:
      value.Set("type", device::kPublicKey);
  }
  value.Set("alg", params.algorithm);
  return base::Value(std::move(value));
}

base::Value ToValue(const device::PublicKeyCredentialDescriptor& descriptor) {
  base::Value::Dict value;
  switch (descriptor.credential_type) {
    case device::CredentialType::kPublicKey:
      value.Set("type", device::kPublicKey);
  }
  value.Set("id", Base64UrlEncode(descriptor.id));
  base::Value::List transports;
  for (const device::FidoTransportProtocol& transport : descriptor.transports) {
    transports.Append(ToString(transport));
  }
  if (!transports.empty()) {
    value.Set("transports", std::move(transports));
  }
  return base::Value(std::move(value));
}

base::Value ToValue(
    const device::AuthenticatorAttachment& authenticator_attachment) {
  switch (authenticator_attachment) {
    case device::AuthenticatorAttachment::kCrossPlatform:
      return base::Value("cross-platform");
    case device::AuthenticatorAttachment::kPlatform:
      return base::Value("platform");
    case device::AuthenticatorAttachment::kAny:
      // Any maps to the key being omitted, not a null value.
      NOTREACHED();
      return base::Value("invalid");
  }
}

base::Value ToValue(
    const device::ResidentKeyRequirement& resident_key_requirement) {
  switch (resident_key_requirement) {
    case device::ResidentKeyRequirement::kDiscouraged:
      return base::Value("discouraged");
    case device::ResidentKeyRequirement::kPreferred:
      return base::Value("preferred");
    case device::ResidentKeyRequirement::kRequired:
      return base::Value("required");
  }
}

base::Value ToValue(
    const device::UserVerificationRequirement& user_verification_requirement) {
  switch (user_verification_requirement) {
    case device::UserVerificationRequirement::kDiscouraged:
      return base::Value("discouraged");
    case device::UserVerificationRequirement::kPreferred:
      return base::Value("preferred");
    case device::UserVerificationRequirement::kRequired:
      return base::Value("required");
  }
}

base::Value ToValue(
    const device::AuthenticatorSelectionCriteria& authenticator_selection) {
  base::Value::Dict value;
  absl::optional<std::string> attachment;
  if (authenticator_selection.authenticator_attachment !=
      device::AuthenticatorAttachment::kAny) {
    value.Set("authenticatorAttachment",
              ToValue(authenticator_selection.authenticator_attachment));
  }
  value.Set("residentKey", ToValue(authenticator_selection.resident_key));
  value.Set("userVerification",
            ToValue(authenticator_selection.user_verification_requirement));
  return base::Value(std::move(value));
}

base::Value ToValue(const device::AttestationConveyancePreference&
                        attestation_conveyance_preference) {
  switch (attestation_conveyance_preference) {
    case device::AttestationConveyancePreference::kNone:
      return base::Value("none");
    case device::AttestationConveyancePreference::kIndirect:
      return base::Value("indirect");
    case device::AttestationConveyancePreference::kDirect:
      return base::Value("direct");
    case device::AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
    case device::AttestationConveyancePreference::
        kEnterpriseIfRPListedOnAuthenticator:
      return base::Value("enterprise");
  }
}

base::Value ToValue(const blink::mojom::RemoteDesktopClientOverride&
                        remote_desktop_client_override) {
  base::Value::Dict value;
  value.Set("origin", remote_desktop_client_override.origin.Serialize());
  value.Set("sameOriginWithAncestors",
            remote_desktop_client_override.same_origin_with_ancestors);
  return base::Value(std::move(value));
}

base::Value ToValue(const blink::mojom::ProtectionPolicy policy) {
  switch (policy) {
    case blink::mojom::ProtectionPolicy::UNSPECIFIED:
      NOTREACHED();
      return base::Value("invalid");
    case blink::mojom::ProtectionPolicy::NONE:
      return base::Value("userVerificationOptional");
    case blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED:
      return base::Value("userVerificationOptionalWithCredentialIDList");
    case blink::mojom::ProtectionPolicy::UV_REQUIRED:
      return base::Value("userVerificationRequired");
  }
}

base::Value ToValue(const device::LargeBlobSupport large_blob) {
  switch (large_blob) {
    case device::LargeBlobSupport::kNotRequested:
      NOTREACHED();
      return base::Value("invalid");
    case device::LargeBlobSupport::kRequired:
      return base::Value("required");
    case device::LargeBlobSupport::kPreferred:
      return base::Value("preferred");
  }
}

base::Value ToValue(const device::CableDiscoveryData& cable_authentication) {
  base::Value::Dict value;
  switch (cable_authentication.version) {
    case device::CableDiscoveryData::Version::INVALID:
      NOTREACHED();
      break;
    case device::CableDiscoveryData::Version::V1:
      value.Set("version", 1);
      value.Set("clientEid",
                Base64UrlEncode(cable_authentication.v1->client_eid));
      value.Set("authenticatorEid",
                Base64UrlEncode(cable_authentication.v1->authenticator_eid));
      value.Set("sessionPreKey",
                Base64UrlEncode(cable_authentication.v1->session_pre_key));
      break;
    case device::CableDiscoveryData::Version::V2:
      value.Set("version", 2);
      value.Set("clientEid",
                Base64UrlEncode(cable_authentication.v2->experiments));
      value.Set("authenticatorEid", "");
      value.Set("sessionPreKey",
                Base64UrlEncode(cable_authentication.v2->server_link_data));
      break;
  }
  return base::Value(std::move(value));
}

absl::optional<device::FidoTransportProtocol> FidoTransportProtocolFromValue(
    const base::Value& value) {
  if (!value.is_string()) {
    return absl::nullopt;
  }
  return device::ConvertToFidoTransportProtocol(value.GetString());
}

absl::optional<device::AuthenticatorAttachment>
NullableAuthenticatorAttachmentFromValue(const base::Value& value) {
  if (!value.is_none() && !value.is_string()) {
    return absl::nullopt;
  }
  if (value.is_none()) {
    // PublicKeyCredential.authenticatorAttachment can be `null`, which is
    // equivalent to `AuthenticatorAttachment::kAny`.
    return device::AuthenticatorAttachment::kAny;
  }
  const std::string& attachment_name = value.GetString();
  if (attachment_name == "platform") {
    return device::AuthenticatorAttachment::kPlatform;
  } else if (attachment_name == "cross-platform") {
    return device::AuthenticatorAttachment::kCrossPlatform;
  }
  return absl::nullopt;
}

}  // namespace

base::Value ToValue(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options) {
  base::Value::Dict value;
  value.Set("rp", ToValue(options->relying_party));
  value.Set("user", ToValue(options->user));
  value.Set("challenge", Base64UrlEncode(options->challenge));
  base::Value::List public_key_parameters;
  for (const device::PublicKeyCredentialParams::CredentialInfo& params :
       options->public_key_parameters) {
    public_key_parameters.Append(ToValue(params));
  }
  value.Set("pubKeyCredParams", std::move(public_key_parameters));
  base::Value::List exclude_credentials;
  for (const device::PublicKeyCredentialDescriptor& descriptor :
       options->exclude_credentials) {
    exclude_credentials.Append(ToValue(descriptor));
  }
  value.Set("excludeCredentials", std::move(exclude_credentials));
  if (options->authenticator_selection) {
    value.Set("authenticatorSelection",
              ToValue(*options->authenticator_selection));
  }
  value.Set("attestation", ToValue(options->attestation));

  base::Value::Dict& extensions =
      value.Set("extensions", base::Value::Dict())->GetDict();

  if (options->hmac_create_secret) {
    extensions.Set("hmacCreateSecret", true);
  }

  if (options->protection_policy !=
      blink::mojom::ProtectionPolicy::UNSPECIFIED) {
    extensions.Set("credentialProtectionPolicy",
                   ToValue(options->protection_policy));
    extensions.Set("enforceCredentialProtectionPolicy",
                   options->enforce_protection_policy);
  }

  if (options->appid_exclude) {
    extensions.Set("appIdExclude", *options->appid_exclude);
  }

  if (options->cred_props) {
    extensions.Set("credProps", true);
  }

  if (options->large_blob_enable != device::LargeBlobSupport::kNotRequested) {
    base::Value::Dict large_blob_value;
    large_blob_value.Set("support", ToValue(options->large_blob_enable));
    extensions.Set("largeBlob", std::move(large_blob_value));
  }

  DCHECK(!options->is_payment_credential_creation);

  if (options->cred_blob) {
    extensions.Set("credBlob", Base64UrlEncode(*options->cred_blob));
  }

  if (options->min_pin_length_requested) {
    extensions.Set("minPinLength", true);
  }

  if (options->remote_desktop_client_override) {
    extensions.Set("remoteDesktopClientOverride",
                   ToValue(*options->remote_desktop_client_override));
  }

  DCHECK(!options->prf_enable);

  return base::Value(std::move(value));
}

base::Value ToValue(
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options) {
  base::Value::Dict value;
  value.Set("challenge", Base64UrlEncode(options->challenge));
  value.Set("rpId", options->relying_party_id);

  base::Value::List allow_credentials;
  for (const device::PublicKeyCredentialDescriptor& descriptor :
       options->allow_credentials) {
    allow_credentials.Append(ToValue(descriptor));
  }
  value.Set("allowCredentials", std::move(allow_credentials));

  value.Set("userVerification", ToValue(options->user_verification));

  base::Value::Dict& extensions =
      value.Set("extensions", base::Value::Dict())->GetDict();

  if (options->appid) {
    extensions.Set("appid", *options->appid);
  }

  base::Value::List cable_authentication_data;
  for (const device::CableDiscoveryData& cable :
       options->cable_authentication_data) {
    cable_authentication_data.Append(ToValue(cable));
  }
  if (!cable_authentication_data.empty()) {
    extensions.Set("cableAuthentication", std::move(cable_authentication_data));
  }

  if (options->get_cred_blob) {
    extensions.Set("getCredBlob", true);
  }

  if (options->large_blob_read || options->large_blob_write) {
    base::Value::Dict large_blob_value;
    if (options->large_blob_read) {
      large_blob_value.Set("read", true);
    }
    if (options->large_blob_write) {
      large_blob_value.Set("write",
                           Base64UrlEncode(*options->large_blob_write));
    }
    extensions.Set("largeBlob", std::move(large_blob_value));
  }

  if (options->remote_desktop_client_override) {
    extensions.Set("remoteDesktopClientOverride",
                   ToValue(*options->remote_desktop_client_override));
  }

  DCHECK(!options->prf);

  return base::Value(std::move(value));
}

std::pair<blink::mojom::MakeCredentialAuthenticatorResponsePtr, std::string>
MakeCredentialResponseFromValue(const base::Value& value) {
  if (!value.is_dict()) {
    return {nullptr, "value is not a dict"};
  }

  const base::Value::Dict& dict = value.GetDict();
  const std::string* type = dict.FindString("type");
  if (!type || *type != device::kPublicKey) {
    return {nullptr, "invalid type"};
  }

  auto response = blink::mojom::MakeCredentialAuthenticatorResponse::New();
  response->info = blink::mojom::CommonCredentialInfo::New();

  const std::string* id = dict.FindString("id");
  if (!id) {
    return {nullptr, "invalid id"};
  }
  response->info->id = *id;
  absl::optional<std::string> raw_id = Base64UrlDecodeStringKey(dict, "rawId");
  if (!raw_id) {
    return {nullptr, "invalid rawId"};
  }
  response->info->raw_id = ToByteVector(*raw_id);

  const base::Value* authenticator_attachment_value =
      dict.Find("authenticatorAttachment");
  if (!authenticator_attachment_value) {
    return {nullptr, "invalid authenticatorAttachment"};
  }
  absl::optional<device::AuthenticatorAttachment> authenticator_attachment =
      NullableAuthenticatorAttachmentFromValue(*authenticator_attachment_value);
  if (!authenticator_attachment) {
    return {nullptr, "invalid authenticatorAttachment"};
  }
  response->authenticator_attachment = *authenticator_attachment;

  const base::Value::Dict* attestation_response = dict.FindDict("response");
  if (!attestation_response) {
    return {nullptr, "invalid response"};
  }

  absl::optional<std::string> authenticator_data =
      Base64UrlDecodeStringKey(*attestation_response, "authenticatorData");
  if (!authenticator_data) {
    return {nullptr, "invalid authenticatorData"};
  }
  response->info->authenticator_data = ToByteVector(*authenticator_data);

  absl::optional<std::string> attestation_object =
      Base64UrlDecodeStringKey(*attestation_response, "attestationObject");
  if (!attestation_object) {
    return {nullptr, "invalid attestationObject"};
  }
  response->attestation_object = ToByteVector(*attestation_object);

  absl::optional<std::string> client_data_json =
      Base64UrlDecodeStringKey(*attestation_response, "clientDataJSON");
  if (!client_data_json) {
    return {nullptr, "invalid clientDataJSON"};
  }
  response->info->client_data_json = ToByteVector(*client_data_json);

  // publicKey is required but nullable.
  auto [ok, opt_public_key] =
      Base64UrlDecodeNullableStringKey(*attestation_response, "publicKey");
  if (!ok) {
    return {nullptr, "invalid publicKey"};
  }
  if (opt_public_key) {
    response->public_key_der = ToByteVector(*opt_public_key);
  }

  absl::optional<int> public_key_algorithm =
      attestation_response->FindInt("publicKeyAlgorithm");
  if (!public_key_algorithm) {
    return {nullptr, "invalid publicKeyAlgorithm"};
  }
  response->public_key_algo = *public_key_algorithm;

  const base::Value::List* transports =
      attestation_response->FindList("transports");
  if (!transports) {
    return {nullptr, "invalid transports"};
  }
  for (const base::Value& transport_name : *transports) {
    absl::optional<device::FidoTransportProtocol> transport =
        FidoTransportProtocolFromValue(transport_name);
    if (!transport) {
      return {nullptr, "invalid transports"};
    }
    response->transports.push_back(*transport);
  }

  const base::Value::Dict* client_extension_results =
      dict.FindDict("clientExtensionResults");
  if (!client_extension_results) {
    return {nullptr, "invalid clientExtensionResults"};
  }
  absl::optional<bool> cred_blob =
      client_extension_results->FindBool("credBlob");
  if (cred_blob) {
    response->echo_cred_blob = true;
    response->cred_blob = *cred_blob;
  }
  const base::Value::Dict* cred_props =
      client_extension_results->FindDict("credProps");
  if (cred_props) {
    response->echo_cred_props = true;
    absl::optional<bool> rk = cred_props->FindBool("rk");
    if (rk) {
      response->has_cred_props_rk = true;
      response->cred_props_rk = *rk;
    }
  }
  const absl::optional<bool> hmac_create_secret =
      client_extension_results->FindBool("hmacCreateSecret");
  if (hmac_create_secret) {
    response->echo_hmac_create_secret = true;
    response->hmac_create_secret = *hmac_create_secret;
  }
  const base::Value::Dict* large_blob =
      client_extension_results->FindDict("largeBlob");
  if (large_blob) {
    response->echo_large_blob = true;
    const absl::optional<bool> supported = large_blob->FindBool("supported");
    if (!supported) {
      return {nullptr, "invalid largeBlob extension"};
    }
    response->supports_large_blob = *supported;
  }

  return {std::move(response), ""};
}

std::pair<blink::mojom::GetAssertionAuthenticatorResponsePtr, std::string>
GetAssertionResponseFromValue(const base::Value& value) {
  if (!value.is_dict()) {
    return {nullptr, "value is not a dict"};
  }

  const base::Value::Dict& dict = value.GetDict();
  const std::string* type = dict.FindString("type");
  if (!type || *type != device::kPublicKey) {
    return {nullptr, "invalid type"};
  }

  auto response = blink::mojom::GetAssertionAuthenticatorResponse::New();
  response->info = blink::mojom::CommonCredentialInfo::New();

  const std::string* id = dict.FindString("id");
  if (!id) {
    return {nullptr, "invalid id"};
  }
  response->info->id = *id;
  absl::optional<std::string> raw_id = Base64UrlDecodeStringKey(dict, "rawId");
  if (!raw_id) {
    return {nullptr, "invalid rawId"};
  }
  response->info->raw_id = ToByteVector(*raw_id);

  const base::Value* authenticator_attachment_value =
      dict.Find("authenticatorAttachment");
  if (!authenticator_attachment_value) {
    return {nullptr, "invalid authenticatorAttachment"};
  }
  absl::optional<device::AuthenticatorAttachment> authenticator_attachment =
      NullableAuthenticatorAttachmentFromValue(*authenticator_attachment_value);
  if (!authenticator_attachment) {
    return {nullptr, "invalid authenticatorAttachment"};
  }
  response->authenticator_attachment = *authenticator_attachment;

  const base::Value::Dict* assertion_response = dict.FindDict("response");
  if (!assertion_response) {
    return {nullptr, "invalid response"};
  }

  absl::optional<std::string> client_data_json =
      Base64UrlDecodeStringKey(*assertion_response, "clientDataJSON");
  if (!client_data_json) {
    return {nullptr, "invalid clientDataJSON"};
  }
  response->info->client_data_json = ToByteVector(*client_data_json);

  absl::optional<std::string> authenticator_data =
      Base64UrlDecodeStringKey(*assertion_response, "authenticatorData");
  if (!authenticator_data) {
    return {nullptr, "invalid authenticatorData"};
  }
  response->info->authenticator_data = ToByteVector(*authenticator_data);

  absl::optional<std::string> signature =
      Base64UrlDecodeStringKey(*assertion_response, "signature");
  if (!signature) {
    return {nullptr, "invalid signature"};
  }
  response->signature = ToByteVector(*signature);

  // userHandle is non-optional but nullable.
  auto [ok, opt_user_handle] =
      Base64UrlDecodeNullableStringKey(*assertion_response, "userHandle");
  if (!ok) {
    return {nullptr, "invalid userHandle"};
  }
  if (opt_user_handle) {
    response->user_handle = ToByteVector(*opt_user_handle);
  }

  const base::Value::Dict* client_extension_results =
      dict.FindDict("clientExtensionResults");
  if (!client_extension_results) {
    return {nullptr, "invalid clientExtensionResults"};
  }
  const absl::optional<bool> app_id =
      client_extension_results->FindBool("appid");
  if (app_id) {
    response->echo_appid_extension = true;
    response->appid_extension = *app_id;
  }
  if (client_extension_results->contains("getCredBlob")) {
    absl::optional<std::string> cred_blob =
        Base64UrlDecodeStringKey(*client_extension_results, "getCredBlob");
    if (!cred_blob) {
      return {nullptr, "invalid credBlob extension"};
    }
    response->get_cred_blob = ToByteVector(*cred_blob);
  }
  const base::Value::Dict* large_blob =
      client_extension_results->FindDict("largeBlob");
  if (large_blob) {
    response->echo_large_blob = true;
    if (large_blob->contains("blob")) {
      absl::optional<std::string> blob =
          Base64UrlDecodeStringKey(*large_blob, "blob");
      if (!blob) {
        return {nullptr, "invalid largeBlob extension"};
      }
      response->large_blob = ToByteVector(*blob);
    }
    const absl::optional<bool> written = large_blob->FindBool("written");
    if (written) {
      response->echo_large_blob_written = true;
      response->large_blob_written = *written;
    }
  }

  return {std::move(response), ""};
}

}  // namespace extensions::webauthn_proxy
