// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_collector.h"

#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kEthernetServicePath[] = "/service/eth1";
constexpr char kWifiServicePath[] = "/service/wifi1";

constexpr char kPackageName[] = "com.example.app";
constexpr char kPackageName2[] = "com.example.app2";

class FakeAppInstallEventLogCollectorDelegate
    : public ArcAppInstallEventLogCollector::Delegate {
 public:
  FakeAppInstallEventLogCollectorDelegate() = default;

  FakeAppInstallEventLogCollectorDelegate(
      const FakeAppInstallEventLogCollectorDelegate&) = delete;
  FakeAppInstallEventLogCollectorDelegate& operator=(
      const FakeAppInstallEventLogCollectorDelegate&) = delete;

  ~FakeAppInstallEventLogCollectorDelegate() override = default;

  struct Request {
    Request(bool for_all,
            bool add_disk_space_info,
            const std::string& package_name,
            const em::AppInstallReportLogEvent& event)
        : for_all(for_all),
          add_disk_space_info(add_disk_space_info),
          package_name(package_name),
          event(event) {}
    const bool for_all;
    const bool add_disk_space_info;
    const std::string package_name;
    const em::AppInstallReportLogEvent event;
  };

  // ArcAppInstallEventLogCollector::Delegate:
  void AddForAllPackages(
      std::unique_ptr<em::AppInstallReportLogEvent> event) override {
    ++add_for_all_count_;
    requests_.emplace_back(true /* for_all */, false /* add_disk_space_info */,
                           std::string() /* package_name */, *event);
  }

  void Add(const std::string& package_name,
           bool add_disk_space_info,
           std::unique_ptr<em::AppInstallReportLogEvent> event) override {
    ++add_count_;
    requests_.emplace_back(false /* for_all */, add_disk_space_info,
                           package_name, *event);
  }

  void UpdatePolicySuccessRate(const std::string& package_name,
                               bool success) override {
    ++update_policy_success_rate_count_;
    auto event = std::make_unique<em::AppInstallReportLogEvent>();
    event->set_event_type(
        success ? em::AppInstallReportLogEvent::INSTALLATION_FINISHED
                : em::AppInstallReportLogEvent::INSTALLATION_FAILED);

    requests_.emplace_back(false /* for_all */, false /* add_disk_space_info */,
                           package_name, *event);
  }

  int add_for_all_count() const { return add_for_all_count_; }

  int add_count() const { return add_count_; }

  int update_policy_success_rate_count() const {
    return update_policy_success_rate_count_;
  }

  const em::AppInstallReportLogEvent& last_event() const {
    return last_request().event;
  }
  const Request& last_request() const { return requests_.back(); }

  const em::AppInstallReportLogEvent& event_at(int index) const {
    return request_at(index).event;
  }

  const Request& request_at(int index) const { return requests_.at(index); }

  const std::vector<Request>& requests() const { return requests_; }

 private:
  int add_for_all_count_ = 0;
  int add_count_ = 0;
  int update_policy_success_rate_count_ = 0;
  std::vector<Request> requests_;
};

int64_t TimeToTimestamp(base::Time time) {
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

}  // namespace

class ArcAppInstallEventLogCollectorTest : public testing::Test {
 public:
  ArcAppInstallEventLogCollectorTest(
      const ArcAppInstallEventLogCollectorTest&) = delete;
  ArcAppInstallEventLogCollectorTest& operator=(
      const ArcAppInstallEventLogCollectorTest&) = delete;

