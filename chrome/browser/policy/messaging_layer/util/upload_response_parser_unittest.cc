// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "components/reporting/proto/synced/configuration_file.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "components/reporting/util/status.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr Priority kPriority = Priority::FAST_BATCH;
constexpr int64_t kSequencingId = 10L;
constexpr int64_t kGenerationId = 1234L;

constexpr std::string_view kEncryptionKey = "Encryption_Key";
constexpr int kPublicKeyId = 12345;
constexpr std::string_view kEncryptionSignature = "Encryption_Signature";

constexpr int kConfigVersion = 11;
constexpr std::string_view kConfigSignature = "Config_Signature";

base::Value::Dict ComposeSequencingInfo(
    std::optional<Priority> priority,
    std::optional<int64_t> sequencing_id,
    std::optional<int64_t> generation_id,
    std::optional<std::string_view> generation_guid) {
  base::Value::Dict seq_info;
  if (priority.has_value()) {
    // Alternate Priority between string and number representation at random.
    if (base::RandUint64() % 2uL == 0uL) {
      seq_info.Set(json_keys::kPriority, priority.value());
    } else {
      seq_info.Set(json_keys::kPriority, Priority_Name(priority.value()));
    }
  }
  if (sequencing_id.has_value()) {
    seq_info.Set(json_keys::kSequencingId,
                 base::NumberToString(sequencing_id.value()));
  }
  if (generation_id.has_value()) {
    seq_info.Set(json_keys::kGenerationId,
                 base::NumberToString(generation_id.value()));
  }
  if (generation_guid.has_value()) {
    seq_info.Set(json_keys::kGenerationGuid, generation_guid.value());
  }
  return seq_info;
}

class ResponseBuilder {
 public:
  ResponseBuilder() = default;

  void SetForceConfirm() { result_.Set(json_keys::kForceConfirm, true); }

  void SetEnableUploadSizeAdjustment() {
    result_.Set(json_keys::kEnableUploadSizeAdjustment, true);
  }

  void SetEncryptionSettings() {
    base::Value::Dict encryption_settings;
    encryption_settings.Set(json_keys::kPublicKey,
                            base::Base64Encode(kEncryptionKey));
    encryption_settings.Set(json_keys::kPublicKeyId, kPublicKeyId);
    encryption_settings.Set(json_keys::kPublicKeySignature,
                            base::Base64Encode(kEncryptionSignature));
    result_.Set(json_keys::kEncryptionSettings, std::move(encryption_settings));
  }

  void SetConfigFile() {
    base::Value::Dict config_file_dict;
    config_file_dict.Set(json_keys::kConfigurationFileVersionResponse,
                         kConfigVersion);
    config_file_dict.Set(json_keys::kConfigurationFileSignature,
                         base::Base64Encode(kConfigSignature));
    // Leave the list empty.
    config_file_dict.Set(json_keys::kBlockedEventConfigs, base::Value::List());
    result_.Set(json_keys::kConfigurationFile, std::move(config_file_dict));
  }

  void SetLastSuccessfulRecord(base::Value::Dict&& seq_info) {
    result_.Set(json_keys::kLastSucceedUploadedRecord, std::move(seq_info));
  }

  void SetFirstFailedRecord(base::Value::Dict&& seq_info, Status status) {
    base::Value::Dict failure_info;
    failure_info.Set(json_keys::kFailedUploadedRecord, std::move(seq_info));

    base::Value::Dict failure_status;
    failure_status.Set(json_keys::kErrorCode, status.error_code());
    failure_status.Set(json_keys::kErrorMessage, status.error_message());
    failure_info.Set(json_keys::kFailureStatus, std::move(failure_status));

    result_.Set(json_keys::kFirstFailedUploadedRecord, std::move(failure_info));
  }

  base::Value::Dict Build() { return std::move(result_); }

 private:
  base::Value::Dict result_;
};

