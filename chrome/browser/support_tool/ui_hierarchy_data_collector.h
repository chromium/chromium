// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_

#include "base/callback_forward.h"
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
      base::OnceCallback<void()> on_data_collected_callback) override;

 private:
  PIIMap pii_map_;
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_UI_HIERARCHY_DATA_COLLECTOR_H_
