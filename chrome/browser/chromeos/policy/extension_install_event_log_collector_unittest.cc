// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_collector.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kEthernetServicePath[] = "/service/eth1";
constexpr char kWifiServicePath[] = "/service/wifi1";

constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";

constexpr char kExtensionName1[] = "name1";

constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";

class FakeExtensionInstallEventLogCollectorDelegate
    : public ExtensionInstallEventLogCollector::Delegate {
 public:
  FakeExtensionInstallEventLogCollectorDelegate() = default;
  ~FakeExtensionInstallEventLogCollectorDelegate() override = default;

  struct Request {
    Request(bool for_all,
            bool add_disk_space_info,
            const extensions::ExtensionId& extension_id,
            const em::ExtensionInstallReportLogEvent& event)
        : for_all(for_all),
          add_disk_space_info(add_disk_space_info),
          extension_id(extension_id),
          event(event) {}
    const bool for_all;
    const bool add_disk_space_info;
    const extensions::ExtensionId extension_id;
    const em::ExtensionInstallReportLogEvent event;
  };

  // AppInstallEventLogCollector::Delegate:
  void AddForAllExtensions(
      std::unique_ptr<em::ExtensionInstallReportLogEvent> event) override {
    ++add_for_all_count_;
    requests_.emplace_back(true /* for_all */, false /* add_disk_space_info */,
                           std::string() /* extension_id */, *event);
  }

  void Add(const extensions::ExtensionId& extension_id,
           bool add_disk_space_info,
           std::unique_ptr<em::ExtensionInstallReportLogEvent> event) override {
    ++add_count_;
    requests_.emplace_back(false /* for_all */, add_disk_space_info,
                           extension_id, *event);
  }

  void OnExtensionInstallationFinished(
      const extensions::ExtensionId& extension_id) override {
    extensions_.erase(extension_id);
  }

  bool IsExtensionPending(
      const extensions::ExtensionId& extension_id) override {
    return extensions_.find(extension_id) != extensions_.end();
  }

  int add_for_all_count() const { return add_for_all_count_; }

  int add_count() const { return add_count_; }

  const em::ExtensionInstallReportLogEvent& last_event() const {
    return last_request().event;
  }
  const Request& last_request() const { return requests_.back(); }
  const std::vector<Request>& requests() const { return requests_; }

 private:
  std::set<extensions::ExtensionId> extensions_ = {kExtensionId1};
  int add_for_all_count_ = 0;
  int add_count_ = 0;
  std::vector<Request> requests_;
};

}  // namespace

class ExtensionInstallEventLogCollectorTest : public testing::Test {
 protected:
  ExtensionInstallEventLogCollectorTest() = default;
  ~ExtensionInstallEventLogCollectorTest() override = default;

  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);

    chromeos::DBusThreadManager::Initialize();
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::NetworkHandler::Initialize();
    profile_ = std::make_unique<TestingProfile>();
    registry_ = extensions::ExtensionRegistry::Get(profile_.get());
    install_stage_tracker_ =
        extensions::InstallStageTracker::Get(profile_.get());
    InitExtensionSystem();
    service_test_ = chromeos::DBusThreadManager::Get()
                        ->GetShillServiceClient()
                        ->GetTestInterface();
    service_test_->AddService(kEthernetServicePath, "eth1_guid", "eth1",
                              shill::kTypeEthernet, shill::kStateOffline,
                              true /* visible */);
    service_test_->AddService(kWifiServicePath, "wifi1_guid", "wifi1",
                              shill::kTypeEthernet, shill::kStateOffline,
                              true /* visible */);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    profile_.reset();
    chromeos::NetworkHandler::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void InitExtensionSystem() {
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);
  }

  void SetNetworkState(
      network::NetworkConnectionTracker::NetworkConnectionObserver* observer,
      const std::string& service_path,
      const std::string& state) {
    service_test_->SetServiceProperty(service_path, shill::kStateProperty,
                                      base::Value(state));
    base::RunLoop().RunUntilIdle();

    network::mojom::ConnectionType connection_type =
        network::mojom::ConnectionType::CONNECTION_NONE;
    std::string network_state;
    service_test_->GetServiceProperties(kWifiServicePath)
        ->GetString(shill::kStateProperty, &network_state);
    if (network_state == shill::kStateOnline) {
      connection_type = network::mojom::ConnectionType::CONNECTION_WIFI;
    }
    service_test_->GetServiceProperties(kEthernetServicePath)
        ->GetString(shill::kStateProperty, &network_state);
    if (network_state == shill::kStateOnline) {
      connection_type = network::mojom::ConnectionType::CONNECTION_ETHERNET;
    }
    if (observer)
      observer->OnConnectionChanged(connection_type);
    base::RunLoop().RunUntilIdle();
  }

  testing::AssertionResult VerifyEventAddedSuccessfully(
      int expected_add_count,
      int expected_add_all_count) {
    if (expected_add_count != delegate()->add_count()) {
      return testing::AssertionFailure()
             << "Expected add count: " << expected_add_count
             << " but actual add count: " << delegate()->add_count();
    }
    if (expected_add_all_count != delegate()->add_for_all_count()) {
      return testing::AssertionFailure()
             << "Expected add all count: " << expected_add_all_count
             << " but actual add all count: "
             << delegate()->add_for_all_count();
    }
    if (delegate()->last_request().extension_id != kExtensionId1) {
      return testing::AssertionFailure()
             << "Expected extension id: " << kExtensionId1
             << " but actual extension id: "
             << delegate()->last_request().extension_id;
    }
    return testing::AssertionSuccess();
  }

  TestingProfile* profile() { return profile_.get(); }
  extensions::ExtensionRegistry* registry() { return registry_; }
  FakeExtensionInstallEventLogCollectorDelegate* delegate() {
    return &delegate_;
  }
  extensions::InstallStageTracker* install_stage_tracker() {
    return install_stage_tracker_;
  }

  chromeos::ShillServiceClient::TestInterface* service_test_ = nullptr;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  extensions::ExtensionRegistry* registry_;
  extensions::InstallStageTracker* install_stage_tracker_;
  FakeExtensionInstallEventLogCollectorDelegate delegate_;
  TestingPrefServiceSimple pref_service_;
};

