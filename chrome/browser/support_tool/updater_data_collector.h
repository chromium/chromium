// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_UPDATER_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_UPDATER_DATA_COLLECTOR_H_

#include <optional>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

// UpdaterDataCollector gathers logs, history, and preferences from the Chrome
// Updater for both system and user scopes. Files collected from the updater may
// be large (on the order of MiB). The implementation avoids loading all files
// into memory at once.
class UpdaterDataCollector final : public DataCollector {
 public:
  // Initializes a collector which operates on the provided per-user and
  // per-system installation directories. These are primarily configurable to
  // facilitate testing. Directories need not exist on the filesystem, however
  // if either value is nullopt, CollectDataAndDetectPII fails with a
  // SupportCodeError.
  explicit UpdaterDataCollector(
      std::optional<base::FilePath> user_install_dir =
          updater::GetInstallDirectory(updater::UpdaterScope::kUser),
      std::optional<base::FilePath> system_install_dir =
          updater::GetInstallDirectory(updater::UpdaterScope::kSystem));
  ~UpdaterDataCollector() override;

  std::string GetName() const override;

  std::string GetDescription() const override;

  const PIIMap& GetDetectedPII() override;

  // Copies data from the updater's install directory to a temporary directory.
  // PII detection is performed on these temporary copies. Snapshotting avoids
  // TOCTOU errors in PII detection, as the updater's files can change
  // concurrently.
  void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container)
      override;

  // Reads the temporary copies made by CollectDataAndDetectPII and performs
  // redaction. Fails with a SupportToolError if called before
  // CollectDataAndDetectPII.
  void ExportCollectedDataWithPII(
      std::set<redaction::PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      DataCollectorDoneCallback on_exported_callback) override;

 private:
  void SetTempDir(const base::FilePath& temp_dir);
  void OnPIIDetected(DataCollectorDoneCallback callback,
                     base::expected<PIIMap, SupportToolError> pii_map);

  SEQUENCE_CHECKER(sequence_checker_);
  const scoped_refptr<base::SequencedTaskRunner> io_sequence_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  const std::optional<base::FilePath> user_install_dir_;
  const std::optional<base::FilePath> system_install_dir_;
  // ScopedTempDir is intentionally not used as the directory must be created
  // and deleted on a sequence which allows blocking.
  base::FilePath temp_dir_;
  PIIMap pii_map_;
  base::WeakPtrFactory<UpdaterDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_UPDATER_DATA_COLLECTOR_H_
