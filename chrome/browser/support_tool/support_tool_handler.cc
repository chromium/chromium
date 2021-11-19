// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_handler.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "third_party/zlib/google/zip.h"

// Zip archives the contents of `src_path` into `target_path`. Adds ".zip"
// extension to target file path.
void ZipOutput(base::FilePath src_path, base::FilePath target_path) {
  base::FilePath zip_path = target_path.AddExtension(FILE_PATH_LITERAL(".zip"));
  if (!zip::Zip(src_path, zip_path, true)) {
    LOG(ERROR) << "Couldn't zip files";
  }
}

// Creates a unique temp directory to store the output files. The caller is
// responsible for deleting the returned directory.
base::FilePath CreateTempDirForOutput() {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    LOG(ERROR) << "Unable to create temp dir.";
  }
  return temp_dir.Take();
}

SupportToolHandler::SupportToolHandler() = default;
SupportToolHandler::~SupportToolHandler() {
  CleanUp();
}

void SupportToolHandler::CleanUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clean the temporary directory in a worker thread if it hasn't been removed
  // yet.
  if (!temp_dir_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(base::GetDeletePathRecursivelyCallback(),
                       std::move(temp_dir_)));
    temp_dir_.clear();
  }
}

void SupportToolHandler::AddDataCollector(
    std::unique_ptr<DataCollector> collector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(collector);
  data_collectors_.emplace_back(std::move(collector));
}

void SupportToolHandler::CollectSupportData(
    SupportToolDataCollectedCallback on_data_collection_done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!on_data_collection_done_callback.is_null());
  DCHECK(!data_collectors_.empty());

  on_data_collection_done_callback_ =
      std::move(on_data_collection_done_callback);

  base::RepeatingClosure collect_data_barrier_closure = base::BarrierClosure(
      data_collectors_.size(),
      base::BindOnce(&SupportToolHandler::OnAllDataCollected,
                     weak_ptr_factory_.GetWeakPtr()));

  for (auto& data_collector : data_collectors_) {
    data_collector->CollectDataAndDetectPII(base::BindOnce(
        &SupportToolHandler::OnDataCollected, weak_ptr_factory_.GetWeakPtr(),
        collect_data_barrier_closure));
  }
}

void SupportToolHandler::OnDataCollected(
    base::RepeatingClosure barrier_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(barrier_closure).Run();
}

void SupportToolHandler::OnAllDataCollected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& data_collector : data_collectors_) {
    const PIIMap& collected = data_collector->GetDetectedPII();
    // Use std::multipmap.merge() function after migration to C++17.
    detected_pii_.insert(collected.begin(), collected.end());
  }
  std::move(on_data_collection_done_callback_).Run(detected_pii_);
}

void SupportToolHandler::ExportCollectedData(
    std::set<PIIType> pii_types_to_keep,
    base::FilePath target_path,
    base::OnceClosure on_data_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_data_export_done_callback_ = std::move(on_data_exported_callback);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CreateTempDirForOutput),
      base::BindOnce(&SupportToolHandler::ExportIntoTempDir,
                     weak_ptr_factory_.GetWeakPtr(), pii_types_to_keep,
                     target_path));
}

void SupportToolHandler::ExportIntoTempDir(std::set<PIIType> pii_types_to_keep,
                                           base::FilePath target_path,
                                           base::FilePath tmp_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  temp_dir_ = tmp_path;
  base::RepeatingClosure export_data_barrier_closure = base::BarrierClosure(
      data_collectors_.size(),
      base::BindOnce(&SupportToolHandler::OnAllDataCollectorsDoneExporting,
                     weak_ptr_factory_.GetWeakPtr(), tmp_path, target_path));

  for (auto& data_collector : data_collectors_) {
    data_collector->ExportCollectedDataWithPII(
        pii_types_to_keep, tmp_path,
        base::BindOnce(&SupportToolHandler::OnDataCollectorDoneExporting,
                       weak_ptr_factory_.GetWeakPtr(),
                       export_data_barrier_closure));
  }
}

void SupportToolHandler::OnDataCollectorDoneExporting(
    base::RepeatingClosure barrier_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(barrier_closure).Run();
}

void SupportToolHandler::OnAllDataCollectorsDoneExporting(
    base::FilePath tmp_path,
    base::FilePath path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Archive the contents in the `tmp_path` into `path` in a zip file.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&ZipOutput, tmp_path, path),
      base::BindOnce(&SupportToolHandler::OnDataExportDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SupportToolHandler::OnDataExportDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clean-up the temporary directory after exporting the data.
  CleanUp();
  std::move(on_data_export_done_callback_).Run();
}
