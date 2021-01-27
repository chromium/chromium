// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process_platform_part_base.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kDesktopFileId[] = "dummy1";
constexpr int kDisplayId = 0;

namespace crostini {

class CrostiniUtilTest : public testing::Test {
 public:
  void SuccessCallback(bool expected_success,
                       const std::string& expected_failure_reason,
                       bool success,
                       const std::string& failure_reason) {
    EXPECT_EQ(expected_success, success);
    EXPECT_EQ(expected_failure_reason, failure_reason);
    run_loop_->Quit();
  }

  CrostiniUtilTest()
      : app_id_(crostini::CrostiniTestHelper::GenerateAppId(kDesktopFileId)),
        task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        browser_part_(g_browser_process->platform_part()) {
    chromeos::DBusThreadManager::Initialize();

    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }
  ~CrostiniUtilTest() override { chromeos::DBusThreadManager::Shutdown(); }
  CrostiniUtilTest(const CrostiniUtilTest&) = delete;
  CrostiniUtilTest& operator=(const CrostiniUtilTest&) = delete;

  void SetUp() override {
    chromeos::DlcserviceClient::InitializeFake();

    component_manager_ =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    component_manager_->set_supported_components({"cros-termina"});
    component_manager_->ResetComponentState(
        "cros-termina",
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path")));
    browser_part_.InitializeCrosComponentManager(component_manager_);

    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    test_helper_->SetupDummyApps();
    g_browser_process->platform_part()
        ->InitializeSchedulerConfigurationManager();
  }

  void TearDown() override {
    g_browser_process->platform_part()->ShutdownSchedulerConfigurationManager();
    test_helper_.reset();
    run_loop_.reset();
    profile_.reset();
    browser_part_.ShutdownCrosComponentManager();
    component_manager_.reset();
    chromeos::DlcserviceClient::Shutdown();
  }

 protected:
  chromeos::FakeConciergeClient* fake_concierge_client_;

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;
  std::string app_id_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;
};

TEST_F(CrostiniUtilTest, ContainerIdEquality) {
  auto container1 = ContainerId{"test1", "test2"};
  auto container2 = ContainerId{"test1", "test2"};
  auto container3 = ContainerId{"test2", "test1"};

  ASSERT_TRUE(container1 == container2);
  ASSERT_FALSE(container1 == container3);
  ASSERT_FALSE(container2 == container3);
}

TEST_F(CrostiniUtilTest, LaunchCallbackRunsOnRestartError) {
  // Set Restart to fail.
  fake_concierge_client_->set_start_vm_response({});

  // Launch should fail and invoke callback.
  LaunchCrostiniApp(
      profile_.get(), app_id_, kDisplayId, {},
      base::BindOnce(&CrostiniUtilTest::SuccessCallback, base::Unretained(this),
                     false,
                     "crostini restart to launch app "
                     "pfdnkhehloenlegacemoalhjljmpllpc failed: 5"));

  run_loop_->Run();
}

}  // namespace crostini