// Test the case when collector is created and destroyed inside the one user
// session. In this case no event is generated. This happens for example when
// all extensions are installed in context of the same user session.
TEST_F(ExtensionInstallEventLogCollectorTest, NoEventsByDefault) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());
  collector.reset();

  EXPECT_EQ(0, delegate()->add_count());
  EXPECT_EQ(0, delegate()->add_for_all_count());
}

TEST_F(ExtensionInstallEventLogCollectorTest, LoginLogout) {
  chromeos::FakeChromeUserManager* fake_user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  user_manager::User* user =
      fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
          account_id, false /*is_affiliated*/, user_manager::USER_TYPE_REGULAR,
          profile());
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  EXPECT_EQ(0, delegate()->add_for_all_count());

  collector->AddLoginEvent();
  EXPECT_EQ(1, delegate()->add_for_all_count());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::LOGIN,
            delegate()->last_event().session_state_change_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::USER_TYPE_REGULAR,
            delegate()->last_event().user_type());
  EXPECT_EQ(true, delegate()->last_event().is_new_user());
  EXPECT_TRUE(delegate()->last_event().has_online());
  EXPECT_FALSE(delegate()->last_event().online());
}

TEST_F(ExtensionInstallEventLogCollectorTest, LoginTypes) {
  chromeos::FakeChromeUserManager* fake_user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  user_manager::User* user =
      fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
          account_id, false /*is_affiliated*/, user_manager::USER_TYPE_REGULAR,
          profile());
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
  {
    ExtensionInstallEventLogCollector collector(registry(), delegate(),
                                                profile());
    collector.AddLoginEvent();
    EXPECT_EQ(1, delegate()->add_for_all_count());
    EXPECT_EQ(em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE,
              delegate()->last_event().event_type());
    EXPECT_EQ(em::ExtensionInstallReportLogEvent::LOGIN,
              delegate()->last_event().session_state_change_type());
    EXPECT_EQ(em::ExtensionInstallReportLogEvent::USER_TYPE_REGULAR,
              delegate()->last_event().user_type());
    EXPECT_EQ(true, delegate()->last_event().is_new_user());
    EXPECT_TRUE(delegate()->last_event().has_online());
    EXPECT_FALSE(delegate()->last_event().online());
  }

  {
    // Check login after restart. No log is expected.
    ExtensionInstallEventLogCollector collector(registry(), delegate(),
                                                profile());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kLoginUser);
    collector.AddLoginEvent();
    EXPECT_EQ(1, delegate()->add_for_all_count());
  }

  {
    // Check logout on restart. No log is expected.
    ExtensionInstallEventLogCollector collector(registry(), delegate(),
                                                profile());
    g_browser_process->local_state()->SetBoolean(prefs::kWasRestarted, true);
    collector.AddLogoutEvent();
    EXPECT_EQ(1, delegate()->add_for_all_count());
  }

  EXPECT_EQ(0, delegate()->add_count());
}

