// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_installer_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_context_manager_mock.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/infra/described.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
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
using BorealisInstallResult = BorealisInstallResult;

class MockObserver : public BorealisInstaller::Observer {
 public:
  MOCK_METHOD1(OnProgressUpdated, void(double));
  MOCK_METHOD1(OnStateUpdated, void(InstallingState));
  MOCK_METHOD2(OnInstallationEnded,
               void(BorealisInstallResult, const std::string&));
  MOCK_METHOD0(OnCancelInitiated, void());
};

class BorealisInstallerTest : public testing::Test,
                              protected guest_os::FakeVmServicesHelper {
 public:
  BorealisInstallerTest() = default;
  ~BorealisInstallerTest() override = default;

  // Disallow copy and assign.
  BorealisInstallerTest(const BorealisInstallerTest&) = delete;
  BorealisInstallerTest& operator=(const BorealisInstallerTest&) = delete;

 protected:
  void SetUp() override {
    test_features_ = std::make_unique<BorealisFeatures>(&profile_);
    test_context_manager_ =
        std::make_unique<NiceMock<BorealisContextManagerMock>>();
    test_window_manager_ = std::make_unique<BorealisWindowManager>(&profile_);
    test_disk_dispatcher_ = std::make_unique<BorealisDiskManagerDispatcher>();
    fake_service_ = BorealisServiceFake::UseFakeForTesting(&profile_);
    fake_service_->SetFeaturesForTesting(test_features_.get());
    fake_service_->SetContextManagerForTesting(test_context_manager_.get());
    fake_service_->SetWindowManagerForTesting(test_window_manager_.get());
    fake_service_->SetDiskManagerDispatcherForTesting(
        test_disk_dispatcher_.get());

    scoped_allowance_ =
        std::make_unique<ScopedAllowBorealis>(&profile_, /*also_enable=*/false);

    installer_impl_ = std::make_unique<BorealisInstallerImpl>(&profile_);
    installer_ = installer_impl_.get();
    observer_ = std::make_unique<NiceMock<MockObserver>>();
    installer_->AddObserver(observer_.get());

    UpdateCurrentDlcs();
    ASSERT_EQ(current_dlcs_.dlc_infos_size(), 0);
    ASSERT_FALSE(
        BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
  }

  void TearDown() override {
    ctx_.reset();
    observer_.reset();
    installer_impl_.reset();
  }

  void StartAndRunToCompletion() {
    installer_->Start();
    task_environment_.RunUntilIdle();
  }

  void PrepareSuccessfulInstallation() {
    DCHECK(scoped_allowance_);
    FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);
    ctx_ = BorealisContext::CreateBorealisContextForTesting(&profile_);
    ctx_->set_vm_name("borealis");
    ctx_->set_container_name("penguin");
    EXPECT_CALL(*test_context_manager_, StartBorealis)
        .WillOnce(testing::Invoke(
            [this](BorealisContextManager::ResultCallback callback) {
              std::move(callback).Run(
                  BorealisContextManager::ContextOrFailure(ctx_.get()));
              // Make a fake main app. We do this inside the callback as it is a
              // better way to simulate garcon's callback.
              CreateFakeMainApp(&profile_);
            }));
  }

  void UpdateCurrentDlcs() {
    base::RunLoop run_loop;
    FakeDlcserviceClient()->GetExistingDlcs(base::BindOnce(
        [](dlcservice::DlcsWithContent* out, base::OnceClosure quit,
           const std::string& err,
           const dlcservice::DlcsWithContent& dlcs_with_content) {
          out->CopyFrom(dlcs_with_content);
          std::move(quit).Run();
        },
        base::Unretained(&current_dlcs_), run_loop.QuitClosure()));
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  TestingProfile profile_;
  std::unique_ptr<BorealisContext> ctx_;
  std::unique_ptr<BorealisFeatures> test_features_;
  std::unique_ptr<BorealisContextManagerMock> test_context_manager_;
  std::unique_ptr<BorealisWindowManager> test_window_manager_;
  std::unique_ptr<BorealisDiskManagerDispatcher> test_disk_dispatcher_;
  BorealisServiceFake* fake_service_;
  std::unique_ptr<ScopedAllowBorealis> scoped_allowance_;
  std::unique_ptr<BorealisInstallerImpl> installer_impl_;
  BorealisInstaller* installer_;
  std::unique_ptr<MockObserver> observer_;
  dlcservice::DlcsWithContent current_dlcs_;
};

class BorealisInstallerTestDlc
    : public BorealisInstallerTest,
      public testing::WithParamInterface<
          std::pair<std::string, BorealisInstallResult>> {};

TEST_F(BorealisInstallerTest, BorealisNotAllowed) {
  scoped_allowance_.reset();

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kBorealisNotAllowed,
                                  testing::Not("")));

  StartAndRunToCompletion();
  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 0);
  EXPECT_FALSE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, DeviceOfflineInstallationFails) {
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker =
          network::TestNetworkConnectionTracker::CreateInstance();
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  EXPECT_CALL(*observer_, OnInstallationEnded(BorealisInstallResult::kOffline,
                                              testing::Not("")));

  StartAndRunToCompletion();
  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 0);
  EXPECT_FALSE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, SucessfulInstallation) {
  PrepareSuccessfulInstallation();

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kSuccess, ""));
  StartAndRunToCompletion();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 1);
  EXPECT_EQ(current_dlcs_.dlc_infos(0).id(), borealis::kBorealisDlcName);
  EXPECT_TRUE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, HandlesMainAppPreExisting) {
  PrepareSuccessfulInstallation();

  // Normally we add the main app after signaling completion, which this a
  // better way of modeling how garcon works. In this test we add the main app
  // well before, to simulate when garcon actually wins the race.
  CreateFakeMainApp(&profile_);

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kSuccess, ""));
  StartAndRunToCompletion();
}

