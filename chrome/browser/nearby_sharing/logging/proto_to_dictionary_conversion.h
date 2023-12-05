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
base::Value::Dict ListPublicCertificatesRequestToReadableDictionary(
    const nearby::sharing::proto::ListPublicCertificatesRequest& request);
base::Value::Dict ListPublicCertificatesResponseToReadableDictionary(
    const nearby::sharing::proto::ListPublicCertificatesResponse& response);
base::Value::Dict PublicCertificateToReadableDictionary(
    const nearby::sharing::proto::PublicCertificate& certificate);
base::Value::Dict TimestampToReadableDictionary(
    const nearby::sharing::proto::Timestamp& timestamp);
base::Value::Dict ListContactPeopleRequestToReadableDictionary(
    const nearby::sharing::proto::ListContactPeopleRequest& request);
base::Value::Dict ListContactPeopleResponseToReadableDictionary(
    const nearby::sharing::proto::ListContactPeopleResponse& response);
base::Value::Dict ContactRecordToReadableDictionary(
    const nearby::sharing::proto::ContactRecord& contact_record);
base::Value::Dict IdentifierToReadableDictionary(
    const nearby::sharing::proto::Contact::Identifier& identifier);
base::Value::Dict UpdateDeviceRequestToReadableDictionary(
    const nearby::sharing::proto::UpdateDeviceRequest& request);
base::Value::Dict DeviceToReadableDictionary(
    const nearby::sharing::proto::Device& device);
base::Value::Dict ContactToReadableDictionary(
    const nearby::sharing::proto::Contact& contact);
base::Value::Dict FieldMaskToReadableDictionary(
    const nearby::sharing::proto::FieldMask& mask);
base::Value::Dict UpdateDeviceResponseToReadableDictionary(
    const nearby::sharing::proto::UpdateDeviceResponse& response);
base::Value::Dict EncryptedMetadataToReadableDictionary(
    const nearby::sharing::proto::EncryptedMetadata& data);

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOGGING_PROTO_TO_DICTIONARY_CONVERSION_H_