TEST_F(ExtensionInstallEventLogCollectorTest, SuspendResume) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, delegate()->add_for_all_count());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SUSPEND,
            delegate()->last_event().session_state_change_type());
  EXPECT_FALSE(delegate()->last_event().online());

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::RESUME,
            delegate()->last_event().session_state_change_type());
  EXPECT_FALSE(delegate()->last_event().online());

  collector.reset();

  EXPECT_EQ(0, delegate()->add_count());
}

// Connect to Ethernet. Start log collector. Verify that a login event with
// network state online is recorded. Then, connect to WiFi and disconnect from
// Ethernet, in this order. Verify that no event is recorded. Then, disconnect
// from WiFi. Verify that a connectivity change event is recorded. Then, connect
// to WiFi with a pending captive portal. Verify that no event is recorded.
// Then, pass the captive portal. Verify that a connectivity change is recorded.
TEST_F(ExtensionInstallEventLogCollectorTest, ConnectivityChanges) {
  SetNetworkState(nullptr, kEthernetServicePath, shill::kStateOnline);
  chromeos::FakeChromeUserManager* fake_user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  user_manager::User* user =
      fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
          account_id, false /*is_affiliated*/, user_manager::USER_TYPE_REGULAR,
          profile());
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);

  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  EXPECT_EQ(0, delegate()->add_for_all_count());

  collector->AddLoginEvent();
  EXPECT_EQ(1, delegate()->add_for_all_count());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::LOGIN,
            delegate()->last_event().session_state_change_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::USER_TYPE_REGULAR,
            delegate()->last_event().user_type());
  EXPECT_EQ(true, delegate()->last_event().is_new_user());
  EXPECT_TRUE(delegate()->last_event().online());

  SetNetworkState(collector.get(), kWifiServicePath, shill::kStateOnline);
  EXPECT_EQ(1, delegate()->add_for_all_count());

  SetNetworkState(collector.get(), kEthernetServicePath, shill::kStateOffline);
  EXPECT_EQ(1, delegate()->add_for_all_count());

  SetNetworkState(collector.get(), kWifiServicePath, shill::kStateOffline);
  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::CONNECTIVITY_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_FALSE(delegate()->last_event().online());

  SetNetworkState(collector.get(), kWifiServicePath,
                  shill::kStateNoConnectivity);
  EXPECT_EQ(2, delegate()->add_for_all_count());

  SetNetworkState(collector.get(), kWifiServicePath, shill::kStateOnline);
  EXPECT_EQ(3, delegate()->add_for_all_count());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::CONNECTIVITY_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_TRUE(delegate()->last_event().online());

  collector.reset();

  EXPECT_EQ(3, delegate()->add_for_all_count());
  EXPECT_EQ(0, delegate()->add_count());
}

TEST_F(ExtensionInstallEventLogCollectorTest,
       ExtensionInstallFailedWithoutMisconfiguration) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  // One extension failed.
  install_stage_tracker()->ReportInfoOnNoUpdatesFailure(kExtensionId1,
                                                        "rate limit");
  install_stage_tracker()->ReportFailure(
      kExtensionId1,
      extensions::InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(1 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED,
            delegate()->last_request().event.event_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::CRX_FETCH_URL_EMPTY,
            delegate()->last_request().event.failure_reason());
  EXPECT_FALSE(delegate()->last_request().event.is_misconfiguration_failure());
}

TEST_F(ExtensionInstallEventLogCollectorTest,
       ExtensionInstallFailedWithMisconfiguration) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  // One extension failed.
  install_stage_tracker()->ReportInfoOnNoUpdatesFailure(kExtensionId1, "");
  install_stage_tracker()->ReportFailure(
      kExtensionId1,
      extensions::InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(1 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED,
            delegate()->last_request().event.event_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::CRX_FETCH_URL_EMPTY,
            delegate()->last_request().event.failure_reason());
  EXPECT_TRUE(delegate()->last_request().event.is_misconfiguration_failure());
}

// Simulate failure after unpacking, so extension type should be reported.
TEST_F(ExtensionInstallEventLogCollectorTest, ExtensionInstallFailedWithType) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  extensions::InstallStageTracker* tracker =
      extensions::InstallStageTracker::Get(profile());

  // One extension failed.
  tracker->ReportExtensionType(kExtensionId1,
                               extensions::Manifest::TYPE_LEGACY_PACKAGED_APP);
  tracker->ReportFailure(kExtensionId1,
                         extensions::InstallStageTracker::FailureReason::
                             CRX_INSTALL_ERROR_DECLINED);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(1 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED,
            delegate()->last_request().event.event_type());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::CRX_INSTALL_ERROR_DECLINED,
            delegate()->last_request().event.failure_reason());
  EXPECT_EQ(em::Extension::TYPE_LEGACY_PACKAGED_APP,
            delegate()->last_request().event.extension_type());
}

