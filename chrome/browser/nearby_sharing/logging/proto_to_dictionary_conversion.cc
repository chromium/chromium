// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/logging/proto_to_dictionary_conversion.h"

#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace {
std::string Encode(const std::string& str) {
  std::string encoded_string;
  base::Base64UrlEncode(str, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_string);
  return encoded_string;
}

std::string TruncateString(const std::string& str) {
  if (str.length() <= 10)
    return str;
  return str.substr(0, 5) + "..." + str.substr(str.length() - 5, str.length());
}
}  // namespace

base::Value ListPublicCertificatesRequestToReadableDictionary(
    const nearbyshare::proto::ListPublicCertificatesRequest& request) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("parent", request.parent());
  dict.SetIntKey("page_size", request.page_size());
  dict.SetStringKey("page_token", request.page_token());

  base::Value secret_ids_list(base::Value::Type::LIST);
  for (const auto& secret_id : request.secret_ids()) {
    secret_ids_list.Append(TruncateString(Encode(secret_id)));
  }
  dict.SetKey("secret_ids", std::move(secret_ids_list));
  return dict;
}

base::Value ListPublicCertificatesResponseToReadableDictionary(
    const nearbyshare::proto::ListPublicCertificatesResponse& response) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("next_page_token", response.next_page_token());

  base::Value public_certificates_list(base::Value::Type::LIST);
  for (const auto& public_certificate : response.public_certificates()) {
    public_certificates_list.Append(
        PublicCertificateToReadableDictionary(public_certificate));
  }
  dict.SetKey("public_certificates", std::move(public_certificates_list));
  return dict;
}

base::Value PublicCertificateToReadableDictionary(
    const nearbyshare::proto::PublicCertificate& certificate) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("secret_id",
                    TruncateString(Encode(certificate.secret_id())));
  dict.SetStringKey("secret_key",
                    TruncateString(Encode(certificate.secret_key())));
  dict.SetStringKey("public_key",
                    TruncateString(Encode(certificate.public_key())));
  dict.SetKey("start_time",
              TimestampToReadableDictionary(certificate.start_time()));
  dict.SetKey("end_time",
              TimestampToReadableDictionary(certificate.end_time()));
  dict.SetBoolKey("for_selected_contacts", certificate.for_selected_contacts());
  dict.SetStringKey(
      "metadata_encryption_key",
      TruncateString(Encode(certificate.metadata_encryption_key())));
  dict.SetStringKey(
      "encrypted_metadata_bytes",
      TruncateString(Encode(certificate.encrypted_metadata_bytes())));
  dict.SetStringKey(
      "metadata_encryption_key_tag",
      TruncateString(Encode(certificate.metadata_encryption_key_tag())));
  return dict;
}

base::Value TimestampToReadableDictionary(
    const nearbyshare::proto::Timestamp& timestamp) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("seconds", base::NumberToString(timestamp.seconds()));
  dict.SetStringKey("nanos", base::NumberToString(timestamp.nanos()));
  return dict;
}

base::Value ListContactPeopleRequestToReadableDictionary(
    const nearbyshare::proto::ListContactPeopleRequest& request) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("page_size", request.page_size());
  dict.SetStringKey("page_token", request.page_token());
  return dict;
}

base::Value ListContactPeopleResponseToReadableDictionary(
    const nearbyshare::proto::ListContactPeopleResponse& response) {
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value contact_records_list(base::Value::Type::LIST);
  for (const auto& contact_record : response.contact_records()) {
    contact_records_list.Append(
        ContactRecordToReadableDictionary(contact_record));
  }
  dict.SetKey("contact_records", std::move(contact_records_list));
  dict.SetStringKey("next_page_token", response.next_page_token());
  return dict;
}

base::Value ContactRecordToReadableDictionary(
    const nearbyshare::proto::ContactRecord& contact_record) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("id", contact_record.id());
  dict.SetStringKey("person_name", contact_record.person_name());
  dict.SetStringKey("image_url", contact_record.image_url());
  base::Value identifiers_list(base::Value::Type::LIST);
  for (const auto& identifier : contact_record.identifiers()) {
    identifiers_list.Append(IdentifierToReadableDictionary(identifier));
  }
  dict.SetKey("identifiers", std::move(identifiers_list));
  return dict;
}

base::Value IdentifierToReadableDictionary(
    const nearbyshare::proto::Contact::Identifier& identifier) {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (!identifier.obfuscated_gaia().empty()) {
    dict.SetStringKey("identifier", identifier.obfuscated_gaia());
  } else if (!identifier.phone_number().empty()) {
    dict.SetStringKey("identifier", identifier.phone_number());
  } else if (!identifier.account_name().empty()) {
    dict.SetStringKey("identifier", identifier.account_name());
  }
  return dict;
}

base::Value UpdateDeviceRequestToReadableDictionary(
    const nearbyshare::proto::UpdateDeviceRequest& request) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("device", DeviceToReadableDictionary(request.device()));
  dict.SetKey("update_mask",
              FieldMaskToReadableDictionary(request.update_mask()));
  return dict;
}

base::Value DeviceToReadableDictionary(
    const nearbyshare::proto::Device& device) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("name", device.name());
  dict.SetStringKey("display_name", device.display_name());
  base::Value contacts_list(base::Value::Type::LIST);
  for (const auto& contact : device.contacts()) {
    contacts_list.Append(ContactToReadableDictionary(contact));
  }
  dict.SetKey("contacts", std::move(contacts_list));
  base::Value public_certificates_list(base::Value::Type::LIST);
  for (const auto& certificate : device.public_certificates()) {
    public_certificates_list.Append(
        PublicCertificateToReadableDictionary(certificate));
  }
  dict.SetKey("public_certificates", std::move(public_certificates_list));
  return dict;
}

base::Value ContactToReadableDictionary(
    const nearbyshare::proto::Contact& contact) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("identifier",
              IdentifierToReadableDictionary(contact.identifier()));
  dict.SetBoolKey("is_selected", contact.is_selected());
  return dict;
}

base::Value FieldMaskToReadableDictionary(
    const nearbyshare::proto::FieldMask& mask) {
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value paths_list(base::Value::Type::LIST);
  for (const auto& path : mask.paths()) {
    paths_list.Append(path);
  }
  dict.SetKey("paths", std::move(paths_list));
  return dict;
}

base::Value UpdateDeviceResponseToReadableDictionary(
    const nearbyshare::proto::UpdateDeviceResponse& response) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("device", DeviceToReadableDictionary(response.device()));
  dict.SetStringKey("person_name", response.person_name());
  dict.SetStringKey("image_url", response.image_url());
  return dict;
}

base::Value EncryptedMetadataToReadableDictionary(
    const nearbyshare::proto::EncryptedMetadata& data) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("device_name", data.device_name());
  dict.SetStringKey("full_name", data.full_name());
  dict.SetStringKey("icon_url", data.icon_url());
  dict.SetStringKey("bluetooth_mac_address",
                    TruncateString(Encode(data.bluetooth_mac_address())));
  dict.SetStringKey("obfuscated_gaia_id", data.obfuscated_gaia_id());
  return dict;
}
