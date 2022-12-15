// Copyright 2020 The Chromium Authors
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

base::Value::Dict ListPublicCertificatesRequestToReadableDictionary(
    const nearbyshare::proto::ListPublicCertificatesRequest& request) {
  base::Value::Dict dict;
  dict.Set("parent", request.parent());
  dict.Set("page_size", request.page_size());
  dict.Set("page_token", request.page_token());

  base::Value::List secret_ids_list;
  for (const auto& secret_id : request.secret_ids()) {
    secret_ids_list.Append(TruncateString(Encode(secret_id)));
  }
  dict.Set("secret_ids", std::move(secret_ids_list));
  return dict;
}

base::Value::Dict ListPublicCertificatesResponseToReadableDictionary(
    const nearbyshare::proto::ListPublicCertificatesResponse& response) {
  base::Value::Dict dict;
  dict.Set("next_page_token", response.next_page_token());

  base::Value::List public_certificates_list;
  for (const auto& public_certificate : response.public_certificates()) {
    public_certificates_list.Append(
        PublicCertificateToReadableDictionary(public_certificate));
  }
  dict.Set("public_certificates", std::move(public_certificates_list));
  return dict;
}

base::Value::Dict PublicCertificateToReadableDictionary(
    const nearbyshare::proto::PublicCertificate& certificate) {
  base::Value::Dict dict;
  dict.Set("secret_id", TruncateString(Encode(certificate.secret_id())));
  dict.Set("secret_key", TruncateString(Encode(certificate.secret_key())));
  dict.Set("public_key", TruncateString(Encode(certificate.public_key())));
  dict.Set("start_time",
           TimestampToReadableDictionary(certificate.start_time()));
  dict.Set("end_time", TimestampToReadableDictionary(certificate.end_time()));
  dict.Set("for_selected_contacts", certificate.for_selected_contacts());
  dict.Set("metadata_encryption_key",
           TruncateString(Encode(certificate.metadata_encryption_key())));
  dict.Set("encrypted_metadata_bytes",
           TruncateString(Encode(certificate.encrypted_metadata_bytes())));
  dict.Set("metadata_encryption_key_tag",
           TruncateString(Encode(certificate.metadata_encryption_key_tag())));
  dict.Set("for_self_share", certificate.for_self_share());
  return dict;
}

base::Value::Dict TimestampToReadableDictionary(
    const nearbyshare::proto::Timestamp& timestamp) {
  base::Value::Dict dict;
  dict.Set("seconds", base::NumberToString(timestamp.seconds()));
  dict.Set("nanos", base::NumberToString(timestamp.nanos()));
  return dict;
}

base::Value::Dict ListContactPeopleRequestToReadableDictionary(
    const nearbyshare::proto::ListContactPeopleRequest& request) {
  base::Value::Dict dict;
  dict.Set("page_size", request.page_size());
  dict.Set("page_token", request.page_token());
  return dict;
}

base::Value::Dict ListContactPeopleResponseToReadableDictionary(
    const nearbyshare::proto::ListContactPeopleResponse& response) {
  base::Value::Dict dict;
  base::Value::List contact_records_list;
  for (const auto& contact_record : response.contact_records()) {
    contact_records_list.Append(
        ContactRecordToReadableDictionary(contact_record));
  }
  dict.Set("contact_records", std::move(contact_records_list));
  dict.Set("next_page_token", response.next_page_token());
  return dict;
}

base::Value::Dict ContactRecordToReadableDictionary(
    const nearbyshare::proto::ContactRecord& contact_record) {
  base::Value::Dict dict;
  dict.Set("id", contact_record.id());
  dict.Set("person_name", contact_record.person_name());
  dict.Set("image_url", contact_record.image_url());
  base::Value::List identifiers_list;
  for (const auto& identifier : contact_record.identifiers()) {
    identifiers_list.Append(IdentifierToReadableDictionary(identifier));
  }
  dict.Set("identifiers", std::move(identifiers_list));
  return dict;
}

base::Value::Dict IdentifierToReadableDictionary(
    const nearbyshare::proto::Contact::Identifier& identifier) {
  base::Value::Dict dict;
  if (!identifier.obfuscated_gaia().empty()) {
    dict.Set("identifier", identifier.obfuscated_gaia());
  } else if (!identifier.phone_number().empty()) {
    dict.Set("identifier", identifier.phone_number());
  } else if (!identifier.account_name().empty()) {
    dict.Set("identifier", identifier.account_name());
  }
  return dict;
}

base::Value::Dict UpdateDeviceRequestToReadableDictionary(
    const nearbyshare::proto::UpdateDeviceRequest& request) {
  base::Value::Dict dict;
  dict.Set("device", DeviceToReadableDictionary(request.device()));
  dict.Set("update_mask", FieldMaskToReadableDictionary(request.update_mask()));
  return dict;
}

base::Value::Dict DeviceToReadableDictionary(
    const nearbyshare::proto::Device& device) {
  base::Value::Dict dict;
  dict.Set("name", device.name());
  dict.Set("display_name", device.display_name());
  base::Value::List contacts_list;
  for (const auto& contact : device.contacts()) {
    contacts_list.Append(ContactToReadableDictionary(contact));
  }
  dict.Set("contacts", std::move(contacts_list));
  base::Value::List public_certificates_list;
  for (const auto& certificate : device.public_certificates()) {
    public_certificates_list.Append(
        PublicCertificateToReadableDictionary(certificate));
  }
  dict.Set("public_certificates", std::move(public_certificates_list));
  return dict;
}

base::Value::Dict ContactToReadableDictionary(
    const nearbyshare::proto::Contact& contact) {
  base::Value::Dict dict;
  dict.Set("identifier", IdentifierToReadableDictionary(contact.identifier()));
  dict.Set("is_selected", contact.is_selected());
  return dict;
}

base::Value::Dict FieldMaskToReadableDictionary(
    const nearbyshare::proto::FieldMask& mask) {
  base::Value::Dict dict;
  base::Value::List paths_list;
  for (const auto& path : mask.paths()) {
    paths_list.Append(path);
  }
  dict.Set("paths", std::move(paths_list));
  return dict;
}

base::Value::Dict UpdateDeviceResponseToReadableDictionary(
    const nearbyshare::proto::UpdateDeviceResponse& response) {
  base::Value::Dict dict;
  dict.Set("device", DeviceToReadableDictionary(response.device()));
  dict.Set("person_name", response.person_name());
  dict.Set("image_url", response.image_url());
  dict.Set("image_token", response.image_token());
  return dict;
}

base::Value::Dict EncryptedMetadataToReadableDictionary(
    const nearbyshare::proto::EncryptedMetadata& data) {
  base::Value::Dict dict;
  dict.Set("device_name", data.device_name());
  dict.Set("full_name", data.full_name());
  dict.Set("icon_url", data.icon_url());
  dict.Set("bluetooth_mac_address",
           TruncateString(Encode(data.bluetooth_mac_address())));
  dict.Set("obfuscated_gaia_id", data.obfuscated_gaia_id());
  return dict;
}