TEST_F(BorealisInstallerTest, InstallationHasAllStages) {
  PrepareSuccessfulInstallation();

  EXPECT_CALL(*observer_, OnStateUpdated(InstallingState::kCheckingIfAllowed));
  EXPECT_CALL(*observer_, OnStateUpdated(InstallingState::kInstallingDlc));
  EXPECT_CALL(*observer_, OnStateUpdated(InstallingState::kStartingUp));
  EXPECT_CALL(*observer_,
              OnStateUpdated(InstallingState::kAwaitingApplications));

  StartAndRunToCompletion();
}

TEST_F(BorealisInstallerTest, CancelledInstallation) {
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);

  EXPECT_CALL(*observer_, OnCancelInitiated());
  EXPECT_CALL(*observer_, OnInstallationEnded(BorealisInstallResult::kCancelled,
                                              testing::Not("")));

  installer_->Start();
  installer_->Cancel();
  task_environment_.RunUntilIdle();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 1);
  EXPECT_EQ(current_dlcs_.dlc_infos(0).id(), borealis::kBorealisDlcName);
  EXPECT_FALSE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, InstallationInProgess) {
  PrepareSuccessfulInstallation();

  EXPECT_CALL(*observer_, OnInstallationEnded(
                              BorealisInstallResult::kBorealisInstallInProgress,
                              testing::Not("")));
  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kSuccess, ""));

  installer_->Start();
  installer_->Start();
  task_environment_.RunUntilIdle();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 1);
  EXPECT_EQ(current_dlcs_.dlc_infos(0).id(), borealis::kBorealisDlcName);
  EXPECT_TRUE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, CancelledThenSuccessfulInstallation) {
  PrepareSuccessfulInstallation();

  EXPECT_CALL(*observer_, OnCancelInitiated());

  installer_->Cancel();
  task_environment_.RunUntilIdle();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 0);
  EXPECT_FALSE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kSuccess, ""));

  installer_->Start();
  task_environment_.RunUntilIdle();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 1);
  EXPECT_EQ(current_dlcs_.dlc_infos(0).id(), borealis::kBorealisDlcName);
  EXPECT_TRUE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, SucessfulInstallationRecordMetrics) {
  PrepareSuccessfulInstallation();

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kSuccess, ""));
  StartAndRunToCompletion();

  histogram_tester_.ExpectTotalCount(kBorealisInstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisInstallResultHistogram,
                                       BorealisInstallResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kBorealisInstallOverallTimeHistogram, 1);
}

TEST_F(BorealisInstallerTest, IncompleteInstallationRecordMetrics) {
  // This error is arbitrarily chosen for simplicity.
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorInternal);

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kDlcInternalError,
                                  testing::Not("")));
  StartAndRunToCompletion();

  histogram_tester_.ExpectTotalCount(kBorealisInstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisInstallResultHistogram,
                                       BorealisInstallResult::kDlcInternalError,
                                       1);
  histogram_tester_.ExpectTotalCount(kBorealisInstallOverallTimeHistogram, 0);
}

