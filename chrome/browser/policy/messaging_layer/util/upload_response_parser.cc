// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/token.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "components/reporting/proto/synced/configuration_file.pb.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {
namespace {

// Priority could come back as an int or as a std::string, this function handles
// both situations.
std::optional<Priority> GetPriorityProtoFromSequenceInformationValue(
    const base::Value::Dict& sequence_information) {
  const std::optional<int> int_priority_result =
      sequence_information.FindInt(json_keys::kPriority);
  if (int_priority_result.has_value()) {
    return Priority(int_priority_result.value());
  }

  const std::string* str_priority_result =
      sequence_information.FindString(json_keys::kPriority);
  if (!str_priority_result) {
    LOG(ERROR) << "Field priority is missing from SequenceInformation: "
               << sequence_information;
    return std::nullopt;
  }

  Priority priority;
  if (!Priority_Parse(*str_priority_result, &priority)) {
    LOG(ERROR) << "Unable to parse field priority in SequenceInformation: "
               << sequence_information;
    return std::nullopt;
  }
  return priority;
}

// Returns a tuple of <SequencingId, GenerationId> if `sequencing_id` and
// `generation_id` can be parsed as numbers. Returns error status otherwise.
StatusOr<std::tuple<int64_t, int64_t>> ParseSequencingIdAndGenerationId(
    const std::string* sequencing_id,
    const std::string* generation_id) {
  int64_t seq_id;
  int64_t gen_id;

  if (!base::StringToInt64(*sequencing_id, &seq_id)) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT, "Could not parse sequencing id."));
  }
  if (!base::StringToInt64(*generation_id, &gen_id) || gen_id == 0) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT, "Could not parse generation id."));
  }
  return std::make_tuple(seq_id, gen_id);
}

// Destination comes back as a string from the server, transform it into a
// proto.
StatusOr<Destination> GetDestinationProto(
    const std::string& destination_string) {
  if (destination_string == "") {
    return base::unexpected(Status(
        error::NOT_FOUND, "Field destination is missing from ConfigFile"));
  }

  Destination destination;
  if (!Destination_Parse(destination_string, &destination)) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT,
               "Unable to parse destination from ConfigFile"));
  }

  // Reject undefined destination.
  if (destination == UNDEFINED_DESTINATION) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT, "Received UNDEFINED_DESTINATION"));
  }

  return destination;
}

StatusOr<ConfigFile> GetConfigurationProtoFromDict(
    const base::Value::Dict& file) {
  ConfigFile config_file;

  // Handle the version.
  const auto config_file_version =
      file.FindInt(json_keys::kConfigurationFileVersionResponse);
  if (!config_file_version.has_value()) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT,
               "Field version is missing from configurationFile"));
  }
  config_file.set_version(config_file_version.value());

  // Handle the signature.
  const std::string* config_file_signature_str =
      file.FindString(json_keys::kConfigurationFileSignature);
  if (!config_file_signature_str || config_file_signature_str->empty()) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT,
               "Field configFileSignature is missing from configurationFile"));
  }
  std::string config_file_signature;
  if (!base::Base64Decode(*config_file_signature_str, &config_file_signature)) {
    return base::unexpected(Status(error::INVALID_ARGUMENT,
                                   "Unable to decode configFileSignature"));
  }
  config_file.set_config_file_signature(config_file_signature);

  auto* const event_config_result =
      file.FindList(json_keys::kBlockedEventConfigs);
  if (!event_config_result) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT,
               "Field blockedEventConfigs is missing from configurationFile"));
  }

  // Parse the list of event configs.
  for (auto& entry : *event_config_result) {
    auto* const current_config = config_file.add_blocked_event_configs();
    auto* const dict = entry.GetIfDict();
    if (dict->empty()) {
      return base::unexpected(Status(
          error::INVALID_ARGUMENT, "Empty event config in configurationFile"));
    }

    // Find destination and turn it into a proto.
    auto* const destination =
        dict->FindString(json_keys::kConfigurationFileDestination);
    ASSIGN_OR_RETURN(const auto proto_destination,
                     GetDestinationProto(*destination));
    current_config->set_destination(proto_destination);

    // Check if there are minimum and/or maximum release versions
    // specified, if there are then we parse them and add them to the proto.
    // This fields are optional so if they are not present it is okay.
    const auto min_version =
        dict->FindInt(json_keys::kConfigurationFileMinimumReleaseVersion);
    if (min_version.has_value()) {
      current_config->set_minimum_release_version(min_version.value());
    }
    const auto max_version =
        dict->FindInt(json_keys::kConfigurationFileMaximumReleaseVersion);
    if (max_version.has_value()) {
      current_config->set_maximum_release_version(max_version.value());
    }
  }

  return config_file;
}
}  // namespace

