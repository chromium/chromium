// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_installer_impl.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/borealis/borealis_features.h"
#include "chrome/browser/chromeos/borealis/borealis_metrics.h"
#include "chrome/browser/chromeos/borealis/borealis_prefs.h"
#include "chrome/browser/chromeos/borealis/borealis_service.h"
#include "chrome/browser/chromeos/borealis/borealis_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {

namespace {}  // namespace

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;
using InstallingState = BorealisInstaller::InstallingState;
using BorealisInstallResult = BorealisInstallResult;

class MockObserver : public BorealisInstaller::Observer {
 public:
  MOCK_METHOD1(OnProgressUpdated, void(double));
  MOCK_METHOD1(OnStateUpdated, void(InstallingState));
  MOCK_METHOD1(OnInstallationEnded, void(BorealisInstallResult));
  MOCK_METHOD0(OnCancelInitiated, void());
};

class BorealisInstallerTest : public testing::Test {
 public:
  BorealisInstallerTest() = default;
  ~BorealisInstallerTest() override = default;

 protected:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    CreateProfile();

    installer_impl_ = std::make_unique<BorealisInstallerImpl>(profile_.get());
    installer_ = installer_impl_.get();
    observer_ = std::make_unique<StrictMock<MockObserver>>();
    installer_->AddObserver(observer_.get());

    chromeos::DlcserviceClient::InitializeFake();
    fake_dlcservice_client_ = static_cast<chromeos::FakeDlcserviceClient*>(
        chromeos::DlcserviceClient::Get());
    UpdateCurrentDlcs();
    ASSERT_EQ(current_dlcs_.dlc_infos_size(), 0);
    ASSERT_FALSE(
        BorealisService::GetForProfile(profile_.get())->Features().IsEnabled());
  }

  void TearDown() override {
    observer_.reset();
    profile_.reset();
    histogram_tester_.reset();

    chromeos::DBusThreadManager::Shutdown();
    chromeos::DlcserviceClient::Shutdown();
  }

  // Set expectations for observer events up to and including |end_state|.
  void ExpectObserverEventsUntil(InstallingState end_state) {
    InstallingState states[] = {
        InstallingState::kInstallingDlc,
    };

    for (InstallingState state : states) {
      EXPECT_CALL(*observer_, OnStateUpdated(state));
      if (state == end_state)
        return;
    }

    NOTREACHED();
  }

  void StartAndRunToCompletion() {
    installer_->Start();
    task_environment_.RunUntilIdle();
  }

  void UpdateCurrentDlcs() {
    base::RunLoop run_loop;
    fake_dlcservice_client_->GetExistingDlcs(base::BindOnce(
        [](dlcservice::DlcsWithContent* out, base::OnceClosure quit,
           const std::string& err,
           const dlcservice::DlcsWithContent& dlcs_with_content) {
          out->CopyFrom(dlcs_with_content);
          std::move(quit).Run();
        },
        base::Unretained(&current_dlcs_), run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<BorealisInstallerImpl> installer_impl_;
  BorealisInstaller* installer_;
  std::unique_ptr<MockObserver> observer_;
  content::BrowserTaskEnvironment task_environment_;
  dlcservice::DlcsWithContent current_dlcs_;
  base::test::ScopedFeatureList feature_list_;

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeDlcserviceClient* fake_dlcservice_client_;

 private:
  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("defaultprofile");
    profile_ = profile_builder.Build();
  }

  // Disallow copy and assign.
  BorealisInstallerTest(const BorealisInstallerTest&) = delete;
  BorealisInstallerTest& operator=(const BorealisInstallerTest&) = delete;
};

class BorealisInstallerTestDlc
    : public BorealisInstallerTest,
      public testing::WithParamInterface<
          std::pair<std::string, BorealisInstallResult>> {};

TEST_F(BorealisInstallerTest, BorealisNotAllowed) {
  feature_list_.InitAndDisableFeature(features::kBorealis);

  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kBorealisNotAllowed));

  StartAndRunToCompletion();
  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 0);
  EXPECT_FALSE(
      BorealisService::GetForProfile(profile_.get())->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, DeviceOfflineInstallationFails) {
  feature_list_.InitAndEnableFeature(features::kBorealis);
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker =
          network::TestNetworkConnectionTracker::CreateInstance();
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  EXPECT_CALL(*observer_, OnInstallationEnded(BorealisInstallResult::kOffline));

  StartAndRunToCompletion();
  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 0);
  EXPECT_FALSE(
      BorealisService::GetForProfile(profile_.get())->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, SucessfulInstallation) {
  feature_list_.InitAndEnableFeature(features::kBorealis);
  fake_dlcservice_client_->set_install_error(dlcservice::kErrorNone);

  ExpectObserverEventsUntil(InstallingState::kInstallingDlc);
  EXPECT_CALL(*observer_, OnInstallationEnded(BorealisInstallResult::kSuccess));

  StartAndRunToCompletion();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 1);
  EXPECT_EQ(current_dlcs_.dlc_infos(0).id(), borealis::kBorealisDlcName);
  EXPECT_TRUE(
      BorealisService::GetForProfile(profile_.get())->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, CancelledInstallation) {
  feature_list_.InitAndEnableFeature(features::kBorealis);
  fake_dlcservice_client_->set_install_error(dlcservice::kErrorNone);

  ExpectObserverEventsUntil(InstallingState::kInstallingDlc);
  EXPECT_CALL(*observer_, OnCancelInitiated());
  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kCancelled));

  installer_->Start();
  installer_->Cancel();
  task_environment_.RunUntilIdle();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 1);
  EXPECT_EQ(current_dlcs_.dlc_infos(0).id(), borealis::kBorealisDlcName);
  EXPECT_FALSE(
      BorealisService::GetForProfile(profile_.get())->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, InstallationInProgess) {
  feature_list_.InitAndEnableFeature(features::kBorealis);
  fake_dlcservice_client_->set_install_error(dlcservice::kErrorNone);

  ExpectObserverEventsUntil(InstallingState::kInstallingDlc);
  EXPECT_CALL(
      *observer_,
      OnInstallationEnded(BorealisInstallResult::kBorealisInstallInProgress));
  EXPECT_CALL(*observer_, OnInstallationEnded(BorealisInstallResult::kSuccess));

  installer_->Start();
  installer_->Start();
  task_environment_.RunUntilIdle();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 1);
  EXPECT_EQ(current_dlcs_.dlc_infos(0).id(), borealis::kBorealisDlcName);
  EXPECT_TRUE(
      BorealisService::GetForProfile(profile_.get())->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, CancelledThenSuccessfulInstallation) {
  feature_list_.InitAndEnableFeature(features::kBorealis);
  fake_dlcservice_client_->set_install_error(dlcservice::kErrorNone);

  EXPECT_CALL(*observer_, OnCancelInitiated());

  installer_->Cancel();
  task_environment_.RunUntilIdle();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 0);
  EXPECT_FALSE(
      BorealisService::GetForProfile(profile_.get())->Features().IsEnabled());

  ExpectObserverEventsUntil(InstallingState::kInstallingDlc);
  EXPECT_CALL(*observer_, OnInstallationEnded(BorealisInstallResult::kSuccess));

  installer_->Start();
  task_environment_.RunUntilIdle();

  UpdateCurrentDlcs();
  ASSERT_EQ(current_dlcs_.dlc_infos_size(), 1);
  EXPECT_EQ(current_dlcs_.dlc_infos(0).id(), borealis::kBorealisDlcName);
  EXPECT_TRUE(
      BorealisService::GetForProfile(profile_.get())->Features().IsEnabled());
}

