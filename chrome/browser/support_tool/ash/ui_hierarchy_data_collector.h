// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_ASH_UI_HIERARCHY_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_ASH_UI_HIERARCHY_DATA_COLLECTOR_H_

#include <set>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

struct UIHierarchyData {
  UIHierarchyData(std::vector<std::string> window_titles, std::string data);
  ~UIHierarchyData();

  // We don't need this struct to be copyable.
  UIHierarchyData(const UIHierarchyData& ui_hierarchy_data) = delete;

  UIHierarchyData(UIHierarchyData&& ui_hierarchy_data);

  // The list of window titles that the UI hierarchy data contains. Window
  // titles are considered PII sensitive data.
  std::vector<std::string> window_titles;
  // UI hierarchy data. Contains window hierarchy, view hierarchy and layer
  // hierarchy.
  std::string data;
};

class UiHierarchyDataCollector : public DataCollector {
 public:
  UiHierarchyDataCollector();
  ~UiHierarchyDataCollector() override;

  // Removes UI hierarchy window titles from `data` and returns the redacted
  // version of `data`. Public for testing.
  static std::string RemoveWindowTitles(const std::string& ui_hierarchy_data);

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
  // Creates a "ui_hierarchy.txt" file under `target_directory` and writes
  // `ui_hierarchy_data` into this file. Tries to scrub PII sensitive data in
  // `ui_hierarchy_data` when writing to it except the data under PII categories
  // in `pii_types_to_keep`.
  static bool WriteOutputFile(std::string ui_hierarchy_data,
                              base::FilePath target_directory,
                              std::set<redaction::PIIType> pii_types_to_keep);

  // Runs `on_exported_callback` when the data export is done. Returns an error
  // to the callback if `success` is false.
  void OnDataExportDone(DataCollectorDoneCallback on_exported_callback,
                        bool success);

  // Inserts the contents of `window_titles` into `pii_map_` of this, with key
  // PIIType::kUIHierarchyWindowTitles.
  void InsertIntoPIIMap(const std::vector<std::string>& window_titles);

  SEQUENCE_CHECKER(sequence_checker_);
  // PII sensitive information that the collected `data_` contains.
  PIIMap pii_map_;
  // UI hierarchy data that the UiHierarchyDataCollector instance collected.
  // Contains window hierarchy, view hierarchy and layer hierarchy.
  std::string data_;
  base::WeakPtrFactory<UiHierarchyDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_ASH_UI_HIERARCHY_DATA_COLLECTOR_H_