TEST_F(BorealisInstallerTest, ReportsStartupFailureAsError) {
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);
  EXPECT_CALL(*test_context_manager_, StartBorealis)
      .WillOnce(
          testing::Invoke([](BorealisContextManager::ResultCallback callback) {
            std::move(callback).Run(
                BorealisContextManager::ContextOrFailure::Unexpected(
                    Described<BorealisStartupResult>{
                        BorealisStartupResult::kStartVmFailed, "Some Error"}));
          }));

  EXPECT_CALL(*observer_, OnStateUpdated(InstallingState::kCheckingIfAllowed));
  EXPECT_CALL(*observer_, OnStateUpdated(InstallingState::kInstallingDlc));
  EXPECT_CALL(*observer_, OnStateUpdated(InstallingState::kStartingUp));
  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kStartupFailed,
                                  testing::HasSubstr("Some Error")));

  StartAndRunToCompletion();
}

TEST_F(BorealisInstallerTest, ReportsMainAppMissingAsError) {
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);
  ctx_ = BorealisContext::CreateBorealisContextForTesting(&profile_);
  EXPECT_CALL(*test_context_manager_, StartBorealis)
      .WillOnce(testing::Invoke(
          [this](BorealisContextManager::ResultCallback callback) {
            std::move(callback).Run(
                BorealisContextManager::ContextOrFailure(ctx_.get()));
          }));

  // Set a zero timeout otherwise the in-progress timeout gets cleaned up.
  installer_impl_->SetMainAppTimeoutForTesting(base::Seconds(0));

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kMainAppNotPresent,
                                  testing::Not("")));

  StartAndRunToCompletion();
}

TEST_F(BorealisInstallerTest, RetriesAfterInternalFailure) {
  PrepareSuccessfulInstallation();
  FakeDlcserviceClient()->set_install_errors({dlcservice::kErrorInternal,
                                              dlcservice::kErrorInternal,
                                              dlcservice::kErrorNone});

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kSuccess, ""));
  StartAndRunToCompletion();

  histogram_tester_.ExpectTotalCount(kBorealisInstallRetriesHistogram, 1);
  histogram_tester_.ExpectBucketCount(kBorealisInstallRetriesHistogram, 2, 1);
}

// Note that we don't check if the DLC has/hasn't been installed, since the
// mocked DLC service will always succeed, so we only care about how the error
// code returned by the service is handled by the installer.
TEST_P(BorealisInstallerTestDlc, DlcError) {
  FakeDlcserviceClient()->set_install_error(GetParam().first);

  EXPECT_CALL(*observer_, OnStateUpdated(InstallingState::kCheckingIfAllowed));
  EXPECT_CALL(*observer_, OnStateUpdated(InstallingState::kInstallingDlc));
  EXPECT_CALL(*observer_,
              OnInstallationEnded(GetParam().second, testing::Not("")));

  StartAndRunToCompletion();
}

INSTANTIATE_TEST_SUITE_P(
    BorealisInstallerTestDlcErrors,
    BorealisInstallerTestDlc,
    testing::Values(std::pair<std::string, BorealisInstallResult>(
                        dlcservice::kErrorInternal,
                        BorealisInstallResult::kDlcInternalError),
                    std::pair<std::string, BorealisInstallResult>(
                        dlcservice::kErrorInvalidDlc,
                        BorealisInstallResult::kDlcUnsupportedError),
                    std::pair<std::string, BorealisInstallResult>(
                        dlcservice::kErrorBusy,
                        BorealisInstallResult::kDlcBusyError),
                    std::pair<std::string, BorealisInstallResult>(
                        dlcservice::kErrorNeedReboot,
                        BorealisInstallResult::kDlcNeedRebootError),
                    std::pair<std::string, BorealisInstallResult>(
                        dlcservice::kErrorAllocation,
                        BorealisInstallResult::kDlcNeedSpaceError),
                    std::pair<std::string, BorealisInstallResult>(
                        dlcservice::kErrorNoImageFound,
                        BorealisInstallResult::kDlcNeedUpdateError),
                    std::pair<std::string, BorealisInstallResult>(
                        "unknown",
                        BorealisInstallResult::kDlcUnknownError)));

