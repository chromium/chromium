// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SIGNIN_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SIGNIN_DATA_COLLECTOR_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

class Profile;

// `SigninDataCollector` collects authentication, sign in and token information
// from `AboutSigninInternals`. It will return an error if the user is not
// signed in or is incognito.
class SigninDataCollector : public DataCollector {
 public:
  explicit SigninDataCollector(Profile* profile);

  ~SigninDataCollector() override;

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

 private:
  void OnFileWritten(DataCollectorDoneCallback on_exported_callback,
                     bool success);
  void OnPIIDetected(DataCollectorDoneCallback on_data_collected_callback,
                     PIIMap piiMap);
  void OnPIIRedacted(base::FilePath target_directory,
                     DataCollectorDoneCallback on_exported_callback,
                     std::string json);
  void CollectAccountIds(base::Value::List* accountList);

  SEQUENCE_CHECKER(sequence_checker_);
  PIIMap pii_map_;
  const raw_ptr<Profile> profile_;
  std::string signin_status_;
  base::WeakPtrFactory<SigninDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SIGNIN_DATA_COLLECTOR_H_
