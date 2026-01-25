// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOGGING_PROTO_TO_DICTIONARY_CONVERSION_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOGGING_PROTO_TO_DICTIONARY_CONVERSION_H_

#include "base/values.h"
#include "third_party/nearby/sharing/proto/certificate_rpc.pb.h"
#include "third_party/nearby/sharing/proto/contact_rpc.pb.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"
#include "third_party/nearby/sharing/proto/encrypted_metadata.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

// Converts Nearby Share protos to readable, JSON-style dictionaries.
base::DictValue ListPublicCertificatesRequestToReadableDictionary(
    const nearby::sharing::proto::ListPublicCertificatesRequest& request);
base::DictValue ListPublicCertificatesResponseToReadableDictionary(
    const nearby::sharing::proto::ListPublicCertificatesResponse& response);
base::DictValue PublicCertificateToReadableDictionary(
    const nearby::sharing::proto::PublicCertificate& certificate);
base::DictValue TimestampToReadableDictionary(
    const nearby::sharing::proto::Timestamp& timestamp);
base::DictValue ListContactPeopleRequestToReadableDictionary(
    const nearby::sharing::proto::ListContactPeopleRequest& request);
base::DictValue ListContactPeopleResponseToReadableDictionary(
    const nearby::sharing::proto::ListContactPeopleResponse& response);
base::DictValue ContactRecordToReadableDictionary(
    const nearby::sharing::proto::ContactRecord& contact_record);
base::DictValue IdentifierToReadableDictionary(
    const nearby::sharing::proto::Contact::Identifier& identifier);
base::DictValue UpdateDeviceRequestToReadableDictionary(
    const nearby::sharing::proto::UpdateDeviceRequest& request);
base::DictValue DeviceToReadableDictionary(
    const nearby::sharing::proto::Device& device);
base::DictValue ContactToReadableDictionary(
    const nearby::sharing::proto::Contact& contact);
base::DictValue FieldMaskToReadableDictionary(
    const nearby::sharing::proto::FieldMask& mask);
base::DictValue UpdateDeviceResponseToReadableDictionary(
    const nearby::sharing::proto::UpdateDeviceResponse& response);
base::DictValue EncryptedMetadataToReadableDictionary(
    const nearby::sharing::proto::EncryptedMetadata& data);

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOGGING_PROTO_TO_DICTIONARY_CONVERSION_H_
