// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_packet_metadata.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_ui_utils.h"
#include "chrome/browser/support_tool/data_collector.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/statistics_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/core/browser/webui/json_generation.h"

using redaction::PIIType;

namespace {

const char kTimestampKey[] = "Timestamp";
const char kIssueDescriptionKey[] = "Issue Description";
const char kSupportCaseIdKey[] = "Support Case ID";
const char kEmailAddressKey[] = "Email Address";
const char kSupportPacketGUIDKey[] = "Support Packet ID";
const char kDataCollectorListKey[] = "Data Collectors Included";
const char kPlatformKey[] = "Platform";
const char kOSKey[] = "OS";
const char kChromeVersionKey[] = "Chrome Version";
const char kChromeRevisionKey[] = "Chrome Revision";
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kSerialNumberKey[] = "Serial Number";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
const char kErrorMessagesKey[] = "Support Tool Errors";

const std::pair<const char* const, redaction::PIIType> kMetadataKeys[] = {
    {kTimestampKey, PIIType::kNone},
    {kIssueDescriptionKey, PIIType::kNone},
    {kSupportCaseIdKey, PIIType::kNone},
    {kEmailAddressKey, PIIType::kEmail},
    {kSupportPacketGUIDKey, PIIType::kNone},
    {kDataCollectorListKey, PIIType::kNone},
    {kPlatformKey, PIIType::kNone},
    {kOSKey, PIIType::kNone},
    {kChromeVersionKey, PIIType::kNone},
    {kChromeRevisionKey, PIIType::kNone},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kSerialNumberKey, PIIType::kSerial},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    {kErrorMessagesKey, PIIType::kNone}};

void WriteContentsOnFile(base::FilePath metadata_file,
                         std::set<redaction::PIIType> pii_to_keep,
                         std::map<std::string, std::string> metadata_contents) {
  // Create the file with write permissions.
  base::WriteFile(metadata_file, "");
  for (const auto& metadata_key : kMetadataKeys) {
    // If the metadata key is considered a PII-sensitive data and the it's not
    // listed in `pii_to_keep`, we redact it.
    bool redact = (metadata_key.second != PIIType::kNone &&
                   pii_to_keep.find(metadata_key.second) == pii_to_keep.end());
    base::AppendToFile(
        metadata_file,
        base::StringPrintf(
            "%s:\n%s\n", metadata_key.first,
            redact ? "<REDACTED>"
                   : metadata_contents[metadata_key.first].c_str()));
  }
}
}  // namespace

SupportPacketMetadata::SupportPacketMetadata(
    std::string case_id,
    std::string email_address,
    std::string issue_description,
    std::optional<std::string> upload_id) {
  metadata_[kSupportCaseIdKey] = case_id;
  metadata_[kIssueDescriptionKey] = issue_description;
  if (!email_address.empty()) {
    metadata_[kEmailAddressKey] = email_address;
    pii_[PIIType::kEmail].insert(email_address);
  }
  metadata_[kSupportPacketGUIDKey] =
      upload_id.has_value() ? upload_id.value() : GetGUIDForSupportPacket();
  SetChromeMetadataFields();
}

SupportPacketMetadata::~SupportPacketMetadata() = default;

void SupportPacketMetadata::SetChromeMetadataFields() {
  base::Value::Dict chrome_metadata = policy::GetChromeMetadataValue(
      policy::GetChromeMetadataParams(/*application_name=*/"Support Tool"));

  FindStringAndSetInSupportPacketMetadata(
      chrome_metadata, policy::kChromeMetadataPlatformKey, kPlatformKey);
  FindStringAndSetInSupportPacketMetadata(chrome_metadata,
                                          policy::kChromeMetadataOSKey, kOSKey);
  FindStringAndSetInSupportPacketMetadata(
      chrome_metadata, policy::kChromeMetadataVersionKey, kChromeVersionKey);
  FindStringAndSetInSupportPacketMetadata(
      chrome_metadata, policy::kChromeMetadataRevisionKey, kChromeRevisionKey);
}

void SupportPacketMetadata::FindStringAndSetInSupportPacketMetadata(
    const base::Value::Dict& chrome_metadata,
    const char* chrome_metadata_key,
    const char* support_packet_key) {
  const std::string* value = chrome_metadata.FindString(chrome_metadata_key);
  if (value) {
    metadata_[support_packet_key] = *value;
  }
}

void SupportPacketMetadata::PopulateMetadataContents(
    const base::Time& timestamp,
    const std::vector<std::unique_ptr<DataCollector>>& data_collectors_included,
    base::OnceClosure on_metadata_contents_populated) {
  metadata_[kDataCollectorListKey] =
      GetDataCollectorsListString(data_collectors_included);
  metadata_[kTimestampKey] = GetTimestampString(timestamp);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(
          base::BindOnce(&SupportPacketMetadata::OnMachineStatisticsLoaded,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(on_metadata_contents_populated)));
#else
  std::move(on_metadata_contents_populated).Run();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SupportPacketMetadata::OnMachineStatisticsLoaded(
    base::OnceClosure on_metadata_contents_populated) {
  const std::optional<std::string_view> machine_serial =
      ash::system::StatisticsProvider::GetInstance()->GetMachineID();
  if (machine_serial && !machine_serial->empty()) {
    pii_[PIIType::kSerial].insert(std::string(machine_serial.value()));
    metadata_[kSerialNumberKey] = std::string(machine_serial.value());
  }
  std::move(on_metadata_contents_populated).Run();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const std::string& SupportPacketMetadata::GetCaseId() {
  return metadata_[kSupportCaseIdKey];
}

const PIIMap& SupportPacketMetadata::GetPII() {
  return pii_;
}

void SupportPacketMetadata::InsertErrors(
    const std::set<SupportToolError>& errors) {
  error_messages_.reserve(error_messages_.size() + errors.size());
  for (const auto& error : errors) {
    error_messages_.push_back(error.error_message);
  }
}

std::string SupportPacketMetadata::GetTimestampString(const base::Time& time) {
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTimeWithTimeZone(time));
}

std::string SupportPacketMetadata::GetDataCollectorsListString(
    const std::vector<std::unique_ptr<DataCollector>>&
        data_collectors_included) {
  std::vector<std::string> names;
  for (const auto& data_collector : data_collectors_included) {
    names.push_back(data_collector->GetName());
  }
  return base::JoinString(names, "\n");
}

std::string SupportPacketMetadata::GetGUIDForSupportPacket() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

void SupportPacketMetadata::AddErrorMessagesToMetadata() {
  metadata_[kErrorMessagesKey] = base::JoinString(error_messages_, "\n");
}

void SupportPacketMetadata::WriteMetadataFile(
    base::FilePath target_path,
    std::set<redaction::PIIType> pii_to_keep,
    base::OnceClosure on_metadata_file_written) {
  AddErrorMessagesToMetadata();
  base::FilePath metadata_file =
      target_path.Append(FILE_PATH_LITERAL("metadata.txt"));
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteContentsOnFile, metadata_file, pii_to_keep,
                     metadata_),
      std::move(on_metadata_file_written));
}
