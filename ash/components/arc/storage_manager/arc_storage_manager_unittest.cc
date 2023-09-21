// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/storage_manager/arc_storage_manager.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_storage_manager_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcStorageManagerTest : public testing::Test {
 protected:
  ArcStorageManagerTest()
      : bridge_(ArcStorageManager::GetForBrowserContextForTesting(&context_)) {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->storage_manager()
        ->SetInstance(&storage_manager_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->storage_manager());
  }
  ArcStorageManagerTest(const ArcStorageManagerTest&) = delete;
  ArcStorageManagerTest& operator=(const ArcStorageManagerTest&) = delete;
  ~ArcStorageManagerTest() override = default;

  const FakeStorageManagerInstance* storage_manager_instance() const {
    return &storage_manager_instance_;
  }
  ArcStorageManager* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeStorageManagerInstance storage_manager_instance_;
  user_prefs::TestBrowserContextWithPrefs context_;
  const raw_ptr<ArcStorageManager, ExperimentalAsh> bridge_;
};

TEST_F(ArcStorageManagerTest, ConstructDestruct) {}

// Tests that calling OpenPrivateVolumeSettings() ends up calling the mojo
// instance.
TEST_F(ArcStorageManagerTest, OpenPrivateVolumeSettings) {
  ASSERT_NE(nullptr, bridge());
  EXPECT_TRUE(bridge()->OpenPrivateVolumeSettings());
  EXPECT_EQ(
      1u,
      storage_manager_instance()->num_open_private_volume_settings_called());
}

// Tests that calling GetApplicationsSize() ends up calling the mojo instance.
// Also verifies that the bridge passes the callback to the instance.
TEST_F(ArcStorageManagerTest, GetApplicationsSize) {
  ASSERT_NE(nullptr, bridge());
  bool called = false;
  EXPECT_TRUE(bridge()->GetApplicationsSize(base::BindLambdaForTesting(
      [&called](bool, mojom::ApplicationsSizePtr) { called = true; })));
  EXPECT_EQ(1u, storage_manager_instance()->num_get_applications_size_called());
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace arc
