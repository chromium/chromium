// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_

#include <set>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
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
      base::OnceClosure on_data_collected_callback) override;

  void ExportCollectedDataWithPII(
      std::set<PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      base::OnceClosure on_exported_callback) override;

 private:
  void CollectUiHierarchyData();

  void WriteOutputFile(base::FilePath target_directory);

  PIIMap pii_map_;
  base::WeakPtrFactory<UiHierarchyDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_
