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
    const nearby::sharing::proto::ListPublicCertificatesRequest& request) {
  base::Value::List secret_ids_list;
  for (const auto& secret_id : request.secret_ids()) {
    secret_ids_list.Append(TruncateString(Encode(secret_id)));
  }

  return base::Value::Dict()
      .Set("parent", request.parent())
      .Set("page_size", request.page_size())
      .Set("page_token", request.page_token())
      .Set("secret_ids", std::move(secret_ids_list));
}

base::Value::Dict ListPublicCertificatesResponseToReadableDictionary(
    const nearby::sharing::proto::ListPublicCertificatesResponse& response) {
  base::Value::List public_certificates_list;
  for (const auto& public_certificate : response.public_certificates()) {
    public_certificates_list.Append(
        PublicCertificateToReadableDictionary(public_certificate));
  }

  return base::Value::Dict()
      .Set("next_page_token", response.next_page_token())
      .Set("public_certificates", std::move(public_certificates_list));
}

base::Value::Dict PublicCertificateToReadableDictionary(
    const nearby::sharing::proto::PublicCertificate& certificate) {
  return base::Value::Dict()
      .Set("secret_id", TruncateString(Encode(certificate.secret_id())))
      .Set("secret_key", TruncateString(Encode(certificate.secret_key())))
      .Set("public_key", TruncateString(Encode(certificate.public_key())))
      .Set("start_time",
           TimestampToReadableDictionary(certificate.start_time()))
      .Set("end_time", TimestampToReadableDictionary(certificate.end_time()))
      .Set("for_selected_contacts", certificate.for_selected_contacts())
      .Set("metadata_encryption_key",
           TruncateString(Encode(certificate.metadata_encryption_key())))
      .Set("encrypted_metadata_bytes",
           TruncateString(Encode(certificate.encrypted_metadata_bytes())))
      .Set("metadata_encryption_key_tag",
           TruncateString(Encode(certificate.metadata_encryption_key_tag())))
      .Set("for_self_share", certificate.for_self_share());
}

base::Value::Dict TimestampToReadableDictionary(
    const nearby::sharing::proto::Timestamp& timestamp) {
  return base::Value::Dict()
      .Set("seconds", base::NumberToString(timestamp.seconds()))
      .Set("nanos", base::NumberToString(timestamp.nanos()));
}

base::Value::Dict ListContactPeopleRequestToReadableDictionary(
    const nearby::sharing::proto::ListContactPeopleRequest& request) {
  return base::Value::Dict()
      .Set("page_size", request.page_size())
      .Set("page_token", request.page_token());
}

base::Value::Dict ListContactPeopleResponseToReadableDictionary(
    const nearby::sharing::proto::ListContactPeopleResponse& response) {
  base::Value::List contact_records_list;
  for (const auto& contact_record : response.contact_records()) {
    contact_records_list.Append(
        ContactRecordToReadableDictionary(contact_record));
  }

  return base::Value::Dict()
      .Set("contact_records", std::move(contact_records_list))
      .Set("next_page_token", response.next_page_token());
}

base::Value::Dict ContactRecordToReadableDictionary(
    const nearby::sharing::proto::ContactRecord& contact_record) {
  base::Value::List identifiers_list;
  for (const auto& identifier : contact_record.identifiers()) {
    identifiers_list.Append(IdentifierToReadableDictionary(identifier));
  }

  return base::Value::Dict()
      .Set("id", contact_record.id())
      .Set("person_name", contact_record.person_name())
      .Set("image_url", contact_record.image_url())
      .Set("identifiers", std::move(identifiers_list));
}

base::Value::Dict IdentifierToReadableDictionary(
    const nearby::sharing::proto::Contact::Identifier& identifier) {
  base::Value::Dict dict;
  if (!identifier.obfuscated_gaia().empty()) {
    dict.Set("identifier", identifier.obfuscated_gaia());
    dict.Set("identifier", identifier.phone_number());
  } else if (!identifier.account_name().empty()) {
    dict.Set("identifier", identifier.account_name());
  }
  return dict;
}

base::Value::Dict UpdateDeviceRequestToReadableDictionary(
    const nearby::sharing::proto::UpdateDeviceRequest& request) {
  return base::Value::Dict()
      .Set("device", DeviceToReadableDictionary(request.device()))
      .Set("update_mask", FieldMaskToReadableDictionary(request.update_mask()));
}

base::Value::Dict DeviceToReadableDictionary(
    const nearby::sharing::proto::Device& device) {
  base::Value::List contacts_list;
  for (const auto& contact : device.contacts()) {
    contacts_list.Append(ContactToReadableDictionary(contact));
  }

  base::Value::List public_certificates_list;
  for (const auto& certificate : device.public_certificates()) {
    public_certificates_list.Append(
        PublicCertificateToReadableDictionary(certificate));
  }

  return base::Value::Dict()
      .Set("name", device.name())
      .Set("display_name", device.display_name())
      .Set("contacts", std::move(contacts_list))
      .Set("public_certificates", std::move(public_certificates_list));
}

base::Value::Dict ContactToReadableDictionary(
    const nearby::sharing::proto::Contact& contact) {
  return base::Value::Dict()
      .Set("identifier", IdentifierToReadableDictionary(contact.identifier()))
      .Set("is_selected", contact.is_selected());
}

base::Value::Dict FieldMaskToReadableDictionary(
    const nearby::sharing::proto::FieldMask& mask) {
  base::Value::List paths_list;
  for (const auto& path : mask.paths()) {
    paths_list.Append(path);
  }

  return base::Value::Dict().Set("paths", std::move(paths_list));
}

base::Value::Dict UpdateDeviceResponseToReadableDictionary(
    const nearby::sharing::proto::UpdateDeviceResponse& response) {
  return base::Value::Dict()
      .Set("device", DeviceToReadableDictionary(response.device()))
      .Set("person_name", response.person_name())
      .Set("image_url", response.image_url())
      .Set("image_token", response.image_token());
}

base::Value::Dict EncryptedMetadataToReadableDictionary(
    const nearby::sharing::proto::EncryptedMetadata& data) {
  return base::Value::Dict()
      .Set("device_name", data.device_name())
      .Set("full_name", data.full_name())
      .Set("icon_url", data.icon_url())
      .Set("bluetooth_mac_address",
           TruncateString(Encode(data.bluetooth_mac_address())))
      .Set("obfuscated_gaia_id", data.obfuscated_gaia_id())
      .Set("account_name", data.account_name());
}