UploadResponseParser::UploadResponseParser(bool is_generation_guid_required,
                                           base::Value::Dict response)
    : is_generation_guid_required_(is_generation_guid_required),
      response_(std::move(response)) {}

UploadResponseParser::UploadResponseParser(UploadResponseParser&& other) =
    default;
UploadResponseParser& UploadResponseParser::operator=(
    UploadResponseParser&& other) = default;

UploadResponseParser::~UploadResponseParser() = default;

StatusOr<SequenceInformation>
UploadResponseParser::last_successfully_uploaded_record_sequence_info() const {
  const base::Value::Dict* const
      last_successfully_uploaded_record_sequence_info =
          response_.FindDict(json_keys::kLastSucceedUploadedRecord);
  if (last_successfully_uploaded_record_sequence_info == nullptr) {
    return base::unexpected(Status(
        error::NOT_FOUND,
        base::StrCat({"Server responded with no lastSucceedUploadedRecord",
                      response_.DebugString()})));
  }
  ASSIGN_OR_RETURN(auto seq_info,
                   SequenceInformationValueToProto(
                       is_generation_guid_required_,
                       *last_successfully_uploaded_record_sequence_info));
  return std::move(seq_info);
}

StatusOr<SignedEncryptionInfo> UploadResponseParser::encryption_settings()
    const {
  // Handle the encryption settings.
  // Note: server can attach it to response regardless of whether
  // the response indicates success or failure, and whether the client
  // set attach_encryption_settings to true in request.
  const auto* const signed_encryption_key_record =
      response_.FindDict(json_keys::kEncryptionSettings);
  if (signed_encryption_key_record == nullptr) {
    return base::unexpected(Status(error::NOT_FOUND, "No encryption settings"));
  }

  std::string public_key;
  {
    const auto* const public_key_str =
        signed_encryption_key_record->FindString(json_keys::kPublicKey);
    if (public_key_str == nullptr ||
        !base::Base64Decode(*public_key_str, &public_key)) {
      return base::unexpected(
          Status(error::FAILED_PRECONDITION,
                 "Public encryption key is malformed or missing"));
    }
  }

  const auto public_key_id_result =
      signed_encryption_key_record->FindInt(json_keys::kPublicKeyId);
  if (!public_key_id_result.has_value()) {
    return base::unexpected(Status(error::FAILED_PRECONDITION,
                                   "Public encryption key ID is missing"));
  }

  std::string public_key_signature;
  {
    const auto* const public_key_signature_str =
        signed_encryption_key_record->FindString(
            json_keys::kPublicKeySignature);
    if (public_key_signature_str == nullptr ||
        !base::Base64Decode(*public_key_signature_str, &public_key_signature)) {
      return base::unexpected(
          Status(error::FAILED_PRECONDITION,
                 "Encryption settings signature missing or malformed"));
    }
  }

  SignedEncryptionInfo signed_encryption_key;
  signed_encryption_key.set_public_asymmetric_key(public_key);
  signed_encryption_key.set_public_key_id(public_key_id_result.value());
  signed_encryption_key.set_signature(public_key_signature);
  return signed_encryption_key;
}

