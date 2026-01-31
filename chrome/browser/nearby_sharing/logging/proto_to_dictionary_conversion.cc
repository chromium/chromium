// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/logging/proto_to_dictionary_conversion.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/base64url.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace {
std::string Encode(const std::string& str) {
  std::string encoded_string;
  base::Base64UrlEncode(str, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_string);
  return encoded_string;
}

std::string TruncateString(std::string_view str) {
  if (str.length() <= 10)
    return std::string(str);
  return base::StrCat(
      {str.substr(0, 5), "...", str.substr(str.length() - 5, str.length())});
}
}  // namespace

base::DictValue ListPublicCertificatesRequestToReadableDictionary(
    const nearby::sharing::proto::ListPublicCertificatesRequest& request) {
  base::ListValue secret_ids_list;
  for (const auto& secret_id : request.secret_ids()) {
    secret_ids_list.Append(TruncateString(Encode(secret_id)));
  }

  return base::DictValue()
      .Set("parent", request.parent())
      .Set("page_size", request.page_size())
      .Set("page_token", request.page_token())
      .Set("secret_ids", std::move(secret_ids_list));
}

base::DictValue ListPublicCertificatesResponseToReadableDictionary(
    const nearby::sharing::proto::ListPublicCertificatesResponse& response) {
  base::ListValue public_certificates_list;
  for (const auto& public_certificate : response.public_certificates()) {
    public_certificates_list.Append(
        PublicCertificateToReadableDictionary(public_certificate));
  }

  return base::DictValue()
      .Set("next_page_token", response.next_page_token())
      .Set("public_certificates", std::move(public_certificates_list));
}

base::DictValue PublicCertificateToReadableDictionary(
    const nearby::sharing::proto::PublicCertificate& certificate) {
  return base::DictValue()
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

base::DictValue TimestampToReadableDictionary(
    const nearby::sharing::proto::Timestamp& timestamp) {
  return base::DictValue()
      .Set("seconds", base::NumberToString(timestamp.seconds()))
      .Set("nanos", base::NumberToString(timestamp.nanos()));
}

base::DictValue ListContactPeopleRequestToReadableDictionary(
    const nearby::sharing::proto::ListContactPeopleRequest& request) {
  return base::DictValue()
      .Set("page_size", request.page_size())
      .Set("page_token", request.page_token());
}

base::DictValue ListContactPeopleResponseToReadableDictionary(
    const nearby::sharing::proto::ListContactPeopleResponse& response) {
  base::ListValue contact_records_list;
  for (const auto& contact_record : response.contact_records()) {
    contact_records_list.Append(
        ContactRecordToReadableDictionary(contact_record));
  }

  return base::DictValue()
      .Set("contact_records", std::move(contact_records_list))
      .Set("next_page_token", response.next_page_token());
}

base::DictValue ContactRecordToReadableDictionary(
    const nearby::sharing::proto::ContactRecord& contact_record) {
  base::ListValue identifiers_list;
  for (const auto& identifier : contact_record.identifiers()) {
    identifiers_list.Append(IdentifierToReadableDictionary(identifier));
  }

  return base::DictValue()
      .Set("id", contact_record.id())
      .Set("person_name", contact_record.person_name())
      .Set("image_url", contact_record.image_url())
      .Set("identifiers", std::move(identifiers_list));
}

base::DictValue IdentifierToReadableDictionary(
    const nearby::sharing::proto::Contact::Identifier& identifier) {
  base::DictValue dict;
  if (!identifier.obfuscated_gaia().empty()) {
    dict.Set("identifier", identifier.obfuscated_gaia());
    dict.Set("identifier", identifier.phone_number());
  } else if (!identifier.account_name().empty()) {
    dict.Set("identifier", identifier.account_name());
  }
  return dict;
}

base::DictValue UpdateDeviceRequestToReadableDictionary(
    const nearby::sharing::proto::UpdateDeviceRequest& request) {
  return base::DictValue()
      .Set("device", DeviceToReadableDictionary(request.device()))
      .Set("update_mask", FieldMaskToReadableDictionary(request.update_mask()));
}

base::DictValue DeviceToReadableDictionary(
    const nearby::sharing::proto::Device& device) {
  base::ListValue contacts_list;
  for (const auto& contact : device.contacts()) {
    contacts_list.Append(ContactToReadableDictionary(contact));
  }

  base::ListValue public_certificates_list;
  for (const auto& certificate : device.public_certificates()) {
    public_certificates_list.Append(
        PublicCertificateToReadableDictionary(certificate));
  }

  return base::DictValue()
      .Set("name", device.name())
      .Set("display_name", device.display_name())
      .Set("contacts", std::move(contacts_list))
      .Set("public_certificates", std::move(public_certificates_list));
}

base::DictValue ContactToReadableDictionary(
    const nearby::sharing::proto::Contact& contact) {
  return base::DictValue()
      .Set("identifier", IdentifierToReadableDictionary(contact.identifier()))
      .Set("is_selected", contact.is_selected());
}

base::DictValue FieldMaskToReadableDictionary(
    const nearby::sharing::proto::FieldMask& mask) {
  base::ListValue paths_list;
  for (const auto& path : mask.paths()) {
    paths_list.Append(path);
  }

  return base::DictValue().Set("paths", std::move(paths_list));
}

base::DictValue UpdateDeviceResponseToReadableDictionary(
    const nearby::sharing::proto::UpdateDeviceResponse& response) {
  return base::DictValue()
      .Set("device", DeviceToReadableDictionary(response.device()))
      .Set("person_name", response.person_name())
      .Set("image_url", response.image_url())
      .Set("image_token", response.image_token());
}

base::DictValue EncryptedMetadataToReadableDictionary(
    const nearby::sharing::proto::EncryptedMetadata& data) {
  return base::DictValue()
      .Set("device_name", data.device_name())
      .Set("full_name", data.full_name())
      .Set("icon_url", data.icon_url())
      .Set("bluetooth_mac_address",
           TruncateString(Encode(data.bluetooth_mac_address())))
      .Set("obfuscated_gaia_id", data.obfuscated_gaia_id())
      .Set("account_name", data.account_name());
}
