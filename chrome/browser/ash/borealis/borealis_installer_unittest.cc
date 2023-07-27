// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_installer_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
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
  BorealisInstallerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~BorealisInstallerTest() override = default;

  // Disallow copy and assign.
  BorealisInstallerTest(const BorealisInstallerTest&) = delete;
  BorealisInstallerTest& operator=(const BorealisInstallerTest&) = delete;

 protected:
  BorealisInstaller* installer() { return installer_.get(); }

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

    installer_ = std::make_unique<BorealisInstallerImpl>(&profile_);

    ASSERT_FALSE(BorealisDlcInstalled());
    ASSERT_FALSE(
        BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
  }

  void TearDown() override {
    ctx_.reset();
    installer_.reset();
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

  bool BorealisDlcInstalled() {
    base::RunLoop run_loop;
    bool installed = false;
    FakeDlcserviceClient()->GetExistingDlcs(base::BindLambdaForTesting(
        [&](const std::string& err,
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
  std::unique_ptr<BorealisContext> ctx_;
  std::unique_ptr<BorealisFeatures> test_features_;
  std::unique_ptr<BorealisContextManagerMock> test_context_manager_;
  std::unique_ptr<BorealisWindowManager> test_window_manager_;
  std::unique_ptr<BorealisDiskManagerDispatcher> test_disk_dispatcher_;
  raw_ptr<BorealisServiceFake, ExperimentalAsh> fake_service_;
  std::unique_ptr<ScopedAllowBorealis> scoped_allowance_;

 private:
  std::unique_ptr<BorealisInstaller> installer_;
};

class BorealisInstallerTestDlc
    : public BorealisInstallerTest,
      public testing::WithParamInterface<
          std::pair<std::string, BorealisInstallResult>> {};

TEST_F(BorealisInstallerTest, BorealisNotAllowed) {
  scoped_allowance_.reset();

  StartAndRunToCompletion();

  EXPECT_FALSE(BorealisDlcInstalled());
  EXPECT_FALSE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
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
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, SucessfulInstallation) {
  PrepareSuccessfulInstallation();

  StartAndRunToCompletion();

  EXPECT_TRUE(BorealisDlcInstalled());
  EXPECT_TRUE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, HandlesMainAppPreExisting) {
  // Normally we add the main app after signaling completion, which this a
  // better way of modeling how garcon works. In this test we add the main app
  // well before, to simulate when garcon actually wins the race.
  CreateFakeMainApp(&profile_);
  PrepareSuccessfulInstallation();

  StartAndRunToCompletion();
  EXPECT_TRUE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, InstallationObserver) {
  testing::StrictMock<MockObserver> observer;
  installer()->AddObserver(&observer);
  PrepareSuccessfulInstallation();

  EXPECT_CALL(observer, OnStateUpdated(InstallingState::kCheckingIfAllowed));
  EXPECT_CALL(observer, OnStateUpdated(InstallingState::kInstallingDlc));
  EXPECT_CALL(observer, OnStateUpdated(InstallingState::kStartingUp));
  EXPECT_CALL(observer, OnStateUpdated(InstallingState::kAwaitingApplications));
  EXPECT_CALL(observer, OnProgressUpdated(_)).Times(testing::AtLeast(1));
  EXPECT_CALL(observer,
              OnInstallationEnded(BorealisInstallResult::kSuccess, ""));

  StartAndRunToCompletion();
}

TEST_F(BorealisInstallerTest, CancelledInstallation) {
  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);

  EXPECT_CALL(observer, OnCancelInitiated());
  EXPECT_CALL(observer, OnInstallationEnded(BorealisInstallResult::kCancelled,
                                            testing::Not("")));

  installer()->Start();
  installer()->Cancel();
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisInstallerTest, InstallationInProgess) {
  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);
  PrepareSuccessfulInstallation();

  EXPECT_CALL(observer, OnInstallationEnded(
                            BorealisInstallResult::kBorealisInstallInProgress,
                            testing::Not("")));
  EXPECT_CALL(observer,
              OnInstallationEnded(BorealisInstallResult::kSuccess, ""));

  installer()->Start();
  installer()->Start();
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisInstallerTest, CancelledThenSuccessfulInstallation) {
  PrepareSuccessfulInstallation();

  installer()->Cancel();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(BorealisDlcInstalled());
  EXPECT_FALSE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());

  installer()->Start();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(BorealisDlcInstalled());
  EXPECT_TRUE(
      BorealisService::GetForProfile(&profile_)->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, SucessfulInstallationRecordMetrics) {
  PrepareSuccessfulInstallation();

  StartAndRunToCompletion();

  histogram_tester_.ExpectTotalCount(kBorealisInstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisInstallResultHistogram,
                                       BorealisInstallResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kBorealisInstallOverallTimeHistogram, 1);
}

TEST_F(BorealisInstallerTest, IncompleteInstallationRecordMetrics) {
  // This error is arbitrarily chosen for simplicity.
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorAllocation);

  StartAndRunToCompletion();

  histogram_tester_.ExpectTotalCount(kBorealisInstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kBorealisInstallResultHistogram,
      BorealisInstallResult::kDlcNeedSpaceError, 1);
  histogram_tester_.ExpectTotalCount(kBorealisInstallOverallTimeHistogram, 0);
}

TEST_F(BorealisInstallerTest, ReportsStartupFailureAsError) {
  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);
  EXPECT_CALL(*test_context_manager_, StartBorealis)
      .WillOnce(
          testing::Invoke([](BorealisContextManager::ResultCallback callback) {
            std::move(callback).Run(
                base::unexpected(Described<BorealisStartupResult>{
                    BorealisStartupResult::kStartVmFailed, "Some Error"}));
          }));

  EXPECT_CALL(observer,
              OnInstallationEnded(BorealisInstallResult::kStartupFailed,
                                  testing::HasSubstr("Some Error")));

  StartAndRunToCompletion();
}

