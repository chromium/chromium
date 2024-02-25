// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_POLICY_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_POLICY_DATA_COLLECTOR_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_value_and_status_aggregator.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

class Profile;

extern const char kRedactedPlaceholder[];

// PolicyDataCollector collects available policy values and metadata about
// policy values.
class PolicyDataCollector
    : public DataCollector,
      public policy::PolicyValueAndStatusAggregator::Observer {
 public:
  // `profile` shouldn't be null.
  explicit PolicyDataCollector(Profile* profile);

  ~PolicyDataCollector() override;

  // Overrides from DataCollector.
  std::string GetName() const override;

  std::string GetDescription() const override;

  const PIIMap& GetDetectedPII() override;

  void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container)
      override;

  void ExportCollectedDataWithPII(
      std::set<redaction::PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      DataCollectorDoneCallback on_exported_callback) override;

  // policy::PolicyValueAndStatusAggregator::Observer implementation.
  // This function will not do anything as we collect the policy data when
  // `CollectDataAndDetectPII()` is called and then export the  same data that
  // was collected. Since we only collect the policy data on call basis, we
  // don't need to do anything when a change is observed.
  void OnPolicyValueAndStatusChanged() override;

 private:
  // Detects the PII in `policy_status_` and inserts them to `pii_map_`.
  void DetectPIIInPolicyStatus();

  void RedactPIIInPolicyStatus(std::set<redaction::PIIType> pii_types_to_keep);

  void OnFileWritten(DataCollectorDoneCallback on_exported_callback,
                     bool success);

  SEQUENCE_CHECKER(sequence_checker_);
  PIIMap pii_map_;
  base::Value::Dict policy_values_;
  // Policy status is a dictionary of `status description` -> `status
  // dictionary` where `status dictionary` is a dictionary of `status field key`
  // -> `status field value`.
  base::Value::Dict policy_status_;
  std::unique_ptr<policy::PolicyValueAndStatusAggregator> policy_aggregator_;
  base::WeakPtrFactory<PolicyDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_POLICY_DATA_COLLECTOR_H_
