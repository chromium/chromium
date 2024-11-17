// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_installer.h"

#include <memory>
#include <ratio>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace borealis {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using InstallingState = BorealisInstaller::InstallingState;
using borealis::mojom::InstallResult;

class MockObserver : public BorealisInstaller::Observer {
 public:
  MOCK_METHOD1(OnProgressUpdated, void(double));
  MOCK_METHOD1(OnStateUpdated, void(InstallingState));
  MOCK_METHOD2(OnInstallationEnded, void(InstallResult, const std::string&));
  MOCK_METHOD0(OnCancelInitiated, void());
};

class BorealisInstallerTest : public testing::Test,
                              protected guest_os::FakeVmServicesHelper {
 public:
  BorealisInstallerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~BorealisInstallerTest() override = default;

  // Disallow copy and assign.
  BorealisInstallerTest(const BorealisInstallerTest&) = delete;
  BorealisInstallerTest& operator=(const BorealisInstallerTest&) = delete;

 protected:
  BorealisInstaller* installer() {
    return &BorealisServiceFactory::GetForProfile(&profile_)->Installer();
  }

  void SetUp() override {
    // TODO(b/293370103): Remove this when we remove the legacy disk management.
    if (!ash::SpacedClient::Get()) {
      ash::SpacedClient::InitializeFake();
      static_cast<ash::FakeSpacedClient*>(ash::SpacedClient::Get())
          ->set_free_disk_space(100 * std::giga::num);
    }

    scoped_allowance_ =
        std::make_unique<ScopedAllowBorealis>(&profile_, /*also_enable=*/false);

    FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);
    guest_os::GuestId id{guest_os::VmType::BOREALIS, "borealis", "penguin"};
    guest_os::GuestOsSessionTrackerFactory::GetForProfile(&profile_)
        ->AddGuestForTesting(id);

    vm_tools::concierge::ListVmDisksResponse resp;
    vm_tools::concierge::VmDiskInfo* img = resp.add_images();
    img->set_name("borealis");
    img->set_user_chosen_size(false);
    resp.set_success(true);
    FakeConciergeClient()->set_list_vm_disks_response(resp);

    // Adding the steam app this early is somewhat unrealistic, but sufficient
    // for testing.
    //
    // A better place would be some time after the StartVm() rpc returns.
    CreateFakeMainApp(&profile_);

    ASSERT_FALSE(BorealisDlcInstalled());
    ASSERT_FALSE(BorealisServiceFactory::GetForProfile(&profile_)
                     ->Features()
                     .IsEnabled());
  }

  void StartAndRunToCompletion() {
    installer()->Start();
    task_environment_.RunUntilIdle();
  }

  bool BorealisDlcInstalled() {
    base::RunLoop run_loop;
    bool installed = false;
    FakeDlcserviceClient()->GetExistingDlcs(base::BindLambdaForTesting(
        [&](std::string_view err,
            const dlcservice::DlcsWithContent& dlcs_with_content) {
          for (const auto& dlc : dlcs_with_content.dlc_infos()) {
            if (dlc.id() == kBorealisDlcName) {
              installed = true;
              break;
            }
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return installed;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  TestingProfile profile_;
  std::unique_ptr<ScopedAllowBorealis> scoped_allowance_;
};

class BorealisInstallerTestDlc : public BorealisInstallerTest,
                                 public testing::WithParamInterface<
                                     std::pair<std::string, InstallResult>> {};

TEST_F(BorealisInstallerTest, BorealisNotAllowed) {
  scoped_allowance_.reset();

  StartAndRunToCompletion();

  EXPECT_FALSE(BorealisDlcInstalled());
  EXPECT_FALSE(
      BorealisServiceFactory::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, DeviceOfflineInstallationFails) {
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker =
          network::TestNetworkConnectionTracker::CreateInstance();
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  StartAndRunToCompletion();

  EXPECT_FALSE(BorealisDlcInstalled());
  EXPECT_FALSE(
      BorealisServiceFactory::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, SucessfulInstallation) {
  StartAndRunToCompletion();

  EXPECT_TRUE(BorealisDlcInstalled());
  EXPECT_TRUE(
      BorealisServiceFactory::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, InstallationObserver) {
  testing::StrictMock<MockObserver> observer;
  installer()->AddObserver(&observer);

  EXPECT_CALL(observer, OnStateUpdated(InstallingState::kCheckingIfAllowed));
  EXPECT_CALL(observer, OnStateUpdated(InstallingState::kInstallingDlc));
  EXPECT_CALL(observer, OnStateUpdated(InstallingState::kStartingUp));
  EXPECT_CALL(observer, OnStateUpdated(InstallingState::kAwaitingApplications));
  EXPECT_CALL(observer, OnProgressUpdated(_)).Times(testing::AtLeast(1));
  EXPECT_CALL(observer, OnInstallationEnded(InstallResult::kSuccess, ""));

  StartAndRunToCompletion();
}

TEST_F(BorealisInstallerTest, CancelledInstallation) {
  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);

  EXPECT_CALL(observer, OnCancelInitiated());
  EXPECT_CALL(observer,
              OnInstallationEnded(InstallResult::kCancelled, testing::Not("")));

  installer()->Start();
  installer()->Cancel();
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisInstallerTest, InstallationInProgess) {
  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);

  EXPECT_CALL(observer,
              OnInstallationEnded(InstallResult::kBorealisInstallInProgress,
                                  testing::Not("")));
  EXPECT_CALL(observer, OnInstallationEnded(InstallResult::kSuccess, ""));

  installer()->Start();
  installer()->Start();
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisInstallerTest, CancelledThenSuccessfulInstallation) {
  installer()->Cancel();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(BorealisDlcInstalled());
  EXPECT_FALSE(
      BorealisServiceFactory::GetForProfile(&profile_)->Features().IsEnabled());

  installer()->Start();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(BorealisDlcInstalled());
  EXPECT_TRUE(
      BorealisServiceFactory::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, SucessfulInstallationRecordMetrics) {
  StartAndRunToCompletion();

  histogram_tester_.ExpectTotalCount(kBorealisInstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisInstallResultHistogram,
                                       InstallResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kBorealisInstallOverallTimeHistogram, 1);
}

TEST_F(BorealisInstallerTest, IncompleteInstallationRecordMetrics) {
  // This error is arbitrarily chosen for simplicity.
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorAllocation);

  StartAndRunToCompletion();

  histogram_tester_.ExpectTotalCount(kBorealisInstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisInstallResultHistogram,
                                       InstallResult::kDlcNeedSpaceError, 1);
  histogram_tester_.ExpectTotalCount(kBorealisInstallOverallTimeHistogram, 0);
}

TEST_F(BorealisInstallerTest, ReportsStartupFailureAsError) {
  vm_tools::concierge::StartVmResponse resp;
  resp.set_success(false);
  resp.set_failure_reason("ABC123");
  FakeConciergeClient()->set_start_vm_response(resp);

  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);
  EXPECT_CALL(observer, OnInstallationEnded(InstallResult::kStartupFailed,
                                            testing::HasSubstr("ABC123")));

  StartAndRunToCompletion();
}

TEST_F(BorealisInstallerTest, ReportsMainAppMissingAsError) {
  // Remove the steam client app, which the framework made for us
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(&profile_)
      ->ClearApplicationList(guest_os::VmType::BOREALIS, "borealis", "penguin");

  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);

  StartAndRunToCompletion();

  EXPECT_CALL(observer, OnInstallationEnded(InstallResult::kMainAppNotPresent,
                                            testing::Not("")));
  task_environment_.FastForwardBy(base::Seconds(6));
}

// Note that we don't check if the DLC has/hasn't been installed, since the
// mocked DLC service will always succeed, so we only care about how the error
// code returned by the service is handled by the installer.
TEST_P(BorealisInstallerTestDlc, DlcError) {
  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);
  FakeDlcserviceClient()->set_install_error(GetParam().first);

  EXPECT_CALL(observer,
              OnInstallationEnded(GetParam().second, testing::Not("")));

  StartAndRunToCompletion();
}

INSTANTIATE_TEST_SUITE_P(
    BorealisInstallerTestDlcErrors,
    BorealisInstallerTestDlc,
    testing::Values(std::pair<std::string, InstallResult>(
                        dlcservice::kErrorInvalidDlc,
                        InstallResult::kDlcUnsupportedError),
                    std::pair<std::string, InstallResult>(
                        dlcservice::kErrorNeedReboot,
                        InstallResult::kDlcNeedRebootError),
                    std::pair<std::string, InstallResult>(
                        dlcservice::kErrorAllocation,
                        InstallResult::kDlcNeedSpaceError),
                    std::pair<std::string, InstallResult>(
                        dlcservice::kErrorNoImageFound,
                        InstallResult::kDlcNeedUpdateError),
                    std::pair<std::string, InstallResult>(
                        "unknown",
                        InstallResult::kDlcUnknownError)));

class BorealisUninstallerTest : public BorealisInstallerTest {
 public:
  void SetUp() override {
    BorealisInstallerTest::SetUp();

    // Install borealis.
    StartAndRunToCompletion();
    ASSERT_TRUE(BorealisServiceFactory::GetForProfile(&profile_)
                    ->Features()
                    .IsEnabled());
  }
};

using CallbackFactory = StrictCallbackFactory<void(BorealisUninstallResult)>;

TEST_F(BorealisUninstallerTest, ErrorIfUninstallIsAlreadyInProgress) {
  CallbackFactory callback_factory;

  EXPECT_CALL(callback_factory,
              Call(BorealisUninstallResult::kAlreadyInProgress))
      .Times(1);

  installer()->Uninstall(callback_factory.BindOnce());
  installer()->Uninstall(callback_factory.BindOnce());
}

TEST_F(BorealisUninstallerTest, ErrorIfShutdownFails) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisUninstallResult::kShutdownFailed));

  FakeConciergeClient()->set_stop_vm_response(std::nullopt);

  installer()->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  // Shutdown failed, so borealis's disk will still be there.
  EXPECT_EQ(FakeConciergeClient()->destroy_disk_image_call_count(), 0);

  // Borealis is still "installed" according to the prefs.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice));
}

