// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/debug_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/pii_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

UiHierarchyDataCollector::UiHierarchyDataCollector() = default;
UiHierarchyDataCollector::~UiHierarchyDataCollector() = default;

namespace {

UIHierarchyData CollectUiHierarchyData() {
  std::ostringstream out;
  out << "UI Hierarchy: Windows\n";
  std::vector<std::string> window_titles =
      ash::debug::PrintWindowHierarchy(&out, /*scrub_data=*/false);

  out << "UI Hierarchy: Views\n";
  ash::debug::PrintViewHierarchy(&out);

  out << "UI Hierarchy: Layers\n";
  ash::debug::PrintLayerHierarchy(&out);

  return UIHierarchyData(window_titles, /*data=*/out.str());
}

bool WriteOutputFile(std::string data, base::FilePath target_directory) {
  return base::WriteFile(
      target_directory.Append(FILE_PATH_LITERAL("ui_hierarchy")), data);
}

}  // namespace

UIHierarchyData::UIHierarchyData(std::vector<std::string> window_titles,
                                 std::string data)
    : window_titles(std::move(window_titles)), data(std::move(data)) {}

UIHierarchyData::~UIHierarchyData() = default;

UIHierarchyData::UIHierarchyData(UIHierarchyData&& ui_hierarchy_data) = default;

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
  UIHierarchyData ui_hierarchy_data = CollectUiHierarchyData();
  InsertIntoPIIMap(ui_hierarchy_data.window_titles);
  data_ = std::move(ui_hierarchy_data.data);
  // `data_` can't be empty.
  DCHECK(!data_.empty());
  std::move(on_data_collected_callback).Run(/*error_code=*/absl::nullopt);
}

void UiHierarchyDataCollector::ExportCollectedDataWithPII(
    std::set<feedback::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteOutputFile, data_, target_directory),
      base::BindOnce(&UiHierarchyDataCollector::OnDataExportDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_exported_callback)));
}

void UiHierarchyDataCollector::OnDataExportDone(
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    std::move(on_exported_callback)
        .Run(SupportToolError::kUIHierarchyDataCollectorError);
    return;
  }
  std::move(on_exported_callback).Run(/*error_code=*/absl::nullopt);
}

void UiHierarchyDataCollector::InsertIntoPIIMap(
    const std::vector<std::string>& window_titles) {
  std::set<std::string>& pii_window_titles =
      pii_map_[feedback::PIIType::kUIHierarchyWindowTitles];
  for (auto const& title : window_titles)
    pii_window_titles.insert(title);
}
