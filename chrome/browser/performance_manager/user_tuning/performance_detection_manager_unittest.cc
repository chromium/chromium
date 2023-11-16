// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include <vector>

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {

namespace {
class MockPerformanceDetectionManagerStatusObserver
    : public PerformanceDetectionManager::StatusObserver {
 public:
  MOCK_METHOD(void,
              OnStatusChanged,
              (side_panel::mojom::ResourceType,
               side_panel::mojom::HealthLevel,
               bool),
              (override));
};

class MockPerformanceDetectionManagerActionableTabsObserver
    : public PerformanceDetectionManager::ActionableTabsObserver {
 public:
  MOCK_METHOD(void,
              OnActionableTabListChanged,
              (side_panel::mojom::ResourceType,
               std::vector<content::WebContents*>),
              (override));
};
}  // namespace

class PerformanceDetectionManagerTest : public ::testing::Test {
 public:
  void StartManager() {
    manager_.reset(new PerformanceDetectionManager());
    manager()->Start();
  }

  PerformanceDetectionManager* manager() {
    return PerformanceDetectionManager::GetInstance();
  }

  std::unique_ptr<PerformanceDetectionManager> manager_;
};

TEST_F(PerformanceDetectionManagerTest, ReturnsInstance) {
  StartManager();
  EXPECT_NE(manager(), nullptr);
}

TEST_F(PerformanceDetectionManagerTest, HasInstance) {
  EXPECT_FALSE(PerformanceDetectionManager::HasInstance());
  StartManager();
  EXPECT_TRUE(PerformanceDetectionManager::HasInstance());
}

TEST_F(PerformanceDetectionManagerTest, StatusObserverCalledOnObserve) {
  StartManager();

  MockPerformanceDetectionManagerStatusObserver observer;
  EXPECT_CALL(observer, OnStatusChanged).Times(1);
  PerformanceDetectionManager::ResourceTypeSet resources;
  resources.Put(side_panel::mojom::ResourceType::kMemory);
  manager()->AddStatusObserver(resources, &observer);
}

TEST_F(PerformanceDetectionManagerTest, ActionableTabsObserverCalledOnObserve) {
  StartManager();

  MockPerformanceDetectionManagerActionableTabsObserver observer;
  EXPECT_CALL(observer, OnActionableTabListChanged).Times(1);
  PerformanceDetectionManager::ResourceTypeSet resources;
  resources.Put(side_panel::mojom::ResourceType::kMemory);
  manager()->AddActionableTabsObserver(resources, &observer);
}

}  // namespace performance_manager::user_tuning
