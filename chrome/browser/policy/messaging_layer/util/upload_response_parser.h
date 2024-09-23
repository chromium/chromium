// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_UPLOAD_RESPONSE_PARSER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_UPLOAD_RESPONSE_PARSER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/reporting/proto/synced/configuration_file.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// {{{Note}}} ERP Response Payload Overview
//
//  {
//    "lastSucceedUploadedRecord": ... // SequenceInformation proto
//    "firstFailedUploadedRecord": {
//      "failedUploadedRecord": ... // SequenceInformation proto
//      "failureStatus": ... // Status proto
//    },
//    "encryptionSettings": ... // EncryptionSettings proto
//    "forceConfirm": true, // if present, flag that lastSucceedUploadedRecord
//                          // is to be accepted unconditionally by client
//    "configurationFile": ... // ConfigurationFile proto
//    // Internal control
//    "enableUploadSizeAdjustment": true,  // If present, upload size
//                                         // adjustment is enabled.
//  }
class UploadResponseParser {
 public:
  UploadResponseParser(bool is_generation_guid_required,
                       base::Value::Dict response);
  UploadResponseParser(UploadResponseParser&& other);
  UploadResponseParser& operator=(UploadResponseParser&& other);
  ~UploadResponseParser();

  // Accessors.
  StatusOr<SequenceInformation>
  last_successfully_uploaded_record_sequence_info() const;
  StatusOr<SignedEncryptionInfo> encryption_settings() const;
  StatusOr<ConfigFile> config_file() const;
  StatusOr<EncryptedRecord> gap_record_for_permanent_failure() const;
  // The accessors below return `true` if the `response` is good and the
  // respective flag is on. In all other cases they return `false`.
  bool force_confirm_flag() const;
  bool enable_upload_size_adjustment() const;

 private:
  // Helper function for converting a base::Value representation of
  // SequenceInformation into a proto. Will return an INVALID_ARGUMENT error
  // if the base::Value is not convertible.
  static StatusOr<SequenceInformation> SequenceInformationValueToProto(
      bool is_generation_guid_required,
      const base::Value::Dict& sequence_information_dict);

  // Helper function for converting a base::Value representation of
  // SequenceInformation into a gap record proto.
  static StatusOr<EncryptedRecord> HandleFailedUploadedSequenceInformation(
      bool is_generation_guid_required,
      const SequenceInformation& highest_sequence_information,
      const base::Value::Dict& sequence_information_dict);

  bool is_generation_guid_required_;
  base::Value::Dict response_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_UPLOAD_RESPONSE_PARSER_H_
