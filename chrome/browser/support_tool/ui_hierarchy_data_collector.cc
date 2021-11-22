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

std::string UiHierarchyDataCollector::GetName() const {
  return "Ui Hierarchy";
}

std::string UiHierarchyDataCollector::GetDescription() const {
  return "Collects UI hiearchy data.";
}

const PIIMap& UiHierarchyDataCollector::GetDetectedPII() {
  return pii_map_;
}

void UiHierarchyDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback) {
  // This function Will be filled later.
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&UiHierarchyDataCollector::CollectUiHierarchyData,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback)));
}

void UiHierarchyDataCollector::CollectUiHierarchyData(
    DataCollectorDoneCallback on_data_collected_callback) {
  // Just an empty function to create the skeleton for the DataCollector.
  // Data will be collected and this.pii_map_ will be filled here.
  std::move(on_data_collected_callback).Run(absl::nullopt);
}

void UiHierarchyDataCollector::ExportCollectedDataWithPII(
    std::set<PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&UiHierarchyDataCollector::WriteOutputFile,
                     weak_ptr_factory_.GetWeakPtr(), target_directory,
                     std::move(on_exported_callback)));
}

void UiHierarchyDataCollector::WriteOutputFile(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback) {
  // Opens a file under `target_directory` and writes the output to the file.
  std::move(on_exported_callback).Run(absl::nullopt);
}
