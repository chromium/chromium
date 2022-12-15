// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOGGING_PROTO_TO_DICTIONARY_CONVERSION_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOGGING_PROTO_TO_DICTIONARY_CONVERSION_H_

#include "base/values.h"
#include "chrome/browser/nearby_sharing/proto/certificate_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/contact_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/device_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/encrypted_metadata.pb.h"
#include "chrome/browser/nearby_sharing/proto/field_mask.pb.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chrome/browser/nearby_sharing/proto/timestamp.pb.h"

// Converts Nearby Share protos to readable, JSON-style dictionaries.
base::Value::Dict ListPublicCertificatesRequestToReadableDictionary(
    const nearbyshare::proto::ListPublicCertificatesRequest& request);
base::Value::Dict ListPublicCertificatesResponseToReadableDictionary(
    const nearbyshare::proto::ListPublicCertificatesResponse& response);
base::Value::Dict PublicCertificateToReadableDictionary(
    const nearbyshare::proto::PublicCertificate& certificate);
base::Value::Dict TimestampToReadableDictionary(
    const nearbyshare::proto::Timestamp& timestamp);
base::Value::Dict ListContactPeopleRequestToReadableDictionary(
    const nearbyshare::proto::ListContactPeopleRequest& request);
base::Value::Dict ListContactPeopleResponseToReadableDictionary(
    const nearbyshare::proto::ListContactPeopleResponse& response);
base::Value::Dict ContactRecordToReadableDictionary(
    const nearbyshare::proto::ContactRecord& contact_record);
base::Value::Dict IdentifierToReadableDictionary(
    const nearbyshare::proto::Contact::Identifier& identifier);
base::Value::Dict UpdateDeviceRequestToReadableDictionary(
    const nearbyshare::proto::UpdateDeviceRequest& request);
base::Value::Dict DeviceToReadableDictionary(
    const nearbyshare::proto::Device& device);
base::Value::Dict ContactToReadableDictionary(
    const nearbyshare::proto::Contact& contact);
base::Value::Dict FieldMaskToReadableDictionary(
    const nearbyshare::proto::FieldMask& mask);
base::Value::Dict UpdateDeviceResponseToReadableDictionary(
    const nearbyshare::proto::UpdateDeviceResponse& response);
base::Value::Dict EncryptedMetadataToReadableDictionary(
    const nearbyshare::proto::EncryptedMetadata& data);

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOGGING_PROTO_TO_DICTIONARY_CONVERSION_H_