TEST_F(ExtensionInstallEventLogCollectorTest, InstallExtension) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  // One extension installation succeeded.
  auto ext = extensions::ExtensionBuilder(kExtensionName1)
                 .SetID(kExtensionId1)
                 .Build();
  collector->OnExtensionLoaded(profile(), ext.get());
  ASSERT_TRUE(VerifyEventAddedSuccessfully(1 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SUCCESS,
            delegate()->last_request().event.event_type());
  EXPECT_EQ(em::Extension::TYPE_EXTENSION,
            delegate()->last_request().event.extension_type());
}

// Verifies that a new event is created when the installation stage is changed
// during the installation process.
TEST_F(ExtensionInstallEventLogCollectorTest, InstallationStageChanged) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  // One extension installation succeeded.
  auto ext = extensions::ExtensionBuilder(kExtensionName1)
                 .SetID(kExtensionId1)
                 .Build();
  collector->OnExtensionInstallationStageChanged(
      kExtensionId1, extensions::InstallStageTracker::Stage::DOWNLOADING);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(1 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::DOWNLOADING,
            delegate()->last_request().event.installation_stage());
  collector->OnExtensionInstallationStageChanged(
      kExtensionId1, extensions::InstallStageTracker::Stage::INSTALLING);
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::INSTALLING,
            delegate()->last_request().event.installation_stage());
  ASSERT_TRUE(VerifyEventAddedSuccessfully(2 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
}

// Verifies that a new event is created when the downloading stage is changed
// during the downloading process.
TEST_F(ExtensionInstallEventLogCollectorTest, DownloadingStageChanged) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  auto ext = extensions::ExtensionBuilder(kExtensionName1)
                 .SetID(kExtensionId1)
                 .Build();
  collector->OnExtensionDownloadingStageChanged(
      kExtensionId1, extensions::ExtensionDownloaderDelegate::Stage::PENDING);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(1 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::DOWNLOAD_PENDING,
            delegate()->last_request().event.downloading_stage());
  collector->OnExtensionDownloadingStageChanged(
      kExtensionId1,
      extensions::ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(2 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::DOWNLOADING_MANIFEST,
            delegate()->last_request().event.downloading_stage());
  collector->OnExtensionDownloadingStageChanged(
      kExtensionId1,
      extensions::ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(3 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::DOWNLOADING_CRX,
            delegate()->last_request().event.downloading_stage());
  collector->OnExtensionDownloadingStageChanged(
      kExtensionId1, extensions::ExtensionDownloaderDelegate::Stage::FINISHED);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(4 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::FINISHED,
            delegate()->last_request().event.downloading_stage());
}

// Verifies that a new event is created when the install creation stage is
// changed.
TEST_F(ExtensionInstallEventLogCollectorTest, InstallCreationStageChanged) {
  std::unique_ptr<ExtensionInstallEventLogCollector> collector =
      std::make_unique<ExtensionInstallEventLogCollector>(
          registry(), delegate(), profile());

  auto ext = extensions::ExtensionBuilder(kExtensionName1)
                 .SetID(kExtensionId1)
                 .Build();
  collector->OnExtensionInstallCreationStageChanged(
      kExtensionId1, extensions::InstallStageTracker::InstallCreationStage::
                         CREATION_INITIATED);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(1 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::CREATION_INITIATED,
            delegate()->last_request().event.install_creation_stage());
  collector->OnExtensionInstallCreationStageChanged(
      kExtensionId1, extensions::InstallStageTracker::InstallCreationStage::
                         NOTIFIED_FROM_MANAGEMENT);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(2 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::NOTIFIED_FROM_MANAGEMENT,
            delegate()->last_request().event.install_creation_stage());
  collector->OnExtensionInstallCreationStageChanged(
      kExtensionId1, extensions::InstallStageTracker::InstallCreationStage::
                         SEEN_BY_POLICY_LOADER);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(3 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SEEN_BY_POLICY_LOADER,
            delegate()->last_request().event.install_creation_stage());
  collector->OnExtensionInstallCreationStageChanged(
      kExtensionId1, extensions::InstallStageTracker::InstallCreationStage::
                         SEEN_BY_EXTERNAL_PROVIDER);
  ASSERT_TRUE(VerifyEventAddedSuccessfully(4 /*expected_add_count*/,
                                           0 /*expected_add_all_count*/));
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SEEN_BY_EXTERNAL_PROVIDER,
            delegate()->last_request().event.install_creation_stage());
}

}  // namespace policy