 protected:
  ArcAppInstallEventLogCollectorTest() = default;
  ~ArcAppInstallEventLogCollectorTest() override = default;

  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);

    chromeos::PowerManagerClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    arc_app_test_.SetUp(profile_.get());

    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
    network_handler_test_helper_->service_test()->AddService(
        kEthernetServicePath, "eth1_guid", "eth1", shill::kTypeEthernet,
        shill::kStateIdle, true /* visible */);
    network_handler_test_helper_->service_test()->AddService(
        kWifiServicePath, "wifi1_guid", "wifi1", shill::kTypeEthernet,
        shill::kStateIdle, true /* visible */);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_handler_test_helper_.reset();
    arc_app_test_.TearDown();

    profile_.reset();
    chromeos::PowerManagerClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void SetNetworkState(
      network::NetworkConnectionTracker::NetworkConnectionObserver* observer,
      const std::string& service_path,
      const std::string& state) {
    network_handler_test_helper_->service_test()->SetServiceProperty(
        service_path, shill::kStateProperty, base::Value(state));
    base::RunLoop().RunUntilIdle();

    network::mojom::ConnectionType connection_type =
        network::mojom::ConnectionType::CONNECTION_NONE;
    const std::string* network_state =
        network_handler_test_helper_->service_test()
            ->GetServiceProperties(kWifiServicePath)
            ->FindString(shill::kStateProperty);
    if (network_state && *network_state == shill::kStateOnline) {
      connection_type = network::mojom::ConnectionType::CONNECTION_WIFI;
    }
    network_state = network_handler_test_helper_->service_test()
                        ->GetServiceProperties(kEthernetServicePath)
                        ->FindString(shill::kStateProperty);
    if (network_state && *network_state == shill::kStateOnline) {
      connection_type = network::mojom::ConnectionType::CONNECTION_ETHERNET;
    }
    if (observer)
      observer->OnConnectionChanged(connection_type);
    base::RunLoop().RunUntilIdle();
  }

  TestingProfile* profile() { return profile_.get(); }
  FakeAppInstallEventLogCollectorDelegate* delegate() { return &delegate_; }
  ArcAppListPrefs* app_prefs() { return arc_app_test_.arc_app_list_prefs(); }

  const std::set<std::string> packages_ = {kPackageName};

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<TestingProfile> profile_;
  FakeAppInstallEventLogCollectorDelegate delegate_;
  TestingPrefServiceSimple pref_service_;
  ArcAppTest arc_app_test_;
};

// Test the case when collector is created and destroyed inside the one user
// session. In this case no event is generated. This happens for example when
// all apps are installed in context of the same user session.
TEST_F(ArcAppInstallEventLogCollectorTest, NoEventsByDefault) {
  std::unique_ptr<ArcAppInstallEventLogCollector> collector =
      std::make_unique<ArcAppInstallEventLogCollector>(delegate(), profile(),
                                                       packages_);
  collector.reset();

  EXPECT_EQ(0, delegate()->add_count());
  EXPECT_EQ(0, delegate()->add_for_all_count());
  EXPECT_EQ(0, delegate()->update_policy_success_rate_count());
}

TEST_F(ArcAppInstallEventLogCollectorTest, LoginLogout) {
  std::unique_ptr<ArcAppInstallEventLogCollector> collector =
      std::make_unique<ArcAppInstallEventLogCollector>(delegate(), profile(),
                                                       packages_);

  EXPECT_EQ(0, delegate()->add_for_all_count());

  collector->OnLogin();
  EXPECT_EQ(1, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::LOGIN,
            delegate()->last_event().session_state_change_type());
  EXPECT_TRUE(delegate()->last_event().has_online());
  EXPECT_FALSE(delegate()->last_event().online());

  collector->OnLogout();
  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::LOGOUT,
            delegate()->last_event().session_state_change_type());
  EXPECT_FALSE(delegate()->last_event().has_online());

  collector.reset();

  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(0, delegate()->add_count());
}