TEST_F(BorealisInstallerTest, ReportsMainAppMissingAsError) {
  testing::NiceMock<MockObserver> observer;
  installer()->AddObserver(&observer);
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);
  ctx_ = BorealisContext::CreateBorealisContextForTesting(&profile_);
  EXPECT_CALL(*test_context_manager_, StartBorealis)
      .WillOnce(testing::Invoke(
          [this](BorealisContextManager::ResultCallback callback) {
            std::move(callback).Run(
                BorealisContextManager::ContextOrFailure(ctx_.get()));
          }));

  StartAndRunToCompletion();

  EXPECT_CALL(observer,
              OnInstallationEnded(BorealisInstallResult::kMainAppNotPresent,
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
    testing::Values(std::pair<std::string, BorealisInstallResult>(
                        dlcservice::kErrorInvalidDlc,
                        BorealisInstallResult::kDlcUnsupportedError),
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

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kFailed);
          }));

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

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  FakeConciergeClient()->set_destroy_disk_image_response(absl::nullopt);

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

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
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

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  installer()->Uninstall(callback_factory.BindOnce());
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
  EXPECT_FALSE(BorealisDlcInstalled());
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
  installer()->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  installer()->Uninstall(callback_factory.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisUninstallerTest, SuccessfulUninstallationRecordsMetrics) {
  EXPECT_CALL(*test_context_manager_, ShutDownBorealis(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisShutdownResult)> callback) {
            std::move(callback).Run(BorealisShutdownResult::kSuccess);
          }));
  installer()->Uninstall(base::DoNothing());
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

  installer()->Uninstall(base::DoNothing());
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kBorealisUninstallNumAttemptsHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kBorealisUninstallResultHistogram,
                                       BorealisUninstallResult::kShutdownFailed,
                                       1);
}

}  // namespace
}  // namespace borealis
