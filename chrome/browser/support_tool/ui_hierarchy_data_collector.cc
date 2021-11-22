// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ui_hierarchy_data_collector.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

UiHierarchyDataCollector::UiHierarchyDataCollector() = default;
UiHierarchyDataCollector::~UiHierarchyDataCollector() = default;

namespace {
absl::optional<PIIMap> CollectUiHierarchyData() {
  // Just an empty function to create the skeleton for the DataCollector.
  // Data will be collected and PIIMap will be filled here.
  PIIMap map;
  return map;
}

bool WriteOutputFile(std::string data, base::FilePath target_directory) {
  // Just an empty function. This will create output file in given
  // `target_directory` and write the data to it. Return success for now.
  return true;
}

}  // namespace

std::string UiHierarchyDataCollector::GetName() const {
  return "UI Hierarchy";
}

std::string UiHierarchyDataCollector::GetDescription() const {
  return "Collects UI hiearchy data.";
}

const PIIMap& UiHierarchyDataCollector::GetDetectedPII() {
  return pii_map_;
}

void UiHierarchyDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CollectUiHierarchyData),
      base::BindOnce(&UiHierarchyDataCollector::OnDataCollectedAndPIIDetected,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback)));
}

void UiHierarchyDataCollector::OnDataCollectedAndPIIDetected(
    DataCollectorDoneCallback on_data_collected_callback,
    absl::optional<PIIMap> pii_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pii_map) {
    pii_map_ = pii_map.value();
    std::move(on_data_collected_callback).Run(absl::nullopt);
  } else {
    std::move(on_data_collected_callback)
        .Run(SupportToolError::kUIHierarchyDataCollectorError);
  }
}

void UiHierarchyDataCollector::ExportCollectedDataWithPII(
    std::set<PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteOutputFile, "some place holder data",
                     target_directory),
      base::BindOnce(&UiHierarchyDataCollector::OnDataExportDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_exported_callback)));
}

void UiHierarchyDataCollector::OnDataExportDone(
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success) {
    std::move(on_exported_callback).Run(absl::nullopt);
  } else {
    std::move(on_exported_callback)
        .Run(SupportToolError::kUIHierarchyDataCollectorError);
  }
}
