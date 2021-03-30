// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "components/update_client/update_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using update_client::UpdateClient;

namespace crosapi {
namespace {

// Copied from browser_loader.cc
constexpr char kLacrosComponentName[] = "lacros-dogfood-dev";
constexpr char kLacrosComponentId[] = "ldobopbhiamakmncndpkeelenhdmgfhk";

// Delegate for testing.
class DelegateImpl : public BrowserLoader::Delegate {
 public:
  DelegateImpl() = default;
  DelegateImpl(const DelegateImpl&) = delete;
  DelegateImpl& operator=(const DelegateImpl&) = delete;
  ~DelegateImpl() override = default;

  // BrowserLoader::Delegate:
  void SetLacrosUpdateAvailable() override { ++set_lacros_update_available_; }

  // Public because this is test code.
  int set_lacros_update_available_ = 0;
};

class BrowserLoaderTest : public testing::Test {
 public:
  BrowserLoaderTest() { browser_util::SetLacrosEnabledForTest(true); }

  ~BrowserLoaderTest() override {
    browser_util::SetLacrosEnabledForTest(false);
  }

  // Public because this is test code.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserLoaderTest, ShowUpdateNotification) {
  // Create dependencies for object under test.
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager =
      base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
  component_manager->set_supported_components({kLacrosComponentName});
  component_manager->ResetComponentState(
      kLacrosComponentName,
      component_updater::FakeCrOSComponentManager::ComponentInfo(
          component_updater::CrOSComponentManager::Error::NONE,
          base::FilePath("/install/path"), base::FilePath("/mount/path")));
  BrowserProcessPlatformPartTestApi browser_part(
      g_browser_process->platform_part());
  browser_part.InitializeCrosComponentManager(component_manager);

  // Create object under test.
  auto delegate_ptr = std::make_unique<DelegateImpl>();
  DelegateImpl* delegate = delegate_ptr.get();
  BrowserLoader browser_loader(std::move(delegate_ptr), component_manager);

  // Creating the loader does not trigger an update notification.
  EXPECT_EQ(0, delegate->set_lacros_update_available_);

  // The initial load of the component does not trigger an update notification.
  base::RunLoop run_loop;
  browser_loader.Load(base::BindLambdaForTesting(
      [&](const base::FilePath&) { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_EQ(0, delegate->set_lacros_update_available_);

  // Update check does not trigger an update notification.
  browser_loader.OnEvent(
      UpdateClient::Observer::Events::COMPONENT_CHECKING_FOR_UPDATES,
      kLacrosComponentId);
  EXPECT_EQ(0, delegate->set_lacros_update_available_);

  // Update download does not trigger an update notification.
  browser_loader.OnEvent(
      UpdateClient::Observer::Events::COMPONENT_UPDATE_DOWNLOADING,
      kLacrosComponentId);
  EXPECT_EQ(0, delegate->set_lacros_update_available_);

  // Update completion trigger the notification.
  browser_loader.OnEvent(UpdateClient::Observer::Events::COMPONENT_UPDATED,
                         kLacrosComponentId);
  EXPECT_EQ(1, delegate->set_lacros_update_available_);

  browser_part.ShutdownCrosComponentManager();
}

}  // namespace
}  // namespace crosapi
