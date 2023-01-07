// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_UNITTEST_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_UNITTEST_UTILS_H_

#include <memory>

#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace testing {

class TestWithPerformanceManager : public ::testing::Test {
 public:
  TestWithPerformanceManager();

  TestWithPerformanceManager(const TestWithPerformanceManager&) = delete;
  TestWithPerformanceManager& operator=(const TestWithPerformanceManager&) =
      delete;

  ~TestWithPerformanceManager() override;

  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<PerformanceManagerImpl> performance_manager_;
  content::BrowserTaskEnvironment task_environment_;
};

}  // namespace testing
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_UNITTEST_UTILS_H_
