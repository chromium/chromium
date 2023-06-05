// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class StorageAccessAPIServiceImplTest : public testing::Test {
 public:
  StorageAccessAPIServiceImplTest() = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("TestProfile");
    service_ = StorageAccessAPIServiceFactory::GetForBrowserContext(profile_);
    ASSERT_NE(service_, nullptr);
  }

  void TearDown() override {
    DCHECK(service_);
    // Even though we reassign this in SetUp, service may be persisted between
    // tests if the factory has already created a service for the testing
    // profile being used.
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  content::BrowserTaskEnvironment& env() { return env_; }

 protected:
  Profile* profile() { return profile_; }
  StorageAccessAPIServiceImpl* service() { return service_; }

 private:
  content::BrowserTaskEnvironment env_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_;
  raw_ptr<StorageAccessAPIServiceImpl> service_;
};

TEST_F(StorageAccessAPIServiceImplTest, RenewPermissionGrant) {
  StorageAccessAPIServiceImpl* service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile());

  ASSERT_NE(nullptr, service);

  // TODO(https://crbug.com/1450356): test grant renewal.
}