class UploadResponseParserTest
    : public ::testing::TestWithParam<
          ::testing::tuple</*need_encryption_key*/ bool,
                           /*force_confirm*/ bool,
                           /*enable_upload_size_adjustment*/ bool,
                           /*config_file*/ bool>> {
 protected:
  const std::string kGenerationGuid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  void SetUp() override {
    if (config_file()) {
      scoped_feature_list_.InitAndEnableFeature(
          kShouldRequestConfigurationFile);
      builder_.SetConfigFile();
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kShouldRequestConfigurationFile);
    }
    if (force_confirm()) {
      builder_.SetForceConfirm();
    }
    if (enable_upload_size_adjustment()) {
      builder_.SetEnableUploadSizeAdjustment();
    }
    if (need_encryption_key()) {
      builder_.SetEncryptionSettings();
    }
  }

  void CommonExpectation(const UploadResponseParser& response) {
    EXPECT_THAT(response.force_confirm_flag(), Eq(force_confirm()));
    EXPECT_THAT(response.enable_upload_size_adjustment(),
                Eq(enable_upload_size_adjustment()));
    if (need_encryption_key()) {
      EXPECT_TRUE(response.encryption_settings().has_value());
      EXPECT_THAT(response.encryption_settings().value(),
                  AllOf(Property(&SignedEncryptionInfo::public_asymmetric_key,
                                 StrEq(kEncryptionKey)),
                        Property(&SignedEncryptionInfo::public_key_id,
                                 Eq(kPublicKeyId)),
                        Property(&SignedEncryptionInfo::signature,
                                 StrEq(kEncryptionSignature))));
    } else {
      EXPECT_FALSE(response.encryption_settings().has_value());
      EXPECT_THAT(response.encryption_settings().error(),
                  Property(&Status::error_code, Eq(error::NOT_FOUND)));
    }
    if (config_file()) {
      EXPECT_TRUE(response.config_file().has_value());
      EXPECT_THAT(response.config_file().value(),
                  AllOf(Property(&ConfigFile::version, Eq(kConfigVersion)),
                        Property(&ConfigFile::config_file_signature,
                                 StrEq(kConfigSignature))));
    } else {
      EXPECT_FALSE(response.config_file().has_value());
      EXPECT_THAT(
          response.config_file().error(),
          Property(&Status::error_code, Eq(error::FAILED_PRECONDITION)));
    }
    EXPECT_FALSE(response.gap_record_for_permanent_failure().has_value());
    EXPECT_THAT(response.gap_record_for_permanent_failure().error(),
                Property(&Status::error_code, Eq(error::NOT_FOUND)));
  }

  bool need_encryption_key() const { return std::get<0>(GetParam()); }
  bool force_confirm() const { return std::get<1>(GetParam()); }
  bool enable_upload_size_adjustment() const { return std::get<2>(GetParam()); }
  bool config_file() const { return std::get<3>(GetParam()); }

  base::test::ScopedFeatureList scoped_feature_list_;
  ResponseBuilder builder_;
};

TEST_P(UploadResponseParserTest, SuccessfulUpload) {
  auto seq_info = ComposeSequencingInfo(kPriority, kSequencingId, kGenerationId,
                                        kGenerationGuid);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                builder_.Build());
  CommonExpectation(response);
  EXPECT_TRUE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().value(),
      AllOf(Property(&SequenceInformation::priority, Eq(kPriority)),
            Property(&SequenceInformation::sequencing_id, Eq(kSequencingId)),
#if BUILDFLAG(IS_CHROMEOS)
            Property(&SequenceInformation::generation_guid,
                     StrEq(kGenerationGuid)),
#endif  // BUILDFLAG(IS_CHROMEOS)
            Property(&SequenceInformation::generation_id, Eq(kGenerationId))));
}

TEST_P(UploadResponseParserTest, MissingPriorityField) {
  auto seq_info = ComposeSequencingInfo(std::nullopt, kSequencingId,
                                        kGenerationId, kGenerationGuid);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                builder_.Build());
  CommonExpectation(response);
  EXPECT_FALSE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().error(),
      Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
}

TEST_P(UploadResponseParserTest, InvalidPriorityField) {
  auto seq_info = ComposeSequencingInfo(kPriority, kSequencingId, kGenerationId,
                                        kGenerationGuid);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  auto response_dict = builder_.Build();
  response_dict.SetByDottedPath("lastSucceedUploadedRecord.priority", "abc");

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                std::move(response_dict));
  CommonExpectation(response);
  EXPECT_FALSE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().error(),
      Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
}

TEST_P(UploadResponseParserTest, MissingSequenceInformation) {
  auto seq_info = ComposeSequencingInfo(kPriority, std::nullopt, kGenerationId,
                                        kGenerationGuid);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  auto response_dict = builder_.Build();
  response_dict.SetByDottedPath("lastSucceedUploadedRecord.priority", "abc");

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                std::move(response_dict));
  CommonExpectation(response);
  EXPECT_FALSE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().error(),
      Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
}

TEST_P(UploadResponseParserTest, ContainsGenerationGuid) {
  auto seq_info = ComposeSequencingInfo(kPriority, kSequencingId, kGenerationId,
                                        kGenerationGuid);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  auto response_dict = builder_.Build();
  // Verify generation guid exists and equals kGenerationGuid.
  ASSERT_THAT(response_dict.FindStringByDottedPath(
                  "lastSucceedUploadedRecord.generationGuid"),
              NotNull());
  EXPECT_THAT(*(response_dict.FindStringByDottedPath(
                  "lastSucceedUploadedRecord.generationGuid")),
              StrEq(kGenerationGuid));

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                std::move(response_dict));
  CommonExpectation(response);
  EXPECT_TRUE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().value(),
      AllOf(Property(&SequenceInformation::priority, Eq(kPriority)),
            Property(&SequenceInformation::sequencing_id, Eq(kSequencingId)),
#if BUILDFLAG(IS_CHROMEOS)
            Property(&SequenceInformation::generation_guid,
                     StrEq(kGenerationGuid)),
#endif  // BUILDFLAG(IS_CHROMEOS)
            Property(&SequenceInformation::generation_id, Eq(kGenerationId))));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(UploadResponseParserTest, InvalidGenerationGuid) {
  auto seq_info = ComposeSequencingInfo(kPriority, kSequencingId, kGenerationId,
                                        kGenerationGuid);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  auto response_dict = builder_.Build();
  // Generation guids must be parsable into `base::Uuid`.
  response_dict.SetByDottedPath("lastSucceedUploadedRecord.generationGuid",
                                "invalid-generation-guid");

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                std::move(response_dict));
  CommonExpectation(response);
  EXPECT_FALSE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().error(),
      Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
}

