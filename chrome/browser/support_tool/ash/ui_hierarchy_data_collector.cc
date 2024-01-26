// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/public/cpp/debug_utils.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "third_party/re2/src/re2/re2.h"

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

}  // namespace

UIHierarchyData::UIHierarchyData(std::vector<std::string> window_titles,
                                 std::string data)
    : window_titles(std::move(window_titles)), data(std::move(data)) {}

UIHierarchyData::~UIHierarchyData() = default;

UIHierarchyData::UIHierarchyData(UIHierarchyData&& ui_hierarchy_data) = default;

bool UiHierarchyDataCollector::WriteOutputFile(
    std::string ui_hierarchy_data,
    base::FilePath target_directory,
    std::set<redaction::PIIType> pii_types_to_keep) {
  if (pii_types_to_keep.count(redaction::PIIType::kUIHierarchyWindowTitles) ==
      0) {
    ui_hierarchy_data = RemoveWindowTitles(ui_hierarchy_data);
  }
  return base::WriteFile(
      target_directory.Append(FILE_PATH_LITERAL("ui_hierarchy.txt")),
      ui_hierarchy_data);
}

std::string UiHierarchyDataCollector::RemoveWindowTitles(
    const std::string& ui_hierarchy_data) {
  // `ui_hierarchy_data` stores every component in a new line. Window titles are
  // stored in "title=<window title>\n" format in `ui_hierarchy_data`.
  re2::RE2 regex_pattern("(?s)(.*?)title=(?-s)(.+)\\n");
  std::string_view input(ui_hierarchy_data);

  // `regex_pattern` has two matching groups: first one is for the skipped input
  // that doesn't contain any window titles and second one is for the matched
  // window title.
  std::string_view skipped_part;
  std::string_view matched_window_title;
  std::string redacted;

  while (re2::RE2::Consume(&input, regex_pattern, &skipped_part,
                           &matched_window_title)) {
    redacted.append(skipped_part);
    redacted += "title=<REDACTED>\n";
  }
  // Append the rest of the input to `redacted`. Only the unmatched last part
  // will be present in the `input` as we're using Consume() function.
  redacted.append(input);
  return redacted;
}

std::string UiHierarchyDataCollector::GetName() const {
  return "UI Hierarchy";
}

std::string UiHierarchyDataCollector::GetDescription() const {
  return "Collects UI hiearchy data and exports the data into file named "
         "ui_hierarchy.";
}

const PIIMap& UiHierarchyDataCollector::GetDetectedPII() {
  return pii_map_;
}

void UiHierarchyDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UIHierarchyData ui_hierarchy_data = CollectUiHierarchyData();
  InsertIntoPIIMap(ui_hierarchy_data.window_titles);
  data_ = std::move(ui_hierarchy_data.data);
  // `data_` can't be empty.
  DCHECK(!data_.empty());
  std::move(on_data_collected_callback).Run(/*error=*/std::nullopt);
}

void UiHierarchyDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&UiHierarchyDataCollector::WriteOutputFile, data_,
                     target_directory, pii_types_to_keep),
      base::BindOnce(&UiHierarchyDataCollector::OnDataExportDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_exported_callback)));
}

void UiHierarchyDataCollector::OnDataExportDone(
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "UiHierarchyDataCollector failed on data export."};
    std::move(on_exported_callback).Run(error);
    return;
  }
  std::move(on_exported_callback).Run(/*error=*/std::nullopt);
}

void UiHierarchyDataCollector::InsertIntoPIIMap(
    const std::vector<std::string>& window_titles) {
  std::set<std::string>& pii_window_titles =
      pii_map_[redaction::PIIType::kUIHierarchyWindowTitles];
  for (auto const& title : window_titles)
    pii_window_titles.insert(title);
}