TEST_F(BorealisInstallerTest, SucessfulInstallationRecordMetrics) {
  feature_list_.InitAndEnableFeature(features::kBorealis);
  fake_dlcservice_client_->set_install_error(dlcservice::kErrorNone);

  ExpectObserverEventsUntil(InstallingState::kInstallingDlc);
  EXPECT_CALL(*observer_, OnInstallationEnded(BorealisInstallResult::kSuccess));
  StartAndRunToCompletion();

  histogram_tester_->ExpectTotalCount(kBorealisInstallNumAttemptsHistogram, 1);
  histogram_tester_->ExpectUniqueSample(kBorealisInstallResultHistogram,
                                        BorealisInstallResult::kSuccess, 1);
  histogram_tester_->ExpectTotalCount(kBorealisInstallOverallTimeHistogram, 1);
}

TEST_F(BorealisInstallerTest, IncompleteInstallationRecordMetrics) {
  feature_list_.InitAndEnableFeature(features::kBorealis);
  // This error is arbitrarily chosen for simplicity.
  fake_dlcservice_client_->set_install_error(dlcservice::kErrorInternal);

  ExpectObserverEventsUntil(InstallingState::kInstallingDlc);
  EXPECT_CALL(*observer_,
              OnInstallationEnded(BorealisInstallResult::kDlcInternalError));
  StartAndRunToCompletion();

  histogram_tester_->ExpectTotalCount(kBorealisInstallNumAttemptsHistogram, 1);
  histogram_tester_->ExpectUniqueSample(
      kBorealisInstallResultHistogram, BorealisInstallResult::kDlcInternalError,
      1);
  histogram_tester_->ExpectTotalCount(kBorealisInstallOverallTimeHistogram, 0);
}

// Note that we don't check if the DLC has/hasn't been installed, since the
// mocked DLC service will always suceeed, so we only care about how the error
// code returned by the service is handled by the installer.
TEST_P(BorealisInstallerTestDlc, DlcError) {
  feature_list_.InitAndEnableFeature(features::kBorealis);
  fake_dlcservice_client_->set_install_error(GetParam().first);

  ExpectObserverEventsUntil(InstallingState::kInstallingDlc);
  EXPECT_CALL(*observer_, OnInstallationEnded(GetParam().second));

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
                        "unkown",
                        BorealisInstallResult::kDlcUnknownError)));
}  // namespace borealis