TEST_P(UploadResponseParserTest, MissingGenerationGuidFailsWhenRequired) {
  auto seq_info = ComposeSequencingInfo(kPriority, kSequencingId, kGenerationId,
                                        std::nullopt);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  auto response_dict = builder_.Build();
  // Remove the generation guid.
  response_dict.RemoveByDottedPath("lastSucceedUploadedRecord.generationGuid");

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                std::move(response_dict));
  CommonExpectation(response);
  EXPECT_FALSE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().error(),
      Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
}

TEST_P(UploadResponseParserTest, MissingGenerationGuidOkWhenNotRequired) {
  auto seq_info = ComposeSequencingInfo(kPriority, kSequencingId, kGenerationId,
                                        std::nullopt);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  UploadResponseParser response(/*is_generation_guid_required=*/false,
                                builder_.Build());
  CommonExpectation(response);
  EXPECT_TRUE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().value(),
      AllOf(Property(&SequenceInformation::priority, Eq(kPriority)),
            Property(&SequenceInformation::sequencing_id, Eq(kSequencingId)),
            Property(&SequenceInformation::generation_id, Eq(kGenerationId)),
            Property(&SequenceInformation::generation_guid, IsEmpty())));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_P(UploadResponseParserTest, GapUponPermanentFailure) {
  auto seq_info = ComposeSequencingInfo(kPriority, kSequencingId, kGenerationId,
                                        kGenerationGuid);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  // Matching failure.
  auto failure_seq_info = ComposeSequencingInfo(kPriority, kSequencingId + 1L,
                                                kGenerationId, kGenerationGuid);
  builder_.SetFirstFailedRecord(
      std::move(failure_seq_info),
      Status(error::UNAUTHENTICATED, "Authentication failure"));

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                builder_.Build());
  EXPECT_TRUE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().value(),
      AllOf(Property(&SequenceInformation::priority, Eq(kPriority)),
            Property(&SequenceInformation::sequencing_id, Eq(kSequencingId)),
#if BUILDFLAG(IS_CHROMEOS)
            Property(&SequenceInformation::generation_guid,
                     StrEq(kGenerationGuid)),
#endif  // BUILDFLAG(IS_CHROMEOS)
            Property(&SequenceInformation::generation_id, Eq(kGenerationId))));

  EXPECT_TRUE(response.gap_record_for_permanent_failure().has_value());
  EXPECT_THAT(
      response.gap_record_for_permanent_failure().value(),
      Property(&EncryptedRecord::sequence_information,
               AllOf(Property(&SequenceInformation::priority, Eq(kPriority)),
                     Property(&SequenceInformation::sequencing_id,
                              Eq(kSequencingId + 1)),
#if BUILDFLAG(IS_CHROMEOS)
                     Property(&SequenceInformation::generation_guid,
                              StrEq(kGenerationGuid)),
#endif  // BUILDFLAG(IS_CHROMEOS)
                     Property(&SequenceInformation::generation_id,
                              Eq(kGenerationId)))));
}

TEST_P(UploadResponseParserTest, GapUponPermanentFailureLoss) {
  auto seq_info = ComposeSequencingInfo(kPriority, kSequencingId, kGenerationId,
                                        kGenerationGuid);
  builder_.SetLastSuccessfulRecord(std::move(seq_info));

  // Mismatching failure.
  auto failure_seq_info = ComposeSequencingInfo(kPriority, kSequencingId,
                                                kGenerationId, kGenerationGuid);
  builder_.SetFirstFailedRecord(
      std::move(failure_seq_info),
      Status(error::UNAUTHENTICATED, "Authentication failure"));

  UploadResponseParser response(/*is_generation_guid_required=*/true,
                                builder_.Build());
  EXPECT_TRUE(
      response.last_successfully_uploaded_record_sequence_info().has_value());
  EXPECT_THAT(
      response.last_successfully_uploaded_record_sequence_info().value(),
      AllOf(Property(&SequenceInformation::priority, Eq(kPriority)),
            Property(&SequenceInformation::sequencing_id, Eq(kSequencingId)),
#if BUILDFLAG(IS_CHROMEOS)
            Property(&SequenceInformation::generation_guid,
                     StrEq(kGenerationGuid)),
#endif  // BUILDFLAG(IS_CHROMEOS)
            Property(&SequenceInformation::generation_id, Eq(kGenerationId))));

  EXPECT_FALSE(response.gap_record_for_permanent_failure().has_value());
  EXPECT_THAT(response.gap_record_for_permanent_failure().error(),
              Property(&Status::error_code, Eq(error::DATA_LOSS)));
}

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    UploadResponseParserTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool(),
                       /*enable_upload_size_adjustment*/ ::testing::Bool(),
                       /*config_file*/ ::testing::Bool()));
}  // namespace
}  // namespace reporting
