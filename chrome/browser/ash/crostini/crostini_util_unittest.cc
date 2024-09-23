// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_util.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kDesktopFileId[] = "dummy1";
constexpr int kDisplayId = 0;

namespace crostini {

class CrostiniUtilTest : public testing::Test {
 public:
  CrostiniUtilTest()
      : app_id_(crostini::CrostiniTestHelper::GenerateAppId(kDesktopFileId)),
        task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        browser_part_(g_browser_process->platform_part()) {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::DebugDaemonClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();

    fake_concierge_client_ = ash::FakeConciergeClient::Get();
  }

  ~CrostiniUtilTest() override {
    ash::SeneschalClient::Shutdown();
    ash::DebugDaemonClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  CrostiniUtilTest(const CrostiniUtilTest&) = delete;
  CrostiniUtilTest& operator=(const CrostiniUtilTest&) = delete;

  void SetUp() override {
    ash::DlcserviceClient::InitializeFake();

    component_manager_ =
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    component_manager_->set_supported_components({"cros-termina"});
    component_manager_->ResetComponentState(
        "cros-termina",
        component_updater::FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path")));
    browser_part_.InitializeComponentManager(component_manager_);

    profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    test_helper_->SetupDummyApps();
    g_browser_process->platform_part()
        ->InitializeSchedulerConfigurationManager();
  }

  void TearDown() override {
    g_browser_process->platform_part()->ShutdownSchedulerConfigurationManager();
    test_helper_.reset();
    profile_.reset();
    browser_part_.ShutdownComponentManager();
    component_manager_.reset();
    ash::DlcserviceClient::Shutdown();
  }

 protected:
  raw_ptr<ash::FakeConciergeClient, DanglingUntriaged> fake_concierge_client_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;
  std::string app_id_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  scoped_refptr<component_updater::FakeComponentManagerAsh> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;
};

TEST_F(CrostiniUtilTest, LaunchCallbackRunsOnRestartError) {
  // Set Restart to fail.
  fake_concierge_client_->set_start_vm_response({});

  base::test::TestFuture<bool, const std::string&> result_future;
  // Launch should fail and invoke callback.
  LaunchCrostiniApp(profile_.get(), app_id_, kDisplayId, {},
                    result_future.GetCallback());
  EXPECT_FALSE(result_future.Get<0>());
  EXPECT_EQ(
      "crostini restart to launch app "
      "pfdnkhehloenlegacemoalhjljmpllpc failed: 5",
      result_future.Get<1>());
}

TEST_F(CrostiniUtilTest, ShouldStopVm) {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_.get());
  guest_os::GuestId containera(kCrostiniDefaultVmType, "apple", "banana");
  guest_os::GuestId containerb(kCrostiniDefaultVmType, "potato", "strawberry");
  base::Value::List containers;
  containers.Append(containera.ToDictValue().Clone());
  containers.Append(containerb.ToDictValue().Clone());

  manager->AddRunningVmForTesting("apple");
  manager->AddRunningVmForTesting("potato");
  manager->AddRunningContainerForTesting(
      "apple", ContainerInfo("banana", "bo", "home", "1.2.3.4"));
  manager->AddRunningContainerForTesting(
      "potato", ContainerInfo("strawberry", "bo", "home", "1.2.3.4"));

  ASSERT_TRUE(manager->IsVmRunning("apple"));
  ASSERT_TRUE(manager->IsVmRunning("potato"));

  profile_->GetPrefs()->SetList(guest_os::prefs::kGuestOsContainers,
                                std::move(containers));

  EXPECT_TRUE(ShouldStopVm(profile_.get(), containera));
}

TEST_F(CrostiniUtilTest, ShouldNotStopVm) {
  CrostiniManager* manager = CrostiniManager::GetForProfile(profile_.get());
  guest_os::GuestId containera(kCrostiniDefaultVmType, "apple", "banana");
  guest_os::GuestId containerb(kCrostiniDefaultVmType, "apple", "strawberry");
  base::Value::List containers;
  containers.Append(containera.ToDictValue().Clone());
  containers.Append(containerb.ToDictValue().Clone());

  manager->AddRunningVmForTesting("apple");
  manager->AddRunningContainerForTesting(
      "apple", ContainerInfo("banana", "bo", "home", "1.2.3.4"));
  manager->AddRunningContainerForTesting(
      "apple", ContainerInfo("strawberry", "bo", "home", "1.2.3.4"));

  ASSERT_TRUE(manager->IsVmRunning("apple"));

  profile_->GetPrefs()->SetList(guest_os::prefs::kGuestOsContainers,
                                std::move(containers));

  EXPECT_FALSE(ShouldStopVm(profile_.get(), containera));
}

}  // namespace crostini
