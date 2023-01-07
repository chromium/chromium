// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/unittest_utils.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"

namespace performance_manager {
namespace testing {

TestWithPerformanceManager::TestWithPerformanceManager() = default;

TestWithPerformanceManager::~TestWithPerformanceManager() = default;

void TestWithPerformanceManager::SetUp() {
  EXPECT_FALSE(PerformanceManagerImpl::IsAvailable());
  performance_manager_ = PerformanceManagerImpl::Create(base::DoNothing());
  // Make sure creation registers the created instance.
  EXPECT_TRUE(PerformanceManagerImpl::IsAvailable());
}

void TestWithPerformanceManager::TearDown() {
  PerformanceManagerImpl::Destroy(std::move(performance_manager_));
  // Make sure destruction unregisters the instance.
  EXPECT_FALSE(PerformanceManagerImpl::IsAvailable());

  task_environment_.RunUntilIdle();
}

}  // namespace testing
}  // namespace performance_manager
