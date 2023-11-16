// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include "base/check_op.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager::user_tuning {
namespace {

PerformanceDetectionManager* g_performance_detection_manager = nullptr;

}  // namespace

void PerformanceDetectionManager::AddStatusObserver(
    ResourceTypeSet resource_types,
    StatusObserver* o) {
  // TODO(joenotcharles): Implement method
  for (auto resource_type : resource_types) {
    o->OnStatusChanged(resource_type, side_panel::mojom::HealthLevel::kHealthy,
                       false);
  }
}
void PerformanceDetectionManager::RemoveStatusObserver(
    ResourceTypeSet resource_types,
    StatusObserver* o) {
  // TODO(joenotcharles): Implement method
}
void PerformanceDetectionManager::RequestStatus(ResourceTypeSet resource_types,
                                                StatusObserver* o) {
  // TODO(joenotcharles): Implement method
}

void PerformanceDetectionManager::AddActionableTabsObserver(
    ResourceTypeSet resource_types,
    ActionableTabsObserver* o) {
  // TODO(joenotcharles): Implement method
  for (auto resource_type : resource_types) {
    o->OnActionableTabListChanged(resource_type, {});
  }
}
void PerformanceDetectionManager::RemoveActionableTabsObserver(
    ResourceTypeSet resource_types,
    ActionableTabsObserver* o) {
  // TODO(joenotcharles): Implement method
}
void PerformanceDetectionManager::RequestActionableTabs(
    ResourceTypeSet resource_types,
    ActionableTabsObserver* o) {
  // TODO(joenotcharles): Implement method
}

PerformanceDetectionManager::PerformanceDetectionManager() {
  CHECK(!g_performance_detection_manager);
  g_performance_detection_manager = this;
}

void PerformanceDetectionManager::Start() {}

// static
bool PerformanceDetectionManager::HasInstance() {
  return g_performance_detection_manager;
}

// static
PerformanceDetectionManager* PerformanceDetectionManager::GetInstance() {
  CHECK(g_performance_detection_manager);
  return g_performance_detection_manager;
}

PerformanceDetectionManager::~PerformanceDetectionManager() {
  CHECK_EQ(this, g_performance_detection_manager);
  g_performance_detection_manager = nullptr;
}

}  // namespace performance_manager::user_tuning
