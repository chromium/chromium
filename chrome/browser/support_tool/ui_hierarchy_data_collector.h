// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_

#include <set>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chrome/browser/support_tool/data_collector.h"

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
      std::set<PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      DataCollectorDoneCallback on_exported_callback) override;

 private:
  // Runs `on_data_collected_callback` when data collection is done. Returns an
  // error message to the callback if the optional `pii_map` is nullopt.
  void OnDataCollectedAndPIIDetected(
      DataCollectorDoneCallback on_data_collected_callback,
      absl::optional<PIIMap> pii_map);

  // Runs `on_exported_callback` when the data export is done. Returns an error
  // to the callback if `success` is false.
  void OnDataExportDone(DataCollectorDoneCallback on_exported_callback,
                        bool success);

  SEQUENCE_CHECKER(sequence_checker_);
  PIIMap pii_map_;
  base::WeakPtrFactory<UiHierarchyDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_
