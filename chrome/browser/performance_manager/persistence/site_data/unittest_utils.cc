// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/unittest_utils.h"
#include "base/bind_helpers.h"
#include "base/callback.h"

#include <utility>

namespace performance_manager {
namespace testing {

MockSiteDataImplOnDestroyDelegate::MockSiteDataImplOnDestroyDelegate() =
    default;
MockSiteDataImplOnDestroyDelegate::~MockSiteDataImplOnDestroyDelegate() =
    default;

NoopSiteDataStore::NoopSiteDataStore() = default;
NoopSiteDataStore::~NoopSiteDataStore() = default;

void NoopSiteDataStore::ReadSiteDataFromStore(
    const url::Origin& origin,
    ReadSiteDataFromStoreCallback callback) {
  std::move(callback).Run(base::nullopt);
}

void NoopSiteDataStore::WriteSiteDataIntoStore(
    const url::Origin& origin,
    const SiteDataProto& site_characteristic_proto) {}

void NoopSiteDataStore::RemoveSiteDataFromStore(
    const std::vector<url::Origin>& site_origins) {}

void NoopSiteDataStore::ClearStore() {}

void NoopSiteDataStore::GetStoreSize(GetStoreSizeCallback callback) {
  std::move(callback).Run(base::nullopt, base::nullopt);
}

void NoopSiteDataStore::SetInitializationCallbackForTesting(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

TestWithPerformanceManager::TestWithPerformanceManager() = default;

TestWithPerformanceManager::~TestWithPerformanceManager() = default;

void TestWithPerformanceManager::SetUp() {
  EXPECT_EQ(nullptr, PerformanceManagerImpl::GetInstance());
  performance_manager_ = PerformanceManagerImpl::Create(base::DoNothing());
  // Make sure creation registers the created instance.
  EXPECT_EQ(performance_manager_.get(), PerformanceManagerImpl::GetInstance());
}

void TestWithPerformanceManager::TearDown() {
  PerformanceManagerImpl::Destroy(std::move(performance_manager_));
  // Make sure destruction unregisters the instance.
  EXPECT_EQ(nullptr, PerformanceManagerImpl::GetInstance());

  task_environment_.RunUntilIdle();
}

}  // namespace testing
}  // namespace performance_manager
