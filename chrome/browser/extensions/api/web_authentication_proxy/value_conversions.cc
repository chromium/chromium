// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/value_conversions.h"

#include "base/base64url.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/cbor/reader.h"
#include "device/fido/attestation_object.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "url/gurl.h"

namespace extensions {

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

std::vector<uint8_t> ToByteVector(const std::string& in) {
  const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(in.data());
  return std::vector<uint8_t>(in_ptr, in_ptr + in.size());
}

base::Value ToValue(const device::PublicKeyCredentialRpEntity& relying_party) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey("id", relying_party.id);
  if (relying_party.name) {
    value.SetKey("name", base::Value(*relying_party.name));
  }
  if (relying_party.icon_url) {
    value.SetKey("icon_url", base::Value(relying_party.icon_url->spec()));
  }
  return value;
}

base::Value ToValue(const device::PublicKeyCredentialUserEntity& user) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey("id", Base64UrlEncode(user.id));
  if (user.name) {
    value.SetKey("name", base::Value(*user.name));
  }
  if (user.icon_url) {
    value.SetKey("icon_url", base::Value(user.icon_url->spec()));
  }
  if (user.display_name) {
    value.SetKey("display_name", base::Value(*user.display_name));
  }
  return value;
}

base::Value ToValue(
    const device::PublicKeyCredentialParams::CredentialInfo& params) {
  base::Value value(base::Value::Type::DICTIONARY);
  switch (params.type) {
    case device::CredentialType::kPublicKey:
      value.SetKey("type", base::Value(device::kPublicKey));
  }
  value.SetIntKey("algorithm", params.algorithm);
  return value;
}

base::Value ToValue(const device::PublicKeyCredentialDescriptor& descriptor) {
  base::Value value(base::Value::Type::DICTIONARY);
  switch (descriptor.credential_type) {
    case device::CredentialType::kPublicKey:
      value.SetKey("type", base::Value(device::kPublicKey));
  }
  value.SetStringKey("id", Base64UrlEncode(descriptor.id));
  std::vector<base::Value> transports;
  for (const device::FidoTransportProtocol& transport : descriptor.transports) {
    transports.emplace_back(base::Value(ToString(transport)));
  }
  value.SetKey("transports", base::Value(std::move(transports)));
  return value;
}

