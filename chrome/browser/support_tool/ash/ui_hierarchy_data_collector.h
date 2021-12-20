// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_ASH_UI_HIERARCHY_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_ASH_UI_HIERARCHY_DATA_COLLECTOR_H_

#include <set>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/pii_types.h"

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

  // Overrides from DataCollector.
  std::string GetName() const override;

  std::string GetDescription() const override;

  const PIIMap& GetDetectedPII() override;

  void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback) override;

  void ExportCollectedDataWithPII(
      std::set<feedback::PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      DataCollectorDoneCallback on_exported_callback) override;

 private:
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