StatusOr<ConfigFile> UploadResponseParser::config_file() const {
  if (!base::FeatureList::IsEnabled(kShouldRequestConfigurationFile)) {
    return base::unexpected(Status(error::FAILED_PRECONDITION,
                                   "Config file attachment not enabled"));
  }
  // Handle the configuration file.
  // The server attaches the configuration file if it was requested
  // by the client. Adding a check to make sure to only process it if the
  // feature is enabled on the client side.
  const base::Value::Dict* signed_configuration_file_record =
      response_.FindDict(json_keys::kConfigurationFile);
  if (signed_configuration_file_record == nullptr) {
    return base::unexpected(
        Status(error::NOT_FOUND, "Config file not attached"));
  }
  ASSIGN_OR_RETURN(
      auto signed_configuration_file,
      GetConfigurationProtoFromDict(*signed_configuration_file_record));
  return std::move(signed_configuration_file);
}

StatusOr<EncryptedRecord>
UploadResponseParser::gap_record_for_permanent_failure() const {
  const auto* const failed_uploaded_record = response_.FindDictByDottedPath(
      base::StrCat({json_keys::kFirstFailedUploadedRecord, ".",
                    json_keys::kFailedUploadedRecord}));
  if (failed_uploaded_record == nullptr) {
    return base::unexpected(
        Status(error::NOT_FOUND, "No permanent failures reporting"));
  }
  // if the record was after the current |highest_sequence_information_|
  // we should return a gap record. A gap record consists of an
  // EncryptedRecord with just SequenceInformation. The server will
  // report success for the gap record and
  // |highest_sequence_information_| will be updated in the next
  // response. In the future there may be recoverable |failureStatus|,
  // but for now all the device can do is delete the record.
  ASSIGN_OR_RETURN(const auto last_succeed_uploaded,
                   last_successfully_uploaded_record_sequence_info());
  ASSIGN_OR_RETURN(auto gap_record,
                   HandleFailedUploadedSequenceInformation(
                       is_generation_guid_required_, last_succeed_uploaded,
                       *failed_uploaded_record));
  return std::move(gap_record);
}

bool UploadResponseParser::force_confirm_flag() const {
  const auto force_confirm_flag = response_.FindBool(json_keys::kForceConfirm);
  return force_confirm_flag.has_value() && force_confirm_flag.value();
}

bool UploadResponseParser::enable_upload_size_adjustment() const {
  const auto enable_upload_size_adjustment =
      response_.FindBool(json_keys::kEnableUploadSizeAdjustment);
  return enable_upload_size_adjustment.has_value() &&
         enable_upload_size_adjustment.value();
}

#if BUILDFLAG(IS_CHROMEOS)
// Returns true if `generation_guid` can be parsed as a GUID or if
// `generation_guid` does not need to be parsed based on the type of device.
// Returns false otherwise.
bool GenerationGuidIsValid(bool is_generation_guid_required,
                           const std::string& generation_guid) {
  if (generation_guid.empty() && !is_generation_guid_required) {
    // This is a legacy ChromeOS managed device and is not required to have
    // a `generation_guid`.
    return true;
  }
  // If the generation guid has some value, try to parse it.
  return base::Uuid::ParseCaseInsensitive(generation_guid).is_valid();
}

