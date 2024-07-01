// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/signin_data_collector.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/signin/core/browser/about_signin_internals.h"

namespace {

PIIMap DetectPII(
    std::string signin_status,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  return redaction_tool->Detect(std::move(signin_status));
}

std::string RedactPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    std::string signin_json) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  auto result = redaction_tool->RedactAndKeepSelected(std::move(signin_json),
                                                      pii_types_to_keep);
  return result;
}

// Writes `signin_json` into a file named "signin.json" in `target_directory`.
bool WriteFile(std::string json, base::FilePath target_directory) {
  base::FilePath target_file =
      target_directory.Append(FILE_PATH_LITERAL("signin.json"));
  return base::WriteFile(target_file, json);
}

}  // namespace

SigninDataCollector::SigninDataCollector(Profile* profile)
    : profile_(profile) {}

SigninDataCollector::~SigninDataCollector() = default;

std::string SigninDataCollector::GetName() const {
  return "Signin Data Collector";
}

std::string SigninDataCollector::GetDescription() const {
  return "Collects signin and token information abut the currently signed in "
         "user, and "
         "writes it to \"signin.json\" file.";
}

const PIIMap& SigninDataCollector::GetDetectedPII() {
  return pii_map_;
}

void SigninDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `profile_` can't be null because `SigninDataCollector` will be created
  // after profile is created. Check `SupportToolHandler` and
  // chrome/browser/support_tool/support_tool_util.h for more details.
  CHECK(profile_);

  if (profile_->IsIncognitoProfile()) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "SigninDataCollector can't work without profile or in incognito mode."};
    std::move(on_data_collected_callback).Run(error);
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile_);
  // See AboutSigninInternals::SigninStatus::ToValue.
  base::Value::Dict status = about_signin_internals->GetSigninStatus();
  base::JSONWriter::WriteWithOptions(
      status, base::JSONWriter::OPTIONS_PRETTY_PRINT, &signin_status_);

  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DetectPII, signin_status_, redaction_tool_container),
      base::BindOnce(&SigninDataCollector::OnPIIDetected,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback)));
}

void SigninDataCollector::OnPIIDetected(
    DataCollectorDoneCallback on_data_collected_callback,
    PIIMap piiMap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pii_map_ = piiMap;
  std::move(on_data_collected_callback).Run(/*error=*/std::nullopt);
}

void SigninDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (signin_status_.empty()) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "SigninDataCollector: Status is empty. Can't export empty status."};
    std::move(on_exported_callback).Run(error);
    return;
  }

  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RedactPII, pii_types_to_keep, target_directory,
                     redaction_tool_container, std::move(signin_status_)),
      base::BindOnce(&SigninDataCollector::OnPIIRedacted,
                     weak_ptr_factory_.GetWeakPtr(), target_directory,
                     std::move(on_exported_callback)));
}

void SigninDataCollector::OnPIIRedacted(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback,
    std::string json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteFile, json, target_directory),
      base::BindOnce(&SigninDataCollector::OnFileWritten,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_exported_callback)));
}

void SigninDataCollector::OnFileWritten(
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    SupportToolError error = {SupportToolErrorCode::kDataCollectorError,
                              "SigninDataCollector failed on data export."};
    std::move(on_exported_callback).Run(error);
    return;
  }
  std::move(on_exported_callback).Run(/*error=*/std::nullopt);
}