base::Value ToValue(
    const device::AuthenticatorAttachment& authenticator_attachment) {
  switch (authenticator_attachment) {
    case device::AuthenticatorAttachment::kAny:
      return base::Value();
    case device::AuthenticatorAttachment::kCrossPlatform:
      return base::Value("cross-platform");
    case device::AuthenticatorAttachment::kPlatform:
      return base::Value("platform");
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
  base::Value value(base::Value::Type::DICTIONARY);
  absl::optional<std::string> attachment;
  value.SetKey("authenticatorAttachment",
               ToValue(authenticator_selection.authenticator_attachment));
  value.SetKey("residentKey", ToValue(authenticator_selection.resident_key));
  value.SetKey("userVerification",
               ToValue(authenticator_selection.user_verification_requirement));
  return value;
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

absl::optional<device::FidoTransportProtocol> FidoTransportProtocolFromValue(
    const base::Value& value) {
  if (!value.is_string()) {
    return absl::nullopt;
  }
  return device::ConvertToFidoTransportProtocol(value.GetString());
}

absl::optional<device::AuthenticatorAttachment>
AuthenticatorAttachmentFromValue(const base::Value& value) {
  if (!value.is_none() && !value.is_string()) {
    return absl::nullopt;
  }
  if (value.is_none()) {
    return device::AuthenticatorAttachment::kAny;
  }
  const std::string& attachment_name = value.GetString();
  if (attachment_name == "platform") {
    return device::AuthenticatorAttachment::kPlatform;
  } else if (attachment_name == "cross-platform") {
    return device::AuthenticatorAttachment::kPlatform;
  }
  return absl::nullopt;
}

}  // namespace

base::Value ToValue(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("rp", ToValue(options->relying_party));
  value.SetKey("user", ToValue(options->user));
  value.SetStringKey("challenge", Base64UrlEncode(options->challenge));
  std::vector<base::Value> public_key_parameters;
  for (const device::PublicKeyCredentialParams::CredentialInfo& params :
       options->public_key_parameters) {
    public_key_parameters.push_back(ToValue(params));
  }
  value.SetKey("pubKeyCredParams",
               base::Value(std::move(public_key_parameters)));
  std::vector<base::Value> exclude_credentials;
  for (const device::PublicKeyCredentialDescriptor& descriptor :
       options->exclude_credentials) {
    exclude_credentials.push_back(ToValue(descriptor));
  }
  value.SetKey("excludeCredentials",
               base::Value(std::move(exclude_credentials)));
  if (options->authenticator_selection) {
    value.SetKey("authenticatorSelection",
                 ToValue(*options->authenticator_selection));
  }
  value.SetKey("attestation", ToValue(options->attestation));

  // TODO(https://crbug.com/1231802): Serialize extensions.

  return value;
}

blink::mojom::MakeCredentialAuthenticatorResponsePtr FromValue(
    const base::Value& value) {
  if (!value.is_dict()) {
    return nullptr;
  }

  const std::string* type = value.FindStringKey("type");
  if (!type || *type != device::kPublicKey) {
    return nullptr;
  }

  auto response = blink::mojom::MakeCredentialAuthenticatorResponse::New();
  response->info = blink::mojom::CommonCredentialInfo::New();

  const std::string* id = value.FindStringKey("id");
  if (!id) {
    return nullptr;
  }
  response->info->id = *id;
  const std::string* raw_id_b64url = value.FindStringKey("rawId");
  if (!id) {
    return nullptr;
  }
  std::string raw_id_string;
  if (!Base64UrlDecode(*raw_id_b64url, &raw_id_string)) {
    return nullptr;
  }
  response->info->raw_id = ToByteVector(raw_id_string);

  const base::Value* authenticator_attachment_value =
      value.FindKey("authenticatorAttachment");
  if (!authenticator_attachment_value) {
    return nullptr;
  }
  absl::optional<device::AuthenticatorAttachment> authenticator_attachment =
      AuthenticatorAttachmentFromValue(*authenticator_attachment_value);
  if (!authenticator_attachment) {
    return nullptr;
  }
  response->authenticator_attachment = *authenticator_attachment;

  const base::Value* response_dict = value.FindDictKey("response");
  if (!response_dict) {
    return nullptr;
  }

  const std::string* client_data_json_b64url =
      response_dict->FindStringKey("clientDataJSON");
  if (!client_data_json_b64url) {
    return nullptr;
  }
  std::string client_data_json;
  if (!Base64UrlDecode(*client_data_json_b64url, &client_data_json)) {
    return nullptr;
  }
  response->info->client_data_json = ToByteVector(client_data_json);

  const std::string* attestation_object_b64url =
      response_dict->FindStringKey("attestationObject");
  if (!attestation_object_b64url) {
    return nullptr;
  }
  std::string attestation_object_bytes;
  if (!Base64UrlDecode(*attestation_object_b64url, &attestation_object_bytes)) {
    return nullptr;
  }
  absl::optional<cbor::Value> attestation_object_cbor = cbor::Reader::Read(
      base::as_bytes(base::make_span(attestation_object_bytes)));
  if (!attestation_object_cbor) {
    return nullptr;
  }
  absl::optional<device::AttestationObject> attestation_object =
      device::AttestationObject::Parse(*attestation_object_cbor);
  if (!attestation_object) {
    return nullptr;
  }
  response->attestation_object = ToByteVector(attestation_object_bytes);
  response->info->authenticator_data =
      attestation_object->authenticator_data().SerializeToByteArray();

  const base::Value* transports = response_dict->FindListKey("transports");
  if (!transports) {
    return nullptr;
  }
  for (const base::Value& transport_name : transports->GetList()) {
    absl::optional<device::FidoTransportProtocol> transport =
        FidoTransportProtocolFromValue(transport_name);
    if (!transport) {
      return nullptr;
    }
    response->transports.push_back(*transport);
  }

  // TODO(https://crbug.com/1231802): Parse getClientExtensionResults().

  return response;
}

}  // namespace extensions