class BorealisUninstallerTest : public BorealisInstallerTest {
 public:
  void SetUp() override {
    BorealisInstallerTest::SetUp();
    // Install borealis.
    PrepareSuccessfulInstallation();
    StartAndRunToCompletion();
    ASSERT_TRUE(
        BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
  }

  // Sets up the registry with a single app. Returns its app id.
  std::string SetDummyApp(const std::string& desktop_file_id) {
    vm_tools::apps::ApplicationList list;
    list.set_vm_name("borealis");
    list.set_container_name("penguin");
    list.set_vm_type(vm_tools::apps::BOREALIS);
    vm_tools::apps::App* app = list.add_apps();
    app->set_desktop_file_id(desktop_file_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app->mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(desktop_file_id);
    app->set_no_display(false);
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(&profile_)
        ->UpdateApplicationList(list);
    return guest_os::GuestOsRegistryService::GenerateAppId(
        desktop_file_id, list.vm_name(), list.container_name());
  }

 protected:
  BorealisServiceFake* fake_service_ = nullptr;
};

using CallbackFactory = StrictCallbackFactory<void(BorealisUninstallResult)>;

TEST_F(BorealisUninstallerTest, ErrorIfUninstallIsAlreadyInProgress) {
  CallbackFactory callback_factory;

  EXPECT_CALL(callback_factory,
              Call(BorealisUninstallResult::kAlreadyInProgress))
      .Times(1);

  installer_->Uninstall(callback_factory.BindOnce());
  installer_->Uninstall(callback_factory.BindOnce());
}

TEST_F(BorealisUninstallerTest, ErrorIfShutdownFails) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisUninstallResult::kShutdownFailed));

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kFailed);
          }));

  installer_->Uninstall(callback_factory.BindOnce());
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

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  FakeConciergeClient()->set_destroy_disk_image_response(absl::nullopt);

  installer_->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  // The DLC should remain because the disk was not removed.
  UpdateCurrentDlcs();
  EXPECT_EQ(current_dlcs_.dlc_infos_size(), 1);

  // Borealis is still "installed" according to the prefs.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice));
}

TEST_F(BorealisUninstallerTest, ErrorIfDlcNotRemoved) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory,
              Call(BorealisUninstallResult::kRemoveDlcFailed));

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  FakeDlcserviceClient()->set_uninstall_error("some failure");

  installer_->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  // Borealis is still "installed" according to the prefs.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice));
}

TEST_F(BorealisUninstallerTest, UninstallationRemovesAllNecessaryPieces) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisUninstallResult::kSuccess));

  // Install a fake app.
  SetDummyApp("dummy.desktop");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(guest_os::GuestOsRegistryServiceFactory::GetForProfile(&profile_)
                ->GetRegisteredApps(vm_tools::apps::BOREALIS)
                .size(),
            1u);

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  installer_->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  // Borealis is not running.
  EXPECT_FALSE(
      BorealisService::GetForProfile(&profile_)->ContextManager().IsRunning());

  // Borealis is not enabled.
  EXPECT_FALSE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());

  // Borealis has no installed apps.
  EXPECT_EQ(guest_os::GuestOsRegistryServiceFactory::GetForProfile(&profile_)
                ->GetRegisteredApps(vm_tools::apps::BOREALIS)
                .size(),
            0u);

  // Borealis has no stateful disk.
  EXPECT_GE(FakeConciergeClient()->destroy_disk_image_call_count(), 1);

  // Borealis's DLC is not installed
  UpdateCurrentDlcs();
  EXPECT_EQ(current_dlcs_.dlc_infos_size(), 0);
}

TEST_F(BorealisUninstallerTest, UninstallationIsIdempotent) {
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisUninstallResult::kSuccess))
      .Times(2);

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  installer_->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  installer_->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisUninstallerTest, SuccessfulUninstallationRecordsMetrics) {
  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  installer_->Uninstall(base::DoNothing());
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kBorealisUninstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisUninstallResultHistogram,
                                       BorealisUninstallResult::kSuccess, 1);
}

TEST_F(BorealisUninstallerTest, FailedUninstallationRecordsMetrics) {
  // Fail via shutdown, as that is the first step.
  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kFailed);
          }));

  installer_->Uninstall(base::DoNothing());
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kBorealisUninstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisUninstallResultHistogram,
                                       BorealisUninstallResult::kShutdownFailed,
                                       1);
}

}  // namespace
}  // namespace borealis