TEST_F(ArcAppInstallEventLogCollectorTest, LoginTypes) {
  {
    ArcAppInstallEventLogCollector collector(delegate(), profile(), packages_);
    collector.OnLogin();
    EXPECT_EQ(1, delegate()->add_for_all_count());
    EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
              delegate()->last_event().event_type());
    EXPECT_EQ(em::AppInstallReportLogEvent::LOGIN,
              delegate()->last_event().session_state_change_type());
    EXPECT_TRUE(delegate()->last_event().has_online());
    EXPECT_FALSE(delegate()->last_event().online());
  }

  {
    // Check login after restart. No log is expected.
    ArcAppInstallEventLogCollector collector(delegate(), profile(), packages_);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kLoginUser);
    collector.OnLogin();
    EXPECT_EQ(1, delegate()->add_for_all_count());
  }

  {
    // Check logout on restart. No log is expected.
    ArcAppInstallEventLogCollector collector(delegate(), profile(), packages_);
    g_browser_process->local_state()->SetBoolean(prefs::kWasRestarted, true);
    collector.OnLogout();
    EXPECT_EQ(1, delegate()->add_for_all_count());
  }

  EXPECT_EQ(0, delegate()->add_count());
}

TEST_F(ArcAppInstallEventLogCollectorTest, SuspendResume) {
  std::unique_ptr<ArcAppInstallEventLogCollector> collector =
      std::make_unique<ArcAppInstallEventLogCollector>(delegate(), profile(),
                                                       packages_);

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::SUSPEND,
            delegate()->last_event().session_state_change_type());

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::RESUME,
            delegate()->last_event().session_state_change_type());

  collector.reset();

  EXPECT_EQ(0, delegate()->add_count());
}