// Returns true if `generation_guid` is required and missing.
// Returns false otherwise.
bool IsMissingGenerationGuid(bool is_generation_guid_required,
                             const std::string* generation_guid) {
  if (!is_generation_guid_required) {
    return false;
  }
  return !generation_guid || generation_guid->empty();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Returns true if any required sequence info is missing. Returns
// false otherwise.
bool IsMissingSequenceInformation(bool is_generation_guid_required,
                                  const std::string* sequencing_id,
                                  const std::string* generation_id,
                                  const std::optional<Priority> priority_result,
                                  const std::string* generation_guid) {
  return !sequencing_id || !generation_id || generation_id->empty() ||
#if BUILDFLAG(IS_CHROMEOS)
         IsMissingGenerationGuid(is_generation_guid_required,
                                 generation_guid) ||
#endif  // BUILDFLAG(IS_CHROMEOS)
         !priority_result.has_value() ||
         !Priority_IsValid(priority_result.value());
}

// static
StatusOr<SequenceInformation>
UploadResponseParser::SequenceInformationValueToProto(
    bool is_generation_guid_required,
    const base::Value::Dict& sequence_information_dict) {
  const std::string* sequencing_id =
      sequence_information_dict.FindString(json_keys::kSequencingId);
  const std::string* generation_id =
      sequence_information_dict.FindString(json_keys::kGenerationId);
  const std::string* generation_guid =
      sequence_information_dict.FindString(json_keys::kGenerationGuid);
  const auto priority_result =
      GetPriorityProtoFromSequenceInformationValue(sequence_information_dict);
  // If required sequence info fields don't exist, or are malformed,
  // return error.
  // Note: `generation_guid` is allowed to be empty - managed devices
  // may not have it.
  if (IsMissingSequenceInformation(is_generation_guid_required, sequencing_id,
                                   generation_id, priority_result,
                                   generation_guid)) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT,
               base::StrCat({"Provided value lacks some fields required by "
                             "SequenceInformation proto: ",
                             sequence_information_dict.DebugString()})));
  }

  ASSIGN_OR_RETURN(
      (const auto [seq_id, gen_id]),
      ParseSequencingIdAndGenerationId(sequencing_id, generation_id));

  SequenceInformation proto;
  proto.set_sequencing_id(seq_id);
  proto.set_generation_id(gen_id);
  proto.set_priority(Priority(priority_result.value()));

#if BUILDFLAG(IS_CHROMEOS)
  // If `generation_guid` does not exist, set it to be an empty string.
  const std::string gen_guid = generation_guid ? *generation_guid : "";
  if (!GenerationGuidIsValid(is_generation_guid_required, gen_guid)) {
    return base::unexpected(Status(
        error::INVALID_ARGUMENT,
        base::StrCat({"Provided value did not conform to a valid "
                      "SequenceInformation proto. Invalid generation guid : ",
                      sequence_information_dict.DebugString()})));
  }
  proto.set_generation_guid(gen_guid);
#endif  // BUILDFLAG(IS_CHROMEOS)
  return proto;
}

// static
StatusOr<EncryptedRecord>
UploadResponseParser::HandleFailedUploadedSequenceInformation(
    bool is_generation_guid_required,
    const SequenceInformation& highest_sequence_information,
    const base::Value::Dict& sequence_information_dict) {
  ASSIGN_OR_RETURN(SequenceInformation sequence_information,
                   SequenceInformationValueToProto(is_generation_guid_required,
                                                   sequence_information_dict));

  // |seq_info| should be of the same generation, generation guid, and
  // priority as highest_sequence_information, and have the next
  // sequencing_id.
  if (sequence_information.generation_id() !=
          highest_sequence_information.generation_id() ||
      sequence_information.generation_guid() !=
          highest_sequence_information.generation_guid() ||
      sequence_information.priority() !=
          highest_sequence_information.priority() ||
      sequence_information.sequencing_id() !=
          highest_sequence_information.sequencing_id() + 1) {
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::
            FAILED_UPLOAD_CONTAINS_INVALID_SEQUENCE_INFORMATION,
        DataLossErrorReason::MAX_VALUE);
    return base::unexpected(
        Status(error::DATA_LOSS,
               base::StrCat({"Record was unprocessable by the server: ",
                             sequence_information_dict.DebugString()})));
  }

  // Build a gap record and return it.
  EncryptedRecord encrypted_record;
  *encrypted_record.mutable_sequence_information() =
      std::move(sequence_information);
  return encrypted_record;
}
}  // namespace reporting
