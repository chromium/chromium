// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_service_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(HistoryServiceFactoryTest, LocalDeviceInfoProvider) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      DeviceInfoSyncServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<syncer::FakeDeviceInfoSyncService>();
      }));
  builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                            HistoryServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = builder.Build();

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile.get(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  EXPECT_EQ(history_service->local_device_info_provider_for_testing(),
            DeviceInfoSyncServiceFactory::GetForProfile(profile.get())
                ->GetLocalDeviceInfoProvider());
}

}  // namespace
