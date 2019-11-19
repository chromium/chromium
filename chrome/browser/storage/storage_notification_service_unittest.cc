// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/storage_notification_service.h"

#include "base/bind.h"
#include "chrome/browser/storage/storage_notification_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class StorageNotificationTestingProfile : public TestingProfile {
 public:
  StorageNotificationTestingProfile() = default;
  ~StorageNotificationTestingProfile() override = default;

  StorageNotificationServiceImpl* GetStorageNotificationService() override {
    return StorageNotificationServiceFactory::GetForBrowserContext(this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StorageNotificationTestingProfile);
};

class StorageNotificationServiceTest : public testing::Test {
 public:
  StorageNotificationServiceTest() {
    profile_ = std::make_unique<StorageNotificationTestingProfile>();
  }
  ~StorageNotificationServiceTest() override {}

 protected:
  StorageNotificationTestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<StorageNotificationTestingProfile> profile_;
};

TEST_F(StorageNotificationServiceTest, Default) {
  StorageNotificationServiceImpl* storage_notification_service =
      static_cast<StorageNotificationServiceImpl*>(
          profile()->GetStorageNotificationService());
  ASSERT_TRUE(storage_notification_service);

  base::RepeatingClosure callback =
      storage_notification_service->GetStoragePressureNotificationClosure();
  ASSERT_TRUE(callback);
}