TEST_F(BorealisUninstallerTest, ErrorIfDiskNotRemoved) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory,
              Call(BorealisUninstallResult::kRemoveDiskFailed));

  FakeConciergeClient()->set_destroy_disk_image_response(std::nullopt);

  installer()->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  // The DLC should remain because the disk was not removed.
  EXPECT_TRUE(BorealisDlcInstalled());

  // Borealis is still "installed" according to the prefs.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice));
}

TEST_F(BorealisUninstallerTest, ErrorIfDlcNotRemoved) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory,
              Call(BorealisUninstallResult::kRemoveDlcFailed));

  FakeDlcserviceClient()->set_uninstall_error("some failure");

  installer()->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  // Borealis is still "installed" according to the prefs.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice));
}

TEST_F(BorealisUninstallerTest, UninstallationRemovesAllNecessaryPieces) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisUninstallResult::kSuccess));

  // Install a fake app.
  CreateFakeApp(&profile_, "test.desktop", "test exec");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(guest_os::GuestOsRegistryServiceFactory::GetForProfile(&profile_)
                ->GetRegisteredApps(vm_tools::apps::BOREALIS)
                .size(),
            1u);

  installer()->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  // Borealis is not running.
  EXPECT_FALSE(BorealisServiceFactory::GetForProfile(&profile_)
                   ->ContextManager()
                   .IsRunning());

  // Borealis is not enabled.
  EXPECT_FALSE(
      BorealisServiceFactory::GetForProfile(&profile_)->Features().IsEnabled());

  // Borealis has no installed apps.
  EXPECT_EQ(guest_os::GuestOsRegistryServiceFactory::GetForProfile(&profile_)
                ->GetRegisteredApps(vm_tools::apps::BOREALIS)
                .size(),
            0u);

  // Borealis has no stateful disk.
  EXPECT_GE(FakeConciergeClient()->destroy_disk_image_call_count(), 1);

  // Borealis's DLC is not installed
  EXPECT_FALSE(BorealisDlcInstalled());
}

TEST_F(BorealisUninstallerTest, UninstallationIsIdempotent) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisUninstallResult::kSuccess))
      .Times(2);

  installer()->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  installer()->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisUninstallerTest, SuccessfulUninstallationRecordsMetrics) {
  installer()->Uninstall(base::DoNothing());
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kBorealisUninstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisUninstallResultHistogram,
                                       BorealisUninstallResult::kSuccess, 1);
}

TEST_F(BorealisUninstallerTest, FailedUninstallationRecordsMetrics) {
  // Fail via shutdown, as that is the first step.
  FakeConciergeClient()->set_stop_vm_response(std::nullopt);

  installer()->Uninstall(base::DoNothing());
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kBorealisUninstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisUninstallResultHistogram,
                                       BorealisUninstallResult::kShutdownFailed,
                                       1);
}

}  // namespace
}  // namespace borealis
