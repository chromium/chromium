// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/policy_data_collector.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_ui_utils.h"
#include "chrome/browser/policy/policy_value_and_status_aggregator.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/policy/core/browser/webui/json_generation.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/browser/webui/policy_webui_constants.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/policy/core/browser/webui/machine_level_user_cloud_policy_status_provider.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// Returns the PII type that `status_field` is categorised in if it's considered
// as PII.
std::optional<redaction::PIIType> GetPIITypeOfStatusField(
    std::string_view status_field) {
  // List of keys in policy status that will be considered as PII and will be
  // redacted selectively.
  // TODO(crbug.com/41486252): Convert to MakeFixedFlatMap().
  static constexpr auto kPersonallyIdentifiableStatusFields =
      base::MakeFixedFlatMap<std::string_view, redaction::PIIType>({
#if !BUILDFLAG(IS_CHROMEOS_ASH)
          {policy::kDeviceIdKey, redaction::PIIType::kStableIdentifier},
          {policy::kEnrollmentTokenKey, redaction::PIIType::kStableIdentifier},
          {policy::kMachineKey, redaction::PIIType::kStableIdentifier},
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
          {policy::kAssetIdKey, redaction::PIIType::kStableIdentifier},
          // kLocationKey is the "Asset location" which is an identifier for
          // the device that is set during enterprise enrollment or by the
          // administrator.
          {policy::kLocationKey, redaction::PIIType::kStableIdentifier},
          {policy::kDirectoryApiIdKey, redaction::PIIType::kStableIdentifier},
          {policy::kGaiaIdKey, redaction::PIIType::kGaiaID},
          {policy::kClientIdKey, redaction::PIIType::kStableIdentifier},
          {policy::kUsernameKey, redaction::PIIType::kEmail},
          {policy::kEnterpriseDomainManagerKey, redaction::PIIType::kEmail},
          {policy::kDomainKey, redaction::PIIType::kEmail}});
  return kPersonallyIdentifiableStatusFields.contains(status_field)
             ? std::make_optional(
                   kPersonallyIdentifiableStatusFields.at(status_field))
             : std::nullopt;
}

// Opens a file named "policies.json" in `target_directory` and writes
// `policy_data`.
bool WritePolicyData(std::string policy_data, base::FilePath target_directory) {
  base::FilePath target_file =
      target_directory.Append(FILE_PATH_LITERAL("policies.json"));
  return base::WriteFile(target_file, policy_data);
}

}  // namespace

const char kRedactedPlaceholder[] = "REDACTED";

PolicyDataCollector::PolicyDataCollector(Profile* profile) {
  policy_aggregator_ = policy::PolicyValueAndStatusAggregator::
      CreateDefaultPolicyValueAndStatusAggregator(profile);
}

PolicyDataCollector::~PolicyDataCollector() = default;

std::string PolicyDataCollector::GetName() const {
  return "Policies";
}

std::string PolicyDataCollector::GetDescription() const {
  return "Collects available policy values and metadata about policies and "
         "writes it to \"policies.json\" file.";
}

const PIIMap& PolicyDataCollector::GetDetectedPII() {
  return pii_map_;
}

void PolicyDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  policy_values_ = policy_aggregator_->GetAggregatedPolicyValues();
  // `policy_aggregator_` returns the list of policy IDs in
  // policy::kPolicyIdsKey. Remove the list of policy IDs from the output as we
  // don't need it and don't want it to clutter the output.
  policy_values_.Remove(policy::kPolicyIdsKey);

  policy_status_ = policy_aggregator_->GetAggregatedPolicyStatus();
  // Policy values are not expected to contain PII so we only detect the PII in
  // policy status.
  DetectPIIInPolicyStatus();

  std::move(on_data_collected_callback).Run(/*error=*/std::nullopt);
}

void PolicyDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Policy values are not expected to contain PII so we only redact the PII in
  // policy status.
  RedactPIIInPolicyStatus(pii_types_to_keep);

  std::string policy_json = policy::GenerateJson(
      std::move(policy_values_), std::move(policy_status_),
      policy::GetChromeMetadataParams(/*application_name=*/"Support Tool"));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WritePolicyData, policy_json, target_directory),
      base::BindOnce(&PolicyDataCollector::OnFileWritten,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_exported_callback)));
}

void PolicyDataCollector::OnFileWritten(
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    SupportToolError error = {SupportToolErrorCode::kDataCollectorError,
                              "PolicyDataCollector failed on data export."};
    std::move(on_exported_callback).Run(error);
    return;
  }
  std::move(on_exported_callback).Run(/*error=*/std::nullopt);
}

void PolicyDataCollector::OnPolicyValueAndStatusChanged() {}

void PolicyDataCollector::DetectPIIInPolicyStatus() {
  for (auto entry : policy_status_) {
    for (const auto [status_key, status_value] : entry.second.GetDict()) {
      std::optional<redaction::PIIType> pii_type =
          GetPIITypeOfStatusField(status_key);
      if (!pii_type)
        continue;
      // A field of policy status can be stored in string or bool. Convert it to
      // string.
      std::string status_value_as_string;
      base::JSONWriter::WriteWithOptions(status_value,
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                         &status_value_as_string);
      pii_map_[pii_type.value()].insert(status_value_as_string);
    }
  }
}

void PolicyDataCollector::RedactPIIInPolicyStatus(
    std::set<redaction::PIIType> pii_types_to_keep) {
  // Iterator is a reference std::pair<const std::string&, Value&> as explained
  // in base/value_iterators.h documentation.
  for (std::pair<const std::string&, base::Value&> entry : policy_status_) {
    for (std::pair<const std::string&, base::Value&> status_pair :
         entry.second.GetDict()) {
      std::optional<redaction::PIIType> pii_type =
          GetPIITypeOfStatusField(status_pair.first);
      if (!pii_type || base::Contains(pii_types_to_keep, pii_type.value()))
        continue;
      // Replace the value with `kRedactedPlaceholder` if it's a PII type that
      // user didn't select to keep.
      status_pair.second = base::Value(kRedactedPlaceholder);
    }
  }
}