// Connect to Ethernet. Start log collector. Verify that a login event with
// network state online is recorded. Then, connect to WiFi and disconnect from
// Ethernet, in this order. Verify that no event is recorded. Then, disconnect
// from WiFi. Verify that a connectivity change event is recorded. Then, connect
// to WiFi with a pending captive portal. Verify that no event is recorded.
// Then, pass the captive portal. Verify that a connectivity change is recorded.
TEST_F(ArcAppInstallEventLogCollectorTest, ConnectivityChanges) {
  SetNetworkState(nullptr, kEthernetServicePath, shill::kStateOnline);

  std::unique_ptr<ArcAppInstallEventLogCollector> collector =
      std::make_unique<ArcAppInstallEventLogCollector>(delegate(), profile(),
                                                       packages_);

  EXPECT_EQ(0, delegate()->add_for_all_count());

  collector->OnLogin();
  EXPECT_EQ(1, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::LOGIN,
            delegate()->last_event().session_state_change_type());
  EXPECT_TRUE(delegate()->last_event().online());

  SetNetworkState(collector.get(), kWifiServicePath, shill::kStateOnline);
  EXPECT_EQ(1, delegate()->add_for_all_count());

  SetNetworkState(collector.get(), kEthernetServicePath, shill::kStateIdle);
  EXPECT_EQ(1, delegate()->add_for_all_count());

  SetNetworkState(collector.get(), kWifiServicePath, shill::kStateIdle);
  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::CONNECTIVITY_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_FALSE(delegate()->last_event().online());

  SetNetworkState(collector.get(), kWifiServicePath,
                  shill::kStateNoConnectivity);
  EXPECT_EQ(2, delegate()->add_for_all_count());

  SetNetworkState(collector.get(), kWifiServicePath, shill::kStateOnline);
  EXPECT_EQ(3, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::CONNECTIVITY_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_TRUE(delegate()->last_event().online());

  collector.reset();

  EXPECT_EQ(3, delegate()->add_for_all_count());
  EXPECT_EQ(0, delegate()->add_count());
}

TEST_F(ArcAppInstallEventLogCollectorTest, InstallPackages) {
  arc::mojom::AppHost* const app_host = app_prefs();

  std::unique_ptr<ArcAppInstallEventLogCollector> collector =
      std::make_unique<ArcAppInstallEventLogCollector>(delegate(), profile(),
                                                       packages_);

  app_host->OnInstallationStarted(kPackageName);
  ASSERT_EQ(1, delegate()->add_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::INSTALLATION_STARTED,
            delegate()->last_event().event_type());
  EXPECT_EQ(kPackageName, delegate()->last_request().package_name);
  EXPECT_TRUE(delegate()->last_request().add_disk_space_info);

  // kPackageName2 is not in the pending set.
  app_host->OnInstallationStarted(kPackageName2);
  EXPECT_EQ(1, delegate()->add_count());

  arc::mojom::InstallationResult result;
  result.package_name = kPackageName;
  result.success = true;
  app_host->OnInstallationFinished(
      arc::mojom::InstallationResultPtr(result.Clone()));
  EXPECT_EQ(2, delegate()->add_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::INSTALLATION_FINISHED,
            delegate()->last_event().event_type());
  EXPECT_EQ(kPackageName, delegate()->last_request().package_name);
  EXPECT_TRUE(delegate()->last_request().add_disk_space_info);

  collector->OnPendingPackagesChanged({kPackageName, kPackageName2});

  app_host->OnInstallationStarted(kPackageName2);
  EXPECT_EQ(3, delegate()->add_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::INSTALLATION_STARTED,
            delegate()->last_event().event_type());
  EXPECT_EQ(kPackageName2, delegate()->last_request().package_name);
  EXPECT_TRUE(delegate()->last_request().add_disk_space_info);

  result.package_name = kPackageName2;
  result.success = false;
  app_host->OnInstallationFinished(
      arc::mojom::InstallationResultPtr(result.Clone()));
  EXPECT_EQ(4, delegate()->add_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::INSTALLATION_FAILED,
            delegate()->last_event().event_type());
  EXPECT_EQ(kPackageName2, delegate()->last_request().package_name);
  EXPECT_TRUE(delegate()->last_request().add_disk_space_info);
}

TEST_F(ArcAppInstallEventLogCollectorTest, OnPlayStoreLocalPolicySet) {
  std::unique_ptr<ArcAppInstallEventLogCollector> collector =
      std::make_unique<ArcAppInstallEventLogCollector>(delegate(), profile(),
                                                       packages_);
  base::Time time = base::Time::Now();
  collector->OnPlayStoreLocalPolicySet(time, packages_);
  ASSERT_EQ(1, delegate()->add_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::PLAYSTORE_LOCAL_POLICY_SET,
            delegate()->last_event().event_type());
  EXPECT_EQ(TimeToTimestamp(time), delegate()->requests()[0].event.timestamp());
  EXPECT_EQ(kPackageName, delegate()->last_request().package_name);
  EXPECT_TRUE(delegate()->last_request().add_disk_space_info);
}

TEST_F(ArcAppInstallEventLogCollectorTest,
       UpdatePolicySuccessRate_InstallSuccess) {
  std::unique_ptr<ArcAppInstallEventLogCollector> collector =
      std::make_unique<ArcAppInstallEventLogCollector>(delegate(), profile(),
                                                       packages_);
  collector->OnInstallationFinished(kPackageName, /*success=*/true,
                                    /*is_launchable_app=*/true);

  int second_to_last_request_index = delegate()->requests().size() - 2;
  EXPECT_EQ(1, delegate()->update_policy_success_rate_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::INSTALLATION_FINISHED,
            delegate()->event_at(second_to_last_request_index).event_type());
  EXPECT_EQ(kPackageName,
            delegate()->request_at(second_to_last_request_index).package_name);
}

TEST_F(ArcAppInstallEventLogCollectorTest,
       UpdatePolicySuccessRate_InstallFailure) {
  std::unique_ptr<ArcAppInstallEventLogCollector> collector =
      std::make_unique<ArcAppInstallEventLogCollector>(delegate(), profile(),
                                                       packages_);
  collector->OnInstallationFinished(kPackageName, /*success=*/false,
                                    /*is_launchable_app=*/false);

  int second_to_last_request_index = delegate()->requests().size() - 2;
  EXPECT_EQ(1, delegate()->update_policy_success_rate_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::INSTALLATION_FAILED,
            delegate()->event_at(second_to_last_request_index).event_type());
  EXPECT_EQ(kPackageName,
            delegate()->request_at(second_to_last_request_index).package_name);
}

}  // namespace policy
