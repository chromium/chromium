// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ui_hierarchy_data_collector.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"

UiHierarchyDataCollector::UiHierarchyDataCollector() = default;
UiHierarchyDataCollector::~UiHierarchyDataCollector() = default;

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
    base::OnceCallback<void()> on_data_collected_callback) {
  // This function Will be filled later.
  // Data will be collected and this.pii_map_ will be filled here.
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(std::move(on_data_collected_callback)));
}
