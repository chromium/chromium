// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/policy/networking/network_configuration_updater.h"
#include "chrome/browser/ui/ash/network/enrollment_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/services/network_config/cros_network_config.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_nss_types.h"
#include "dbus/object_path.h"
#include "net/cert/cert_database.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

namespace {

namespace network_mojom = ::chromeos::network_config::mojom;
using ::base::test::DictionaryHasValue;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::SizeIs;

constexpr char kUserProfilePath[] = "user_profile";
constexpr char kSharedProfilePath[] = "/profile/default";
constexpr char kDeviceWifiPath[] = "/device/wifi1";
constexpr char kServiceEth[] = "/service/0";
constexpr char kServiceWifi1[] = "/service/1";
constexpr char kServiceWifi2[] = "/service/2";
constexpr char kServiceWifi3[] = "/service/3";
constexpr char kServiceWifi4[] = "/service/4";

constexpr char kUIDataKeyUserSettings[] = "user_settings";

constexpr char kUserIdentity[] = "user_identity";

constexpr char kTestDomain[] = "test_domain";
constexpr char kTestDeviceId[] = "test_device_id";

// Records all values that shill service property had during the lifetime of
// ServicePropertyValueWatcher. Only supports string properties at the moment.
class ServicePropertyValueWatcher : public ash::ShillPropertyChangedObserver {
 public:
  ServicePropertyValueWatcher(
      ash::ShillServiceClient::TestInterface* shill_service_client_test,
      const std::string& service_path,
      const std::string& property_name)
      : shill_service_client_test_(shill_service_client_test),
        service_path_(service_path),
        property_name_(property_name) {
    ash::ShillServiceClient::Get()->AddPropertyChangedObserver(
        dbus::ObjectPath(service_path_), this);

    // If the service already exists and has `property_name`, record the initial
    // value.
    const base::Value::Dict* initial_service_properties =
        shill_service_client_test_->GetServiceProperties(service_path);
    if (!initial_service_properties) {
      return;
    }
    const std::string* property_value =
        initial_service_properties->FindString(property_name);
    if (!property_value) {
      return;
    }
    values_.push_back(*property_value);
  }

  ~ServicePropertyValueWatcher() override {
    ash::ShillServiceClient::Get()->RemovePropertyChangedObserver(
        dbus::ObjectPath(service_path_), this);
  }

  ServicePropertyValueWatcher(const ServicePropertyValueWatcher&) = delete;
  ServicePropertyValueWatcher& operator=(const ServicePropertyValueWatcher&) =
      delete;

  void OnPropertyChanged(const std::string& name,
                         const base::Value& value) override {
    if (name != property_name_) {
      return;
    }
    if (!value.is_string()) {
      return;
    }
    if (!values_.empty() && values_.back() == value.GetString()) {
      return;
    }
    values_.push_back(value.GetString());
    if (wait_for_value_state_ &&
        wait_for_value_state_->predicate.Run(value.GetString())) {
      wait_for_value_state_->run_loop.Quit();
    }
  }

  // Returns all values that the property passed to the constructor had since
  // this instance has been created.
  const std::vector<std::string>& GetValues() { return values_; }

  void WaitForNonEmptyValue() {
    WaitForValue(base::BindRepeating(
        [](const std::string& value) { return !value.empty(); }));
  }

  void WaitForValue(const std::string& expected_value) {
    WaitForValue(
        base::BindLambdaForTesting([expected_value](const std::string& value) {
          return value == expected_value;
        }));
  }

 private:
  using ValuePredicate = base::RepeatingCallback<bool(const std::string&)>;

  struct WaitForValueState {
    explicit WaitForValueState(ValuePredicate predicate)
        : predicate(predicate) {}

    ValuePredicate predicate;
    base::RunLoop run_loop;
  };

  void WaitForValue(ValuePredicate predicate) {
    if (!values_.empty() && predicate.Run(values_.back())) {
      return;
    }
    wait_for_value_state_.emplace(predicate);
    wait_for_value_state_->run_loop.Run();
    wait_for_value_state_.reset();
  }

  const raw_ptr<ash::ShillServiceClient::TestInterface>
      shill_service_client_test_;

  const std::string service_path_;
  const std::string property_name_;

  std::vector<std::string> values_;
  std::optional<WaitForValueState> wait_for_value_state_;
};

// Shorthand for ServicePropertyValueWatcher that allows waiting for a specific
// shill::kStateProperty value.
class ServiceStateWaiter {
 public:
  ServiceStateWaiter(
      ash::ShillServiceClient::TestInterface* shill_service_client_test,
      const std::string& service_path)
      : property_value_watcher_(shill_service_client_test,
                                service_path,
                                shill::kStateProperty) {}

  ServiceStateWaiter(const ServiceStateWaiter&) = delete;
  ServiceStateWaiter& operator=(const ServiceStateWaiter&) = delete;

  void Wait(const std::string& expected_state) {
    property_value_watcher_.WaitForValue(expected_state);
  }

 private:
  ServicePropertyValueWatcher property_value_watcher_;
};

// Registers itself as ash::NetworkPolicyObserver and records events for
// ash::NetworkPolicyObserver::PoliciesApplied and
// ash::NetworkPolicyObserver::PolicyAppliedtoNetwork.
class ScopedNetworkPolicyApplicationObserver
    : public ash::NetworkPolicyObserver {
 public:
  ScopedNetworkPolicyApplicationObserver() {
    ash::ManagedNetworkConfigurationHandler*
        managed_network_configuration_handler =
            ash::NetworkHandler::Get()->managed_network_configuration_handler();
    managed_network_configuration_handler->AddObserver(this);
  }

  ~ScopedNetworkPolicyApplicationObserver() override {
    ash::ManagedNetworkConfigurationHandler*
        managed_network_configuration_handler =
            ash::NetworkHandler::Get()->managed_network_configuration_handler();
    managed_network_configuration_handler->RemoveObserver(this);
  }

  void PoliciesApplied(const std::string& userhash) override {
    policies_applied_events_.push_back(userhash);
    policies_applied_wait_loop_[userhash].Quit();
  }

  void PolicyAppliedToNetwork(const std::string& service_path) override {
    policy_applied_to_network_events_.push_back(service_path);
  }

  const std::vector<std::string>& policies_applied_events() {
    return policies_applied_events_;
  }
  const std::vector<std::string>& policy_applied_to_network_events() {
    return policy_applied_to_network_events_;
  }

  // Clears all recorded events.
  void ResetEvents() {
    policies_applied_events_.clear();
    policy_applied_to_network_events_.clear();
  }

  void WaitPoliciesApplied(const std::string& userhash) {
    policies_applied_wait_loop_[userhash].Run();
  }

 private:
  std::vector<std::string> policies_applied_events_;
  std::vector<std::string> policy_applied_to_network_events_;

  std::map<std::string, base::RunLoop> policies_applied_wait_loop_;
};

class ScopedNetworkCertLoaderRefreshWaiter
    : public ash::NetworkCertLoader::Observer {
 public:
  ScopedNetworkCertLoaderRefreshWaiter() {
    ash::NetworkCertLoader::Get()->AddObserver(this);
  }

  ~ScopedNetworkCertLoaderRefreshWaiter() override {
    ash::NetworkCertLoader::Get()->RemoveObserver(this);
  }

  void OnCertificatesLoaded() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

// Allows waiting until a set of GUIDs is available as NetworkStateProperties in
// CrosNetworkConfig.
class CrosNetworkConfigGuidsAvailableWaiter
    : chromeos::network_config::CrosNetworkConfigObserver {
 public:
  CrosNetworkConfigGuidsAvailableWaiter(
      ash::network_config::CrosNetworkConfig* cros_network_config,
      const std::set<std::string>& expected_guids)
      : cros_network_config_(cros_network_config),
        expected_guids_(expected_guids) {
    // This is a "mojo remote" so it will be automatically removed when
    // destroyed.
    cros_network_config_->AddObserver(
        cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  }

  ~CrosNetworkConfigGuidsAvailableWaiter() override = default;

  void Wait() {
    if (DoAllNetworkStatesExist()) {
      return;
    }
    run_loop_.Run();
  }

 private:
  // Fired when the list of networks changes.
  void OnNetworkStateListChanged() override {
    if (DoAllNetworkStatesExist()) {
      run_loop_.Quit();
    }
  }

  bool DoAllNetworkStatesExist() {
    // Use GetNetworkState to check if a NetworkState with `guid_` exists.
    base::test::TestFuture<
        std::vector<network_mojom::NetworkStatePropertiesPtr>>
        network_states_future;
    cros_network_config_->GetNetworkStateList(
        AcceptEverything(), network_states_future.GetCallback());
    std::set<std::string> guids =
        NetworkStatesToGuids(network_states_future.Get());
    return std::includes(guids.begin(), guids.end(), expected_guids_.begin(),
                         expected_guids_.end());
  }

  static network_mojom::NetworkFilterPtr AcceptEverything() {
    return network_mojom::NetworkFilter::New(network_mojom::FilterType::kAll,
                                             network_mojom::NetworkType::kAll,
                                             network_mojom::kNoLimit);
  }

  static std::set<std::string> NetworkStatesToGuids(
      const std::vector<network_mojom::NetworkStatePropertiesPtr>&
          network_states) {
    std::set<std::string> guids;
    base::ranges::transform(network_states, std::inserter(guids, guids.begin()),
                            &network_mojom::NetworkStateProperties::guid);
    return guids;
  }

  base::RunLoop run_loop_;
  const raw_ptr<ash::network_config::CrosNetworkConfig> cros_network_config_;
  const std::set<std::string> expected_guids_;

  // Receiver for the CrosNetworkConfigObserver events.
  mojo::Receiver<network_mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
};

std::string OncPolicyToSelectClientCert(const std::string& guid,
                                        const std::string& ssid,
                                        const std::string& issuer_common_name,
                                        const std::string& identity) {
  return base::StringPrintf(R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "%s",
          "Name": "OncPolicyToSelectClientCert",
          "Type": "WiFi",
          "WiFi": {
             "EAP":  {
              "Outer": "EAP-TLS",
              "ClientCertType": "Pattern",
              "Identity": "%s",
              "ClientCertPattern": {
                "Issuer": {
                  "CommonName": "%s"
                }
              }
             },
             "SSID": "%s",
             "Security": "WPA-EAP"
          }
        }
      ]
    })",
                            guid.c_str(), identity.c_str(),
                            issuer_common_name.c_str(), ssid.c_str());
}

// Returns the configured static IP address from `shill_properties`, or an empty
// string if no static IP address is configured.
std::string GetStaticIPAddressFromShillProperties(
    const base::Value::Dict& shill_properties) {
  const base::Value::Dict* static_ip_config =
      shill_properties.FindDict(shill::kStaticIPConfigProperty);
  if (!static_ip_config) {
    return std::string();
  }
  const std::string* address =
      static_ip_config->FindString(shill::kAddressProperty);
  if (!address) {
    return std::string();
  }
  EXPECT_THAT(*address, Not(IsEmpty()));
  return *address;
}

// Returns the configured static name servers from `shill_properties`, or an
// empty vector if no static name servers are configured.
std::vector<std::string> GetStaticNameServersFromShillProperties(
    const base::Value::Dict& shill_properties) {
  const base::Value::Dict* static_ip_config =
      shill_properties.FindDict(shill::kStaticIPConfigProperty);
  if (!static_ip_config) {
    return {};
  }
  const base::Value::List* nameservers =
      static_ip_config->FindList(shill::kNameServersProperty);
  if (!nameservers) {
    return {};
  }
  std::vector<std::string> result;
  for (const base::Value& item : *nameservers) {
    result.push_back(item.GetString());
  }
  EXPECT_THAT(result, Not(IsEmpty()));
  return result;
}

// Waits until a views::View exists which is a (possibly indirect) child of a
// parent View and satisfies some predicate.
class ViewWaiter : public views::ViewObserver {
 public:
  // This will be used to check if a view is the one the ViewWaiter is waiting
  // for.
  using ViewPredicate = base::RepeatingCallback<bool(views::View*)>;

  ViewWaiter(views::View* observed_view, ViewPredicate view_predicate)
      : observed_view_(observed_view), view_predicate_(view_predicate) {}

  void Wait() {
    matching_view_ = FindExpectedView(observed_view_);
    if (matching_view_ != nullptr) {
      return;
    }

    scoped_observation_.Observe(observed_view_);
    run_loop.Run();
    if (matching_view_ == nullptr) {
      FAIL() << "Could not find the expected view";
    }
  }

  views::View* GetMatchingView() { return matching_view_; }

 private:
  void OnViewHierarchyChanged(
      views::View* observed_view,
      const views::ViewHierarchyChangedDetails& details) override {
    matching_view_ = FindExpectedView(observed_view_);
    if (matching_view_ != nullptr) {
      run_loop.Quit();
    }
  }

  views::View* FindExpectedView(views::View* view) {
    if (view_predicate_.Run(view)) {
      return view;
    }
    for (views::View* const child : view->children()) {
      if (views::View* const found = FindExpectedView(child)) {
        return found;
      }
    }
    return nullptr;
  }

  raw_ptr<views::View> observed_view_;
  ViewPredicate view_predicate_;
  base::ScopedObservation<views::View, ViewWaiter> scoped_observation_{this};
  base::RunLoop run_loop;
  raw_ptr<views::View> matching_view_;
};

ViewWaiter::ViewPredicate ViewWithLabel(const std::u16string& label) {
  return base::BindLambdaForTesting([label](views::View* view) {
    if (views::Label* const label_view = views::AsViewClass<views::Label>(view);
        label_view && label_view->GetText() == label) {
      return true;
    }
    return false;
  });
}

ViewWaiter::ViewPredicate LabelButtonWithLabel(const std::u16string& label) {
  return base::BindLambdaForTesting([label](views::View* view) {
    if (views::LabelButton* const label_button_view =
            views::AsViewClass<views::LabelButton>(view);
        label_button_view && label_button_view->GetText() == label) {
      return true;
    }
    return false;
  });
}

// Opens the "network detailed view" of the system tray and attempts to press on
// the entry that is displaying `ssid`. This relies on the fact that the SSID
// will be on the corresponding UI label verbatim.
void ConnectToSsidUsingSystemTray(std::string_view ssid) {
  auto system_tray = ash::SystemTrayTestApi::Create();
  system_tray->ShowNetworkDetailedView();

  ViewWaiter waiter(system_tray->GetMainBubbleView(),
                    ViewWithLabel(base::UTF8ToUTF16(ssid)));
  ASSERT_NO_FATAL_FAILURE(waiter.Wait());

  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());

  event_generator.MoveMouseTo(
      waiter.GetMatchingView()->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

void AcceptCertEnrollmentDialog(views::Widget* dialog_widget) {
  ViewWaiter waiter(dialog_widget->GetRootView(),
                    LabelButtonWithLabel(l10n_util::GetStringUTF16(
                        IDS_NETWORK_ENROLLMENT_HANDLER_BUTTON)));
  ASSERT_NO_FATAL_FAILURE(waiter.Wait());

  // For some reason, clicking on the button found by `waiter` (which is the
  // "Accept" button of the dialog) doesn't seem to have an action in a
  // browsertest, so "accept" the dialog programmatically.
  dialog_widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
}

}  // namespace

// This class is used for implementing integration tests for network policy
// application across sign-in screen and/or user session.
class NetworkPolicyApplicationTest : public ash::LoginManagerTest {
 public:
  NetworkPolicyApplicationTest() : LoginManagerTest() {
    MarkEnterpriseEnrolled();
    login_mixin_.AppendRegularUsers(1);
    test_account_id_ = login_mixin_.users()[0].account_id;
  }

  NetworkPolicyApplicationTest(const NetworkPolicyApplicationTest&) = delete;
  NetworkPolicyApplicationTest& operator=(const NetworkPolicyApplicationTest&) =
      delete;

 protected:
  // InProcessBrowserTest:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    BrowserPolicyConnector::SetPolicyProviderForTesting(&policy_provider_);

    LoginManagerTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);

    // Allow policy fetches to fail - these tests use
    // MockConfigurationPolicyProvider.
    command_line->AppendSwitch(ash::switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    shill_manager_client_test_ =
        ash::ShillManagerClient::Get()->GetTestInterface();
    shill_service_client_test_ =
        ash::ShillServiceClient::Get()->GetTestInterface();
    shill_profile_client_test_ =
        ash::ShillProfileClient::Get()->GetTestInterface();
    shill_device_client_test_ =
        ash::ShillDeviceClient::Get()->GetTestInterface();
    shill_service_client_test_->ClearServices();
    shill_device_client_test_->ClearDevices();
    shill_profile_client_test_->ClearProfiles();

    shill_manager_client_test_->SetWifiServicesVisibleByDefault(false);

    shill_manager_client_test_->AddTechnology(shill::kTypeWifi,
                                              true /* enabled */);
    shill_device_client_test_->AddDevice(kDeviceWifiPath, shill::kTypeWifi,
                                         "stub_wifi_device1");
    shill_profile_client_test_->AddProfile(kSharedProfilePath, "");
    shill_service_client_test_->ClearServices();

    cros_network_config_ =
        std::make_unique<ash::network_config::CrosNetworkConfig>();
  }

  void TearDownOnMainThread() override {
    cros_network_config_.reset();

    LoginManagerTest::TearDownOnMainThread();
  }

  void MarkEnterpriseEnrolled() {
    stub_install_attributes_.Get()->SetCloudManaged(kTestDomain, kTestDeviceId);
  }

  // Sets the DeviceEphemeralNetworkPoliciesEnabled policy to `value`.
  void SetDeviceEphemeralNetworkPoliciesEnabledPolicy(bool value) {
    current_policy_.Set(key::kDeviceEphemeralNetworkPoliciesEnabled,
                        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                        POLICY_SOURCE_CLOUD, base::Value(value), nullptr);
    policy_provider_.UpdateChromePolicy(current_policy_);
  }

  // Sets `device_onc_policy_blob` as DeviceOpenNetworkConfiguration device
  // policy. If `wait_applied` is true, waits for a
  // NetworkPolicyObserver::PoliciesApplied observer call for the device-wide
  // network profile.
  void SetDeviceOpenNetworkConfiguration(
      const std::string& device_onc_policy_blob,
      bool wait_applied) {
    ScopedNetworkPolicyApplicationObserver network_policy_application_observer;
    current_policy_.Set(key::kDeviceOpenNetworkConfiguration,
                        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                        POLICY_SOURCE_CLOUD,
                        base::Value(device_onc_policy_blob), nullptr);
    policy_provider_.UpdateChromePolicy(current_policy_);
    if (wait_applied) {
      network_policy_application_observer.WaitPoliciesApplied(
          /*userhash=*/std::string());
    }
  }

  // Sets `user_onc_policy_blob` as OpenNetworkConfiguration user policy using
  // `policy_provider_`. If `wait_applied` is true, waits for a
  // NetworkPolicyObserver::PoliciesApplied observer call for the network
  // profile for `user_hash`.
  void SetUserOpenNetworkConfiguration(const std::string& user_hash,
                                       const std::string& user_onc_policy_blob,
                                       bool wait_applied) {
    ScopedNetworkPolicyApplicationObserver network_policy_application_observer;
    current_policy_.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
                        POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                        base::Value(user_onc_policy_blob), nullptr);
    policy_provider_.UpdateChromePolicy(current_policy_);
    if (wait_applied) {
      network_policy_application_observer.WaitPoliciesApplied(user_hash);
    }
  }

  void OnConnectToServiceFailed(base::OnceClosure run_loop_quit_closure,
                                const std::string& error_name,
                                const std::string& error_message) {
    ADD_FAILURE() << "Connect failed with " << error_name << " - "
                  << error_message;
    std::move(run_loop_quit_closure).Run();
  }

  void ConnectToService(const std::string& service_path) {
    base::RunLoop run_loop;
    ash::ShillServiceClient::Get()->Connect(
        dbus::ObjectPath(service_path), run_loop.QuitClosure(),
        base::BindOnce(&NetworkPolicyApplicationTest::OnConnectToServiceFailed,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Imports the certificate and key described by the |cert_filename| and
  // |key_filename| files in |source_dir| to the system token (device-wide).
  // Then triggers NetworkCertLoader to re-load its certificates cache.
  // Should be wrapped in ASSERT_NO_FATAL_FAILURE.
  void ImportCert(const base::FilePath& source_dir,
                  const std::string& cert_filename,
                  const std::string& key_filename) {
    // Before importing, configure NetworkCertLoader to assume that all
    // certificates can be used for network authentication.
    ash::NetworkCertLoader::Get()->ForceAvailableForNetworkAuthForTesting();

    net::ScopedCERTCertificate cert;
    // Import testing key pair and certificate.
    {
      base::ScopedAllowBlockingForTesting allow_io;
      net::ImportClientCertAndKeyFromFile(
          source_dir, cert_filename, key_filename,
          system_nss_key_slot_mixin_.slot(), &cert);
    }
    ASSERT_TRUE(cert);

    // Trigger refreshing the NetworkCertLoader's cache so the certificate
    // becomes available for networks. Production code does this through
    // NSSCertDatabase::ImportUserCert.
    ScopedNetworkCertLoaderRefreshWaiter network_cert_loader_refresh_waiter;
    net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
    network_cert_loader_refresh_waiter.Wait();
  }

  // Applies `properties` to the network identified by `guid` using
  // CrosNetworkConfig.
  void CrosNetworkConfigSetProperties(
      const std::string& guid,
      network_mojom::ConfigPropertiesPtr properties) {
    base::test::TestFuture<bool, std::string> set_properties_future;
    cros_network_config_->SetProperties(
        guid, std::move(properties),
        set_properties_future.GetCallback<bool, const std::string&>());
    ASSERT_TRUE(set_properties_future.Wait());
    ASSERT_TRUE(set_properties_future.Get<bool>())
        << "Error msg: " << set_properties_future.Get<std::string>();
  }

  // Retrieves the "managed properties" of the network identified by `guid`
  // using CrosNetworkConfig.
  network_mojom::ManagedPropertiesPtr CrosNetworkConfigGetManagedProperties(
      const std::string& guid) {
    base::test::TestFuture<network_mojom::ManagedPropertiesPtr>
        get_managed_properties_future;
    cros_network_config_->GetManagedProperties(
        guid, get_managed_properties_future.GetCallback());
    return get_managed_properties_future.Take();
  }

  network_mojom::GlobalPolicyPtr CrosNetworkConfigGetGlobalPolicy() {
    base::test::TestFuture<network_mojom::GlobalPolicyPtr> global_policy_ptr;
    cros_network_config_->GetGlobalPolicy(global_policy_ptr.GetCallback());
    return global_policy_ptr.Take();
  }

  network_mojom::NetworkStatePropertiesPtr
  CrosNetworkConfigGetNetworkStateProps(const std::string& guid) {
    base::test::TestFuture<network_mojom::NetworkStatePropertiesPtr>
        network_state_props_ptr;
    cros_network_config_->GetNetworkState(
        guid, network_state_props_ptr.GetCallback());
    return network_state_props_ptr.Take();
  }

  network_mojom::NetworkCertificatePtr CrosNetworkConfigFindClientCert(
      const std::string& ui_name) {
    ash::network_config::CrosNetworkConfig cros_network_config;
    base::test::TestFuture<std::vector<network_mojom::NetworkCertificatePtr>,
                           std::vector<network_mojom::NetworkCertificatePtr>>
        future;
    cros_network_config.GetNetworkCertificates(future.GetCallback());
    std::vector<network_mojom::NetworkCertificatePtr> client_certs =
        std::get<1>(future.Take());
    for (auto& client_cert : client_certs) {
      if (client_cert->issued_to == ui_name) {
        return std::move(client_cert);
      }
    }
    return {};
  }

  // Extracts the UIData dictionary from the shill UIData property of the
  // service `service_path`.
  std::optional<base::Value::Dict> GetUIDataDict(
      const std::string& service_path) {
    const base::Value::Dict* properties =
        shill_service_client_test_->GetServiceProperties(service_path);
    if (!properties)
      return {};
    const std::string* ui_data_json =
        properties->FindString(shill::kUIDataProperty);
    if (!ui_data_json)
      return {};
    std::optional<base::Value> ui_data_value =
        base::JSONReader::Read(*ui_data_json);
    if (!ui_data_value || !ui_data_value->is_dict())
      return {};
    return std::move(*ui_data_value).TakeDict();
  }

  // Sets the shill UIData property of the service `service_path` to the
  // serialized `ui_data_dict`.
  void SetUIDataDict(const std::string& service_path,
                     const base::Value::Dict& ui_data_dict) {
    std::string ui_data_json;
    base::JSONWriter::Write(ui_data_dict, &ui_data_json);
    shill_service_client_test_->SetServiceProperty(
        service_path, shill::kUIDataProperty, base::Value(ui_data_json));
  }

  // Returns the GUID from the "user_settings" from `ui_data` or an empty string
  // of no "user_settings" or no GUID was found.
  std::string GetGUIDFromUIData(const base::Value::Dict& ui_data) {
    const base::Value::Dict* user_settings =
        ui_data.FindDict(kUIDataKeyUserSettings);
    if (!user_settings)
      return std::string();
    const std::string* guid =
        user_settings->FindString(::onc::network_config::kGUID);
    if (!guid)
      return std::string();
    return *guid;
  }

  const base::Value::Dict* GetWifiProps(const std::string& guid) {
    std::optional<std::string> wifi_service;
    wifi_service = shill_service_client_test_->FindServiceMatchingGUID(guid);
    if (wifi_service->empty()) {
      ADD_FAILURE() << "No wifi service found for: " << guid;
    }
    return shill_service_client_test_->GetServiceProperties(
        wifi_service.value());
  }

  void SetServiceVisibility(const std::string& guid, bool visible) {
    std::optional<std::string> wifi_service;
    wifi_service = shill_service_client_test_->FindServiceMatchingGUID(guid);
    if (wifi_service->empty()) {
      ADD_FAILURE() << "No wifi service found for: " << guid;
      return;
    }
    shill_service_client_test_->SetServiceProperty(
        wifi_service.value(), shill::kVisibleProperty, base::Value(visible));
  }

  const std::string GetTestUserHash() {
    return user_manager::UserManager::Get()
        ->FindUser(test_account_id_)
        ->username_hash();
  }

  bool IsProhibitedByPolicyInCrosNetworkConfig(const std::string& guid) {
    return CrosNetworkConfigGetNetworkStateProps(guid)->prohibited_by_policy;
  }

  const std::string GetWifiStateFromShillClient(const std::string& guid) {
    const base::Value::Dict* wifi_properties = GetWifiProps(guid);
    const std::string* wifi_state =
        wifi_properties->FindString(shill::kStateProperty);
    if (!wifi_state) {
      ADD_FAILURE() << "Network has no WiFi state properties: " << guid;
      return "";
    }
    return *wifi_state;
  }

  void AddPskWifiService(const std::string& service_path,
                         const std::string& initial_guid,
                         const std::string& ssid,
                         const std::string& initial_state) {
    shill_service_client_test_->AddService(service_path, initial_guid, ssid,
                                           shill::kTypeWifi, initial_state,
                                           /*visible=*/true);
    shill_service_client_test_->SetServiceProperty(
        service_path, shill::kSSIDProperty, base::Value(ssid));
    shill_service_client_test_->SetServiceProperty(
        service_path, shill::kSecurityClassProperty,
        base::Value(shill::kSecurityClassPsk));
  }

  void Add8021xWifiService(const std::string& service_path,
                           const std::string& initial_guid,
                           const std::string& ssid,
                           const std::string& initial_state) {
    shill_service_client_test_->AddService(service_path, initial_guid, ssid,
                                           shill::kTypeWifi, initial_state,
                                           /*visible=*/true);
    shill_service_client_test_->SetServiceProperty(
        service_path, shill::kSSIDProperty, base::Value(ssid));
    shill_service_client_test_->SetServiceProperty(
        service_path, shill::kSecurityClassProperty,
        base::Value(shill::kSecurityClass8021x));
  }

  // Adds a pre-existing PEAP-MSCHAPv2 service into the shared profile that is
  // marked as originating from device policy.
  void AddSharedDevicePolicyMschapv2Service(
      const std::string& service_path,
      const std::string& initial_guid,
      const std::string& ssid,
      const std::string& initial_identity) {
    Add8021xWifiService(service_path, initial_guid, "wifi1", shill::kStateIdle);
    shill_profile_client_test_->AddService(kSharedProfilePath, service_path);
    shill_service_client_test_->SetServiceProperty(
        service_path, shill::kEapMethodProperty, base::Value("PEAP"));
    shill_service_client_test_->SetServiceProperty(
        service_path, shill::kEapIdentityProperty,
        base::Value(initial_identity));
    shill_service_client_test_->SetServiceProperty(
        service_path, shill::kUIDataProperty,
        base::Value(R"({"onc_source":"device_policy"})"));
  }

  void SimulateWifiScanCompleted() {
    shill_device_client_test_->SetDeviceProperty(
        kDeviceWifiPath, shill::kScanningProperty, base::Value(true),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
    shill_device_client_test_->SetDeviceProperty(
        kDeviceWifiPath, shill::kScanningProperty, base::Value(false),
        /*notify_changed=*/true);
  }

  // Unowned pointers -- just pointers to the singleton instances.
  raw_ptr<ash::ShillManagerClient::TestInterface, DanglingUntriaged>
      shill_manager_client_test_ = nullptr;
  raw_ptr<ash::ShillServiceClient::TestInterface, DanglingUntriaged>
      shill_service_client_test_ = nullptr;
  raw_ptr<ash::ShillProfileClient::TestInterface, DanglingUntriaged>
      shill_profile_client_test_ = nullptr;
  raw_ptr<ash::ShillDeviceClient::TestInterface, DanglingUntriaged>
      shill_device_client_test_ = nullptr;

  ash::ScopedStubInstallAttributes stub_install_attributes_;

  AccountId test_account_id_;

  std::unique_ptr<ash::network_config::CrosNetworkConfig> cros_network_config_;

 private:
  ash::ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{&mixin_host_};
  ash::LoginManagerMixin login_mixin_{&mixin_host_};

  testing::NiceMock<MockConfigurationPolicyProvider> policy_provider_;
  PolicyMap current_policy_;
};

// A variant of NetworkPolicyApplicationTest which enables the
// EphemeralNetworkPolicies feature.
class NetworkPolicyApplicationEphemeralActionsEnabledTest
    : public NetworkPolicyApplicationTest {
 public:
  NetworkPolicyApplicationEphemeralActionsEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kEphemeralNetworkPolicies);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A variant of NetworkPolicyApplicationTest which disables the
// EphemeralNetworkPolicies feature.
class NetworkPolicyApplicationEphemeralActionsDisabledTest
    : public NetworkPolicyApplicationTest {
 public:
  NetworkPolicyApplicationEphemeralActionsDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::
                                  kEphemeralNetworkPoliciesEnabledPolicy},
        /*disabled_features=*/{ash::features::kEphemeralNetworkPolicies});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A variant of NetworkPolicyApplicationTest which enables the
// EphemeralNetworkPolicies feature and the device is not enterprise enrolled at
// startup.
class NetworkPolicyApplicationEphemeralActionsEnabledUnenrolledTest
    : public NetworkPolicyApplicationTest {
 public:
  NetworkPolicyApplicationEphemeralActionsEnabledUnenrolledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kEphemeralNetworkPolicies);
    stub_install_attributes_.Get()->Clear();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A variant of NetworkPolicyApplicationTest which disables the
// EphemeralNetworkPolicies and EphemeralNetworkPoliciesEnabledPolicy feature.
class NetworkPolicyApplicationEphemeralActionsKillSwitchTest
    : public NetworkPolicyApplicationTest {
 public:
  NetworkPolicyApplicationEphemeralActionsKillSwitchTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            ash::features::kEphemeralNetworkPolicies,
            ash::features::kEphemeralNetworkPoliciesEnabledPolicy});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test applies a global network policy with
// AllowOnlyPolicyNetworksToAutoconnect set to true. It then performs a user
// log-in and simulates that user policy application is slow. This is a
// regression test for https://crbug.com/936677.
// Specifically, it simulates that:
// 1. ash-chrome applies device network policy in shill.
//    The device policy mandates that only policy configured networks may
//    auto-connect.
// 2. The user manually connects to a non-policy-managed network
// 3. The user signs in and ash-chrome applies user network policy in shill.
//    Important:
//    shill does not reflect the property changes back to chrome through
//    D-Bus PropertyChanged events yet.
//    In the test, this is simulated by
//      shill_service_client_test_->SetHoldBackServicePropertyUpdates(true);
// In this case, the signal that policies have been applied yet may not be
// triggered yet.
// Only after shill is allowed to send PropertyChanged events to chrome will
// chrome's data models be updated, and then the "policies applied" signal
// should be triggered.
//
// This is checked in the test in two ways:
// - Direct observation of ash::NetworkPolicyObserver through
// ScopedNetworkPolicyApplicationObserver.
// - Checking that AutoConnectHandler didn't disconnect the manually-connected
//   network, which was an observable consequence of the bug in this setup.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       OnlyPolicyAutoconnectWithSlowUserPolicyApplication) {
  ScopedNetworkPolicyApplicationObserver network_policy_application_observer;

  // Set up two services.
  AddPskWifiService(kServiceWifi1, "wifi_orig_guid_1", "WifiOne",
                    shill::kStateIdle);
  AddPskWifiService(kServiceWifi2, "wifi_orig_guid_2", "WifiTwo",
                    shill::kStateIdle);

  // Apply device ONC policy and wait until it takes effect (one of the networks
  // auto connects).
  const char kDeviceONC[] = R"(
    {
      "GlobalNetworkConfiguration": {
        "AllowOnlyPolicyNetworksToAutoconnect": true,
        "AllowOnlyPolicyNetworksToConnect": false
      },
      "NetworkConfigurations": [
        {
          "GUID": "{device-policy-for-Wifi1}",
          "Name": "DeviceLevelWifi",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": true,
             "HiddenSSID": false,
             "Passphrase": "DeviceLevelWifiPwd",
             "SSID": "WifiOne",
             "Security": "WPA-PSK"
          }
        }
      ]
    })";
  ServiceStateWaiter wifi_one_connected_waiter(shill_service_client_test_,
                                               kServiceWifi1);
  shill_manager_client_test_->SetBestServiceToConnect(kServiceWifi1);
  SetDeviceOpenNetworkConfiguration(kDeviceONC, /*wait_applied=*/true);
  wifi_one_connected_waiter.Wait(shill::kStateOnline);

  EXPECT_THAT(
      network_policy_application_observer.policy_applied_to_network_events(),
      ElementsAre(kServiceWifi1));
  EXPECT_THAT(network_policy_application_observer.policies_applied_events(),
              ElementsAre(std::string() /* shill shared profile */));
  network_policy_application_observer.ResetEvents();

  std::optional<std::string> wifi_service =
      shill_service_client_test_->FindServiceMatchingGUID(
          "{device-policy-for-Wifi1}");
  ASSERT_TRUE(wifi_service);
  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(wifi_service.value());
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kAutoConnectProperty, base::Value(true)));
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kProfileProperty,
                                   base::Value(kSharedProfilePath)));
  }

  // Manually connect to the other network.
  ConnectToService(kServiceWifi2);

  // Sign-in a user and apply user ONC policy. Simulate that shill takes a while
  // to reflect the changes back to chrome by holding back service property
  // updates (regression test for https://crbug.com/936677).
  shill_service_client_test_->SetHoldBackServicePropertyUpdates(true);

  LoginUser(test_account_id_);
  const std::string user_hash = user_manager::UserManager::Get()
                                    ->FindUser(test_account_id_)
                                    ->username_hash();
  shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);

  // When AutoConnectHandler triggers ScanAndConnectToBestServices, shill should
  // not do anything for now. This allows us to test whether AutoConnectHandler
  // is explicitly disconnecting networks.
  shill_manager_client_test_->SetBestServiceToConnect(std::string());
  const char kUserONC[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{user-policy-for-Wifi1}",
          "Name": "DeviceLevelWifi",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": false,
             "HiddenSSID": false,
             "Passphrase": "DeviceLevelWifiPwd",
             "SSID": "WifiOne",
             "Security": "WPA-PSK"
          }
        },
        {
          "GUID": "{user-policy-for-Wifi2}",
          "Name": "UserLevelWifi",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": true,
             "HiddenSSID": false,
             "Passphrase": "UserLevelWifiPwd",
             "SSID": "WifiTwo",
             "Security": "WPA-PSK"
          }
        }
      ]
    })";
  SetUserOpenNetworkConfiguration(user_hash, kUserONC, /*wait_applied=*/false);

  // Expect that the policies have not been signalled as applied yet because
  // property updates are being held back by FakeShillServiceClient.
  EXPECT_TRUE(
      network_policy_application_observer.policy_applied_to_network_events()
          .empty());
  EXPECT_TRUE(
      network_policy_application_observer.policies_applied_events().empty());

  // Now let fake shill reflect the property updates, so policy application is
  // marked as done.
  shill_service_client_test_->SetHoldBackServicePropertyUpdates(false);
  network_policy_application_observer.WaitPoliciesApplied(user_hash);
  EXPECT_THAT(
      network_policy_application_observer.policy_applied_to_network_events(),
      ElementsAre(kServiceWifi1, kServiceWifi2));

  // Expect that the same service path now has the user policy GUID.
  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(wifi_service.value());
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kGuidProperty,
                                   base::Value("{user-policy-for-Wifi1}")));
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kAutoConnectProperty, base::Value(false)));
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kProfileProperty,
                                   base::Value(kUserProfilePath)));
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kStateProperty,
                                   base::Value(shill::kStateIdle)));
  }

  std::optional<std::string> wifi2_service =
      shill_service_client_test_->FindServiceMatchingGUID(
          "{user-policy-for-Wifi2}");
  ASSERT_TRUE(wifi2_service);
  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(wifi2_service.value());
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kAutoConnectProperty, base::Value(true)));
    // This service is still connected. This is an important check in this
    // regression test:
    // In https://crbug.com/936677, AutoConnectHandler was already running
    // (because OnPoliciesApplied was already triggered) when the NetworkState
    // for a policy-managed network was not marked managed yet (because shill
    // has not reflected the property changes yet). As a consequence,
    // AutoConnectHandler disconnected the current network because of the global
    // AllowOnlyPolicyNetworksToAutoconnect policy. Verify that this has not
    // happened in this test.
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kStateProperty,
                                   base::Value(shill::kStateOnline)));
  }
}

// Verify that AllowOnlyPolicyNetworksToConnect is working correctly , so
// non-policy-managed networks can be connected before user's login, but
// they will be automatically disconnected after the user's login and they also
// will be forbidden in CrosNetworkConfig.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       OnlyPolicyAllowedNetworkCanBeConected) {
  ScopedNetworkPolicyApplicationObserver policy_observer;
  ash::ShillServiceClient::TestInterface* shill_client =
      shill_service_client_test_;
  constexpr char kGuidWifi1[] = "wifi_orig_guid_1";
  constexpr char kGuidWifi2[] = "wifi_orig_guid_2";
  constexpr char kGuidWifi3[] = "wifi_orig_guid_3";
  constexpr char kGuidWifi4[] = "wifi_orig_guid_4";

  // Network 1 will be managed by device policy and it will have auto connect.
  // Network 2 will be managed by user policy.
  // Network 3 and 4 will be not managed. Network 4 is added just to make sure
  // that we have more than one un-managed network.
  CrosNetworkConfigGuidsAvailableWaiter available_waiter(
      cros_network_config_.get(),
      {kGuidWifi1, kGuidWifi2, kGuidWifi3, kGuidWifi4});
  AddPskWifiService(kServiceWifi1, kGuidWifi1, "WifiOne", shill::kStateIdle);
  AddPskWifiService(kServiceWifi2, kGuidWifi2, "WifiTwo", shill::kStateIdle);
  AddPskWifiService(kServiceWifi3, kGuidWifi3, "WifiThree", shill::kStateIdle);
  AddPskWifiService(kServiceWifi4, kGuidWifi4, "WifiFour", shill::kStateIdle);
  available_waiter.Wait();

  // Check that initially no policies applied and CrosNetworkStateProperties
  // has no prohibited networks.
  {
    EXPECT_FALSE(CrosNetworkConfigGetGlobalPolicy()
                     ->allow_only_policy_wifi_networks_to_connect);

    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi3));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi4));
  }

  // Apply device ONC policy and wait until it takes effect (1st network
  // start auto connects). Those setting are not effective before user's login,
  // but we can check that CrosNetworkStateProperties are updated correctly.
  {
    const char kDeviceONC[] = R"(
        {
          "GlobalNetworkConfiguration": {
            "AllowOnlyPolicyNetworksToConnect": true
          },
          "NetworkConfigurations": [
            {
              "GUID": "wifi_orig_guid_1",
              "Name": "DeviceLevelWifi",
              "Type": "WiFi",
              "WiFi": {
                "AutoConnect": true,
                "HiddenSSID": false,
                "Passphrase": "DeviceLevelWifiPwd",
                "SSID": "WifiOne",
                "Security": "WPA-PSK"
              }
            }
          ]
        })";

    ServiceStateWaiter wifi_one_connected_waiter(shill_client, kServiceWifi1);
    shill_manager_client_test_->SetBestServiceToConnect(kServiceWifi1);
    SetDeviceOpenNetworkConfiguration(kDeviceONC, /*wait_applied=*/true);
    wifi_one_connected_waiter.Wait(shill::kStateOnline);
  }

  // Check before login that device can connect to any available network.
  // This is because AllowOnlyPolicyNetworksToConnect only applies in user's
  // sessions, even though it is a device-wide network policy.
  // WiFi3 will be kept online and it should be disconnected automatically after
  // the login because policy will take action, WiFi1 will become connected
  // after the login because it has "autoconnect".
  {
    // Verify that GlobalPolicy from CrosNetworkConfig will allow connection
    // only to networks defined and all networks are not prohibited.
    EXPECT_TRUE(CrosNetworkConfigGetGlobalPolicy()
                    ->allow_only_policy_wifi_networks_to_connect);

    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi3));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi4));

    // Check that device is connected to WiFi1 while other networks (2,3,4)
    // are "idle".
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateOnline);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi3), shill::kStateIdle);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi4), shill::kStateIdle);

    // Manually connect to the 2nd network and verify that it is "online"
    // while network 1, 3, 4 are "idle".
    ConnectToService(kServiceWifi2);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateIdle);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateOnline);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi3), shill::kStateIdle);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi4), shill::kStateIdle);

    // Manually connect to the 3d network and verify that it is "online"
    // network 1, 2, 4 are "idle".
    ConnectToService(kServiceWifi3);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateIdle);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi3), shill::kStateOnline);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi4), shill::kStateIdle);
  }

  // Sign-in a user and apply user ONC policy for WiFi2.
  {
    LoginUser(test_account_id_);
    const std::string user_hash = GetTestUserHash();
    shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);

    const char kUserONC[] = R"(
        {
          "NetworkConfigurations": [
            {
              "GUID": "wifi_orig_guid_2",
              "Name": "UserLevelWifi",
              "Type": "WiFi",
              "WiFi": {
                "AutoConnect": true,
                "HiddenSSID": false,
                "Passphrase": "UserLevelWifiPwd",
                "SSID": "WifiTwo",
                "Security": "WPA-PSK"
              }
            }
          ]
        })";

    SetUserOpenNetworkConfiguration(user_hash, kUserONC,
                                    /*wait_applied=*/true);
  }

  // Verify that WiFi3 is disconnected as it is not included into device or
  // into user's policies. Also now WiFi3 and WiFi4 are prohibited
  // by policy. WiFi1 should be connected (online) because it has auto connect.
  {
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateOnline);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi3), shill::kStateIdle);
    EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi4), shill::kStateIdle);

    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
    EXPECT_TRUE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi3));
    EXPECT_TRUE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi4));
  }
}

// Tests behavior of BlockedHexSSIDs on login screen and in a user session.
// Also tests that if a blocked SSID was connected on the sign-in screen, it is
// disconnected when a user signs in.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest, BlockedHexSSIDs) {
  constexpr char kGuidWifi1[] = "wifi_orig_guid_1";
  constexpr char kGuidWifi2[] = "wifi_orig_guid_2";

  CrosNetworkConfigGuidsAvailableWaiter available_waiter(
      cros_network_config_.get(), {kGuidWifi1, kGuidWifi2});
  AddPskWifiService(kServiceWifi1, kGuidWifi1, "WifiOne", shill::kStateIdle);
  AddPskWifiService(kServiceWifi2, kGuidWifi2, "WifiTwo", shill::kStateIdle);
  available_waiter.Wait();

  // Check that initially no policies applied and CrosNetworkStateProperties
  // has no prohibited networks.
  {
    EXPECT_THAT(CrosNetworkConfigGetGlobalPolicy()->blocked_hex_ssids,
                IsEmpty());

    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  }

  // Apply device ONC policy.
  // 576966694F6E65 is hex-encoded ASCII "WifiOne"
  {
    const char kDeviceONC[] = R"(
        {
          "GlobalNetworkConfiguration": {
            "BlockedHexSSIDs": [ "576966694F6E65" ]
          },
          "NetworkConfigurations": [ ]
        })";
    SetDeviceOpenNetworkConfiguration(kDeviceONC, /*wait_applied=*/true);

    EXPECT_THAT(CrosNetworkConfigGetGlobalPolicy()->blocked_hex_ssids,
                ElementsAre("576966694F6E65"));
  }

  // The network is still connectable because BlockedHexSSIDs is not applied on
  // the sign-in screen.
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));

  ConnectToService(kServiceWifi1);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateOnline);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);

  // Sign-in a user and apply user without ONC policy.
  // The blocked network should be automatically disconnected.
  {
    ServiceStateWaiter wifi_disconnected_waiter(shill_service_client_test_,
                                                kServiceWifi1);

    LoginUser(test_account_id_);
    const std::string user_hash = GetTestUserHash();
    shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);

    wifi_disconnected_waiter.Wait(shill::kStateIdle);
  }

  EXPECT_TRUE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));

  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateIdle);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);

  // TODO(b/277809215): Attempt to connect to the prohibited service using
  // CrosNetworkConfig.
}

// Behavior of AllowOnlyPolicyNetworksToConnectIfAvailable when no policy
// networks are configured.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       OnlyPolicyIfAvailable_NotConfigured) {
  constexpr char kGuidWifi1[] = "wifi_orig_guid_1";
  constexpr char kGuidWifi2[] = "wifi_orig_guid_2";

  CrosNetworkConfigGuidsAvailableWaiter available_waiter(
      cros_network_config_.get(), {kGuidWifi1, kGuidWifi2});
  AddPskWifiService(kServiceWifi1, kGuidWifi1, "WifiOne", shill::kStateIdle);
  AddPskWifiService(kServiceWifi2, kGuidWifi2, "WifiTwo", shill::kStateIdle);
  available_waiter.Wait();

  // Check that initially no policies applied and CrosNetworkStateProperties
  // has no prohibited networks.
  {
    EXPECT_FALSE(CrosNetworkConfigGetGlobalPolicy()
                     ->allow_only_policy_wifi_networks_to_connect);

    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  }

  // Apply device ONC policy
  {
    const char kDeviceONC[] = R"(
        {
          "GlobalNetworkConfiguration": {
            "AllowOnlyPolicyNetworksToConnectIfAvailable": true
          },
          "NetworkConfigurations": [ ]
        })";
    SetDeviceOpenNetworkConfiguration(kDeviceONC, /*wait_applied=*/true);

    EXPECT_TRUE(CrosNetworkConfigGetGlobalPolicy()
                    ->allow_only_policy_wifi_networks_to_connect_if_available);
  }

  // Manually connecting to a non-policy network must be possible.
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));

  ConnectToService(kServiceWifi1);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateOnline);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);

  // Sign-in a user and apply user without ONC policy.
  {
    LoginUser(test_account_id_);
    const std::string user_hash = GetTestUserHash();
    shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);
  }

  // WiFi1 should still be connected.
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));

  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateOnline);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);
}

// Behavior of AllowOnlyPolicyNetworksToConnectIfAvailable when no policy
// networks are visible initially, then after user login user login, a
// policy-provided network becomes visible. After that the policy-provided
// network becomes not visible again.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       OnlyPolicyIfAvailable_VisibleBackAndForth) {
  constexpr char kGuidWifi1[] = "wifi_orig_guid_1";
  constexpr char kGuidWifi2[] = "wifi_orig_guid_2";

  CrosNetworkConfigGuidsAvailableWaiter available_waiter(
      cros_network_config_.get(), {kGuidWifi1, kGuidWifi2});
  AddPskWifiService(kServiceWifi1, kGuidWifi1, "WifiOne", shill::kStateIdle);
  AddPskWifiService(kServiceWifi2, kGuidWifi2, "WifiTwo", shill::kStateIdle);
  available_waiter.Wait();

  // Check that initially no policies applied and CrosNetworkStateProperties
  // has no prohibited networks.
  {
    EXPECT_FALSE(CrosNetworkConfigGetGlobalPolicy()
                     ->allow_only_policy_wifi_networks_to_connect);

    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  }

  // Apply device ONC policy
  {
    const char kDeviceONC[] = R"(
        {
          "GlobalNetworkConfiguration": {
            "AllowOnlyPolicyNetworksToConnectIfAvailable": true
          },
          "NetworkConfigurations": [
            {
              "GUID": "wifi_policy_1",
              "Name": "PolicyDeviceLevelWifi",
              "Type": "WiFi",
              "WiFi": {
                "HiddenSSID": false,
                "Passphrase": "DeviceLevelWifiPwd",
                "SSID": "PolicyDeviceLevelWifi",
                "Security": "WPA-PSK"
              }
            }
          ]
        })";
    SetDeviceOpenNetworkConfiguration(kDeviceONC, /*wait_applied=*/true);

    EXPECT_TRUE(CrosNetworkConfigGetGlobalPolicy()
                    ->allow_only_policy_wifi_networks_to_connect_if_available);
  }

  // Manually connecting to a non-policy network must be possible.
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_1"));

  ConnectToService(kServiceWifi1);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateOnline);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);
  EXPECT_EQ(GetWifiStateFromShillClient("wifi_policy_1"), shill::kStateIdle);

  // Sign-in a user and apply user ONC policy for another unavailable network.
  {
    LoginUser(test_account_id_);
    const std::string user_hash = GetTestUserHash();
    shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);

    const char kUserONC[] = R"(
        {
          "NetworkConfigurations": [
            {
              "GUID": "wifi_policy_2",
              "Name": "UserLevelWifi",
              "Type": "WiFi",
              "WiFi": {
                "HiddenSSID": false,
                "Passphrase": "UserLevelWifiPwd",
                "SSID": "PolicyUserLevelWifi",
                "Security": "WPA-PSK"
              }
            }
          ]
        })";

    SetUserOpenNetworkConfiguration(user_hash, kUserONC,
                                    /*wait_applied=*/true);
  }

  // WiFi1 should still be connected.
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_1"));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_2"));

  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateOnline);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);
  EXPECT_EQ(GetWifiStateFromShillClient("wifi_policy_1"), shill::kStateIdle);
  EXPECT_EQ(GetWifiStateFromShillClient("wifi_policy_2"), shill::kStateIdle);

  {
    // Now the policy-provided network becomes visible in a wifi scan.
    // Expect that wifi_policy_2 connects.
    std::optional<std::string> user_policy_wifi_service_path =
        shill_service_client_test_->FindServiceMatchingGUID("wifi_policy_2");
    ASSERT_TRUE(user_policy_wifi_service_path);
    ServiceStateWaiter wifi_connected_waiter(
        shill_service_client_test_, user_policy_wifi_service_path.value());
    SetServiceVisibility("wifi_policy_2", true);
    SimulateWifiScanCompleted();

    wifi_connected_waiter.Wait(shill::kStateOnline);
  }

  // Expects that the non-policy WiFi services are now prohibited and that the
  // policy-provided network has connected.
  EXPECT_TRUE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_TRUE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_1"));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_2"));

  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateIdle);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);
  EXPECT_EQ(GetWifiStateFromShillClient("wifi_policy_1"), shill::kStateIdle);
  EXPECT_EQ(GetWifiStateFromShillClient("wifi_policy_2"), shill::kStateOnline);

  // Now the policy-provided network becomes invisible again, and no network is
  // prohibited anymore.
  SetServiceVisibility("wifi_policy_2", false);
  SimulateWifiScanCompleted();

  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_1"));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_2"));
}

// Behavior of AllowOnlyPolicyNetworksToConnectIfAvailable when a device policy
// network is visible on user login.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       OnlyPolicyIfAvailable_DeviceNetworkVisibleOnLogin) {
  constexpr char kGuidWifi1[] = "wifi_orig_guid_1";
  constexpr char kGuidWifi2[] = "wifi_orig_guid_2";

  CrosNetworkConfigGuidsAvailableWaiter available_waiter(
      cros_network_config_.get(), {kGuidWifi1, kGuidWifi2});
  AddPskWifiService(kServiceWifi1, kGuidWifi1, "WifiOne", shill::kStateIdle);
  AddPskWifiService(kServiceWifi2, kGuidWifi2, "WifiTwo", shill::kStateIdle);
  available_waiter.Wait();

  // Check that initially no policies applied and CrosNetworkStateProperties
  // has no prohibited networks.
  {
    EXPECT_FALSE(CrosNetworkConfigGetGlobalPolicy()
                     ->allow_only_policy_wifi_networks_to_connect);

    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
    EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  }

  // Apply device ONC policy
  {
    const char kDeviceONC[] = R"(
        {
          "GlobalNetworkConfiguration": {
            "AllowOnlyPolicyNetworksToConnectIfAvailable": true
          },
          "NetworkConfigurations": [
            {
              "GUID": "wifi_policy_1",
              "Name": "PolicyDeviceLevelWifi",
              "Type": "WiFi",
              "WiFi": {
                "HiddenSSID": false,
                "Passphrase": "DeviceLevelWifiPwd",
                "SSID": "PolicyDeviceLevelWifi",
                "Security": "WPA-PSK"
              }
            }
          ]
        })";
    SetDeviceOpenNetworkConfiguration(kDeviceONC, /*wait_applied=*/true);

    EXPECT_TRUE(CrosNetworkConfigGetGlobalPolicy()
                    ->allow_only_policy_wifi_networks_to_connect_if_available);
  }
  // Make the device policy network visible.
  SetServiceVisibility("wifi_policy_1", true);

  // Manually connecting to a non-policy network must be possible (this is still
  // on the sign-in screen).
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_1"));

  ConnectToService(kServiceWifi1);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi1), shill::kStateOnline);
  EXPECT_EQ(GetWifiStateFromShillClient(kGuidWifi2), shill::kStateIdle);
  EXPECT_EQ(GetWifiStateFromShillClient("wifi_policy_1"), shill::kStateIdle);

  // Sign-in a user. The device policy network should connect because the
  // AllowOnlyPolicyNetworksToConnectIfAvailable became effective on user login.
  {
    std::optional<std::string> policy_wifi_service_path =
        shill_service_client_test_->FindServiceMatchingGUID("wifi_policy_1");
    ASSERT_TRUE(policy_wifi_service_path);
    ServiceStateWaiter wifi_connected_waiter(shill_service_client_test_,
                                             policy_wifi_service_path.value());

    shill_manager_client_test_->SetBestServiceToConnect(
        policy_wifi_service_path.value());
    LoginUser(test_account_id_);
    const std::string user_hash = GetTestUserHash();
    shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);

    wifi_connected_waiter.Wait(shill::kStateOnline);
  }

  // Expects that the non-policy WiFi services are now prohibited.
  EXPECT_TRUE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi1));
  EXPECT_TRUE(IsProhibitedByPolicyInCrosNetworkConfig(kGuidWifi2));
  EXPECT_FALSE(IsProhibitedByPolicyInCrosNetworkConfig("wifi_policy_1"));
}

// Checks the edge case where a policy with GUID {same_guid} applies to network
// with SSID "WifiTwo", and subsequently the policy changes, the new
// NetworkConfiguration with GUID {same_guid} now applying to SSID "WifiOne".
// For this to work correctly, PolicyApplicator must first clear the "WifiTwo"
// settings so it is not matched by GUID, and then write the new policy to
// shill.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       PolicyWithSameGUIDAppliesToOtherSSID) {
  // Set up two services.
  AddPskWifiService(kServiceWifi1, "wifi_orig_guid_1", "WifiOne",
                    shill::kStateIdle);
  AddPskWifiService(kServiceWifi2, "wifi_orig_guid_2", "WifiTwo",
                    shill::kStateIdle);

  const char kDeviceONC1[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{same_guid}",
          "Name": "X",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": false,
             "HiddenSSID": false,
             "Passphrase": "Passphrase",
             "SSID": "WifiTwo",
             "Security": "WPA-PSK"
          }
        }
      ]
    })";
  SetDeviceOpenNetworkConfiguration(kDeviceONC1, /*wait_applied=*/true);

  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi2);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kGuidProperty, base::Value("{same_guid}")));
  }

  // Same GUID for a different SSID.
  const char kDeviceONC2[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{same_guid}",
          "Name": "X",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": false,
             "HiddenSSID": false,
             "Passphrase": "SomePassphrase",
             "SSID": "WifiOne",
             "Security": "WPA-PSK"
          }
        }
      ]
    })";
  SetDeviceOpenNetworkConfiguration(kDeviceONC2, /*wait_applied=*/true);
  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi2);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_FALSE(wifi_service_properties->Find(shill::kGuidProperty));
  }
  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kGuidProperty, base::Value("{same_guid}")));
  }
}

// Tests that application of policy settings does not wipe an already-configured
// client certificate. This is a regression test for b/203015922.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest, DoesNotWipeCertSettings) {
  const char* kCertKeyFilename = "client_3.pk8";
  const char* kCertFilename = "client_3.pem";
  const char* kCertIssuerCommonName = "E CA";
  ASSERT_NO_FATAL_FAILURE(ImportCert(net::GetTestCertsDirectory(),
                                     kCertFilename, kCertKeyFilename));

  // Set up a policy-managed EAP wifi with a certificate already selected.
  Add8021xWifiService(kServiceWifi1, "DeviceLevelWifiGuidOrig",
                      "DeviceLevelWifiSsid", shill::kStateOnline);

  ServicePropertyValueWatcher eap_cert_id_watcher(
      shill_service_client_test_, kServiceWifi1, shill::kEapCertIdProperty);
  ServicePropertyValueWatcher eap_key_id_watcher(
      shill_service_client_test_, kServiceWifi1, shill::kEapKeyIdProperty);
  ServicePropertyValueWatcher eap_identity_watcher(
      shill_service_client_test_, kServiceWifi1, shill::kEapIdentityProperty);

  SetDeviceOpenNetworkConfiguration(
      OncPolicyToSelectClientCert("DeviceLevelWifiGuid", "DeviceLevelWifiSsid",
                                  /*issuer_common_name=*/kCertIssuerCommonName,
                                  /*identity=*/"identity_1"),
      /*wait_applied=*/true);

  // Verify that the EAP.CertId and EAP.KeyId properties are present and not
  // empty, i.e. that a client certificate has been selected.
  ASSERT_THAT(eap_cert_id_watcher.GetValues(), ElementsAre(Not(IsEmpty())));
  ASSERT_THAT(eap_key_id_watcher.GetValues(), ElementsAre(Not(IsEmpty())));
  std::string orig_eap_cert_id = eap_cert_id_watcher.GetValues().back();
  std::string orig_eap_key_id = eap_key_id_watcher.GetValues().back();

  EXPECT_THAT(eap_identity_watcher.GetValues(), ElementsAre("identity_1"));

  SetDeviceOpenNetworkConfiguration(
      OncPolicyToSelectClientCert("DeviceLevelWifiGuid", "DeviceLevelWifiSsid",
                                  /*issuer_common_name=*/kCertIssuerCommonName,
                                  /*identity=*/"identity_2"),
      /*wait_applied=*/true);

  // Verify that the EAP.CertId and EAP.KeyId properties have not been changed
  // to anything else (also not an empty string).
  ASSERT_THAT(eap_cert_id_watcher.GetValues(), ElementsAre(orig_eap_cert_id));
  ASSERT_THAT(eap_key_id_watcher.GetValues(), ElementsAre(orig_eap_key_id));
  EXPECT_THAT(eap_identity_watcher.GetValues(),
              ElementsAre("identity_1", "identity_2"));
}

// Configures a device-wide network that uses variable expansions
// (https://chromium.googlesource.com/chromium/src/+/main/components/onc/docs/onc_spec.md#string-expansions)
// and then tests that these variables are replaced with their values in the
// config pushed to shill.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       DevicePolicyProfileWideVariableExpansions) {
  const std::string kSerialNumber = "test_serial";
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kSerialNumber);

  Add8021xWifiService(kServiceWifi1, "DeviceLevelWifiGuidOrig",
                      "DeviceLevelWifiSsid", shill::kStateIdle);

  SetDeviceOpenNetworkConfiguration(
      OncPolicyToSelectClientCert("{DeviceLevelWifiGuid}",
                                  "DeviceLevelWifiSsid",
                                  /*issuer_common_name=*/"Example Inc.",
                                  /*identity=*/"${DEVICE_SERIAL_NUMBER}"),
      /*wait_applied=*/true);

  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kGuidProperty,
                                   base::Value("{DeviceLevelWifiGuid}")));
    // Expect that the EAP.Identity has been replaced
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kEapIdentityProperty,
                                   base::Value(kSerialNumber)));

    // TODO(b/209084821): Also test DEVICE_ASSET_ID when it's easily
    // configurable in a browsertest.
  }
}

// Configures a network that uses variable expansions with variables based on a
// client certificate selected using a CertificatePattern.
// The network is device-wide because that is easier to set up in the test.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       DevicePolicyCertBasedVariableExpansions) {
  const char* kCertKeyFilename = "client_3.pk8";
  const char* kCertFilename = "client_3.pem";
  const char* kCertIssuerCommonName = "E CA";
  const char* kIdentityPolicyValue =
      "${CERT_SUBJECT_COMMON_NAME}/${CERT_SAN_UPN}/${CERT_SAN_EMAIL}";
  const char* kExpectedIdentity =
      "Client Cert F/santest@ad.corp.example.com/santest@example.com";
  ASSERT_NO_FATAL_FAILURE(ImportCert(net::GetTestCertsDirectory(),
                                     kCertFilename, kCertKeyFilename));

  Add8021xWifiService(kServiceWifi1, "DeviceLevelWifiGuidOrig",
                      "DeviceLevelWifiSsid", shill::kStateIdle);

  SetDeviceOpenNetworkConfiguration(
      OncPolicyToSelectClientCert("{DeviceLevelWifiGuid}",
                                  "DeviceLevelWifiSsid",
                                  /*issuer_common_name=*/kCertIssuerCommonName,
                                  /*identity=*/kIdentityPolicyValue),
      /*wait_applied=*/true);
  ServicePropertyValueWatcher(shill_service_client_test_, kServiceWifi1,
                              shill::kEapCertIdProperty)
      .WaitForNonEmptyValue();

  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kGuidProperty,
                                   base::Value("{DeviceLevelWifiGuid}")));
    // Expect that the EAP.Identity has been replaced
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kEapIdentityProperty,
                                   base::Value(kExpectedIdentity)));
  }
}

// Configures a user-specific network that uses variable expansions
// (https://chromium.googlesource.com/chromium/src/+/main/components/onc/docs/onc_spec.md#string-expansions)
// and then tests that these variables are replaced with their values in the
// config pushed to shill.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       UserPolicyProfileWideVariableExpansions) {
  Add8021xWifiService(kServiceWifi1, "UserLevelWifiGuidOrig",
                      "UserLevelWifiSsid", shill::kStateIdle);

  LoginUser(test_account_id_);
  const std::string user_hash = user_manager::UserManager::Get()
                                    ->FindUser(test_account_id_)
                                    ->username_hash();
  shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);

  // Note that while a policy is used here that uses a ClientCertPattern, no
  // client certificate is resolved (because no such certificate was imported).
  // Still, the "user-specific variable" should be expanded.
  SetUserOpenNetworkConfiguration(
      user_hash,
      OncPolicyToSelectClientCert("{UserLevelWifiGuid}", "UserLevelWifiSsid",
                                  /*issuer_common_name=*/"Example Inc.",
                                  /*identity=*/"${LOGIN_EMAIL}"),
      /*wait_applied=*/true);

  {
    const base::Value::Dict* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kGuidProperty,
                                   base::Value("{UserLevelWifiGuid}")));
    // Expect that the EAP.Identity has been replaced
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kEapIdentityProperty,
                           base::Value(test_account_id_.GetUserEmail())));
  }
}

// Tests that re-applying Ethernet policy retains a manually-set IP address.
// This is a regression test for b/183676832 and b/180365271.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest, RetainEthernetIPAddr) {
  constexpr char kEthernetGuid[] = "{EthernetGuid}";

  shill_service_client_test_->AddService(kServiceEth, "orig_guid_ethernet_any",
                                         "ethernet_any", shill::kTypeEthernet,
                                         shill::kStateOnline, /*visible=*/true);

  {
    std::string kDeviceONCEverythingRecommended =
        base::StringPrintf(R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "%s",
          "Name": "EthernetName",
          "Type": "Ethernet",
          "Ethernet": {
             "Authentication": "None"
          },
          "StaticIPConfig": {
             "Recommended": ["Gateway", "IPAddress", "RoutingPrefix",
                             "NameServers"]
          },
          "Recommended": ["IPAddressConfigType", "NameServersConfigType"]
        }
      ]
    })",

                           kEthernetGuid);
    SetDeviceOpenNetworkConfiguration(kDeviceONCEverythingRecommended,
                                      /*wait_applied=*/true);
  }

  {
    const base::Value::Dict* eth_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceEth);
    ASSERT_TRUE(eth_service_properties);
    EXPECT_THAT(
        *eth_service_properties,
        DictionaryHasValue(shill::kGuidProperty, base::Value(kEthernetGuid)));
  }

  // Check that IP address is modifiable and policy-recommended.
  {
    auto properties = CrosNetworkConfigGetManagedProperties("{EthernetGuid}");
    ASSERT_TRUE(properties);
    EXPECT_EQ(properties->ip_address_config_type->policy_source,
              network_mojom::PolicySource::kDevicePolicyRecommended);
  }

  // Simulate setting an IP addr through the UI.
  {
    auto properties = network_mojom::ConfigProperties::New();
    properties->type_config =
        network_mojom::NetworkTypeConfigProperties::NewEthernet(
            network_mojom::EthernetConfigProperties::New());
    properties->ip_address_config_type =
        ::onc::network_config::kIPConfigTypeStatic;
    properties->static_ip_config = network_mojom::IPConfigProperties::New();
    properties->static_ip_config->ip_address = "192.168.1.44";
    properties->static_ip_config->gateway = "192.168.1.1";
    properties->static_ip_config->routing_prefix = 4;
    ASSERT_NO_FATAL_FAILURE(
        CrosNetworkConfigSetProperties(kEthernetGuid, std::move(properties)));
  }

  // Verify that the Static IP config has been applied.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(kServiceEth);
    ASSERT_TRUE(shill_properties);
    EXPECT_EQ(GetStaticIPAddressFromShillProperties(*shill_properties),
              "192.168.1.44");
  }

  {
    // Modify the policy: Force custom nameserver, but allow IP address to be
    // modifiable.
    std::string kDeviceONC2 = base::StringPrintf(R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "%s",
          "Name": "EthernetName",
          "Type": "Ethernet",
          "Ethernet": {
             "Authentication": "None"
          },
          "StaticIPConfig": {
             "NameServers": ["8.8.3.1", "8.8.2.1"],
             "Recommended": ["Gateway", "IPAddress", "RoutingPrefix"]
          },
          "NameServersConfigType": "Static",
          "Recommended": ["IPAddressConfigType"]
        }
      ]
    })",
                                                 kEthernetGuid);
    SetDeviceOpenNetworkConfiguration(kDeviceONC2, /*wait_applied=*/true);
  }

  // Verify that the Static IP is still active, and the custom name server has
  // been applied.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(kServiceEth);
    ASSERT_TRUE(shill_properties);
    EXPECT_EQ(GetStaticIPAddressFromShillProperties(*shill_properties),
              "192.168.1.44");
    EXPECT_THAT(GetStaticNameServersFromShillProperties(*shill_properties),
                ElementsAre("8.8.3.1", "8.8.2.1", "0.0.0.0", "0.0.0.0"));
  }

  // Modify the policy: Force DHCP ip address
  const char kDeviceONC3[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{EthernetGuid}",
          "Name": "EthernetName",
          "Type": "Ethernet",
          "IPAddressConfigType": "DHCP",
          "NameServersConfigType": "DHCP",
          "Ethernet": {
             "Authentication": "None"
          },
          "StaticIPConfig": {
             "Recommended": []
          }
        }
      ]
    })";
  SetDeviceOpenNetworkConfiguration(kDeviceONC3, /*wait_applied=*/true);

  // Check that IP address is not modifiable.
  {
    auto properties = CrosNetworkConfigGetManagedProperties("{EthernetGuid}");
    ASSERT_TRUE(properties);
    EXPECT_EQ(properties->ip_address_config_type->policy_source,
              network_mojom::PolicySource::kDevicePolicyEnforced);
  }

  // Verify that the Static IP is gone.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(kServiceEth);
    ASSERT_TRUE(shill_properties);
    EXPECT_THAT(GetStaticIPAddressFromShillProperties(*shill_properties),
                IsEmpty());
  }
}

// Tests that Ethernet fixes the 'GUID' in 'UIData', if another GUID was
// persisted due to a bug.
// Note: UIData is a String property that chrome fills with a serialized
// dictionary.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest, FixEthernetUIDataGUID) {
  constexpr char kEthernetGuid[] = "{EthernetGuid}";

  shill_service_client_test_->AddService(kServiceEth, "orig_guid_ethernet_any",
                                         "ethernet_any", shill::kTypeEthernet,
                                         shill::kStateOnline, /*visible=*/true);

  // Apply Ethernet policy with a GUID.
  std::string kDeviceONC1 = base::StringPrintf(R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "%s",
          "Name": "EthernetName",
          "Type": "Ethernet",
          "Ethernet": {
             "Authentication": "None"
          }
        }
      ]
    })",
                                               kEthernetGuid);
  SetDeviceOpenNetworkConfiguration(kDeviceONC1, /*wait_applied=*/true);

  // Set GUID in the "user_settings" part of the UIData dictionary to a
  // inconsistent value.
  {
    std::optional<base::Value::Dict> ui_data = GetUIDataDict(kServiceEth);
    ASSERT_TRUE(ui_data);
    base::Value::Dict* user_settings =
        ui_data->EnsureDict(kUIDataKeyUserSettings);
    user_settings->Set(::onc::network_config::kGUID, "wrong-guid");
    ASSERT_NO_FATAL_FAILURE(SetUIDataDict(kServiceEth, *ui_data));
  }

  // Verify that UIData now has the incorrect GUID.
  {
    std::optional<base::Value::Dict> ui_data = GetUIDataDict(kServiceEth);
    ASSERT_TRUE(ui_data);
    EXPECT_NE(GetGUIDFromUIData(*ui_data), kEthernetGuid);
  }

  // Re-apply Ethernet policy.
  std::string kDeviceONC2 = base::StringPrintf(R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "%s",
          "Name": "EthernetName",
          "Type": "Ethernet",
          "Ethernet": {
            "Authentication": "None",
            "StaticIPConfig": {
               "NameServers": ["8.8.3.1", "8.8.2.1"],
               "Recommended": ["Gateway", "IPAddress", "RoutingPrefix"]
            },
            "NameServersConfigType": "Static",
            "Recommended": ["IPAddressConfigType"]
          }
        }
      ]
    })",
                                               kEthernetGuid);
  SetDeviceOpenNetworkConfiguration(kDeviceONC2, /*wait_applied=*/true);

  // Check that GUID in the UIData dictionary has been fixed.
  {
    std::optional<base::Value::Dict> ui_data = GetUIDataDict(kServiceEth);
    ASSERT_TRUE(ui_data);
    EXPECT_EQ(GetGUIDFromUIData(*ui_data), kEthernetGuid);
  }
}

// Tests that a client certificate that has been resolved from a
// ClientCertPattern gets propated to the UI, specifically to the
// CrosNetworkConfig layer.
IN_PROC_BROWSER_TEST_F(
    NetworkPolicyApplicationTest,
    DevicePolicyClientCertPatternSuccessResultPropagatedToUi) {
  const char* kCertKeyFilename = "client_3.pk8";
  const char* kCertFilename = "client_3.pem";
  const char* kCertIssuerCommonName = "E CA";
  const char* kCertSubjectCommonName = "Client Cert F";
  ASSERT_NO_FATAL_FAILURE(ImportCert(net::GetTestCertsDirectory(),
                                     kCertFilename, kCertKeyFilename));

  Add8021xWifiService(kServiceWifi1, "DeviceLevelWifiGuidOrig",
                      "DeviceLevelWifiSsid", shill::kStateOnline);

  SetDeviceOpenNetworkConfiguration(
      OncPolicyToSelectClientCert("{DeviceLevelWifiGuid}",
                                  "DeviceLevelWifiSsid",
                                  /*issuer_common_name=*/kCertIssuerCommonName,
                                  /*identity=*/"TestIdentity"),
      /*wait_applied=*/true);

  {
    auto expected_client_cert =
        CrosNetworkConfigFindClientCert(kCertSubjectCommonName);
    ASSERT_TRUE(expected_client_cert)
        << "Couldn't find cert " << kCertSubjectCommonName;

    auto properties =
        CrosNetworkConfigGetManagedProperties("{DeviceLevelWifiGuid}");
    ASSERT_TRUE(properties);
    ASSERT_EQ(properties->type_properties->which(),
              network_mojom::NetworkTypeManagedProperties::Tag::kWifi);
    const auto& eap = properties->type_properties->get_wifi()->eap;

    // Check the selected certificate
    ASSERT_TRUE(eap->client_cert_pkcs11_id);
    EXPECT_EQ(eap->client_cert_pkcs11_id->active_value,
              expected_client_cert->pem_or_id);
    EXPECT_EQ(eap->client_cert_pkcs11_id->policy_source,
              network_mojom::PolicySource::kDevicePolicyEnforced);
    ASSERT_EQ(eap->client_cert_pkcs11_id->policy_value,
              std::make_optional(expected_client_cert->pem_or_id));

    // The type should be "PKCS11Id" in the UI.
    ASSERT_TRUE(eap->client_cert_type);
    EXPECT_EQ(eap->client_cert_type->active_value, onc::client_cert::kPKCS11Id);
    EXPECT_EQ(eap->client_cert_type->policy_source,
              network_mojom::PolicySource::kDevicePolicyEnforced);
    EXPECT_EQ(eap->client_cert_type->policy_value, onc::client_cert::kPKCS11Id);
  }
}

// Tests that resolution of a ClientCertPattern is tirggered when a client
// certificate is imported.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       DevicePolicyClientCertResolutionTriggeredOnCertImport) {
  const char* kCertKeyFilename = "client_3.pk8";
  const char* kCertFilename = "client_3.pem";
  const char* kCertIssuerCommonName = "E CA";
  const char* kCertSubjectCommonName = "Client Cert F";

  Add8021xWifiService(kServiceWifi1, "DeviceLevelWifiGuidOrig",
                      "DeviceLevelWifiSsid", shill::kStateOnline);

  SetDeviceOpenNetworkConfiguration(
      OncPolicyToSelectClientCert("{DeviceLevelWifiGuid}",
                                  "DeviceLevelWifiSsid",
                                  /*issuer_common_name=*/kCertIssuerCommonName,
                                  /*identity=*/"TestIdentity"),
      /*wait_applied=*/true);

  {
    // The  ClientCertPattern could not be resolved yet, the UI (the
    // CrosNetworkConfig layer) gets an empty PKCS11Id.
    auto properties =
        CrosNetworkConfigGetManagedProperties("{DeviceLevelWifiGuid}");
    ASSERT_TRUE(properties);
    ASSERT_EQ(properties->type_properties->which(),
              network_mojom::NetworkTypeManagedProperties::Tag::kWifi);
    const auto& eap = properties->type_properties->get_wifi()->eap;

    // Check that the selected certificate field is empty.
    ASSERT_TRUE(eap->client_cert_pkcs11_id);
    EXPECT_EQ(eap->client_cert_pkcs11_id->active_value, std::string());
    EXPECT_EQ(eap->client_cert_pkcs11_id->policy_source,
              network_mojom::PolicySource::kDevicePolicyEnforced);
    EXPECT_EQ(eap->client_cert_pkcs11_id->policy_value, std::string());

    // The type should still be "PKCS11Id" for the UI layer to indicate an empty
    // selection.
    ASSERT_TRUE(eap->client_cert_type);
    EXPECT_EQ(eap->client_cert_type->active_value, onc::client_cert::kPKCS11Id);
    EXPECT_EQ(eap->client_cert_type->policy_source,
              network_mojom::PolicySource::kDevicePolicyEnforced);
    EXPECT_EQ(eap->client_cert_type->policy_value, onc::client_cert::kPKCS11Id);
  }

  ServicePropertyValueWatcher eap_cert_id_watcher(
      shill_service_client_test_, kServiceWifi1, shill::kEapCertIdProperty);
  ASSERT_NO_FATAL_FAILURE(ImportCert(net::GetTestCertsDirectory(),
                                     kCertFilename, kCertKeyFilename));
  eap_cert_id_watcher.WaitForNonEmptyValue();

  {
    auto expected_client_cert =
        CrosNetworkConfigFindClientCert(kCertSubjectCommonName);
    ASSERT_TRUE(expected_client_cert)
        << "Couldn't find cert " << kCertSubjectCommonName;

    auto properties =
        CrosNetworkConfigGetManagedProperties("{DeviceLevelWifiGuid}");
    ASSERT_TRUE(properties);
    ASSERT_EQ(properties->type_properties->which(),
              network_mojom::NetworkTypeManagedProperties::Tag::kWifi);
    const auto& eap = properties->type_properties->get_wifi()->eap;

    // Check the selected certificate
    ASSERT_TRUE(eap->client_cert_pkcs11_id);
    EXPECT_EQ(eap->client_cert_pkcs11_id->active_value,
              expected_client_cert->pem_or_id);
    EXPECT_EQ(eap->client_cert_pkcs11_id->policy_source,
              network_mojom::PolicySource::kDevicePolicyEnforced);
    ASSERT_EQ(eap->client_cert_pkcs11_id->policy_value,
              std::make_optional(expected_client_cert->pem_or_id));

    // The type should be "PKCS11Id" in the UI.
    ASSERT_TRUE(eap->client_cert_type);
    EXPECT_EQ(eap->client_cert_type->active_value, onc::client_cert::kPKCS11Id);
    EXPECT_EQ(eap->client_cert_type->policy_source,
              network_mojom::PolicySource::kDevicePolicyEnforced);
    EXPECT_EQ(eap->client_cert_type->policy_value, onc::client_cert::kPKCS11Id);
  }
}

IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       RetainEthernetIPAddrUnmanagedToManaged) {
  constexpr char kEthernetGuidUnmanaged[] = "{orig_guid_ethernet_any}";
  constexpr char kEthernetGuidManaged[] = "{EthernetGuid}";
  constexpr char kStaticIpAddr[] = "192.168.1.44";

  CrosNetworkConfigGuidsAvailableWaiter available_waiter(
      cros_network_config_.get(), {kEthernetGuidUnmanaged});
  shill_service_client_test_->AddService(kServiceEth, kEthernetGuidUnmanaged,
                                         "ethernet_any", shill::kTypeEthernet,
                                         shill::kStateOnline, /*visible=*/true);
  shill_profile_client_test_->AddService(kSharedProfilePath, kServiceEth);
  available_waiter.Wait();

  // Check that IP address is modifiable and does not come from policy
  {
    auto properties =
        CrosNetworkConfigGetManagedProperties(kEthernetGuidUnmanaged);
    ASSERT_TRUE(properties);
    EXPECT_EQ(properties->ip_address_config_type->policy_source,
              network_mojom::PolicySource::kNone);
  }

  // Simulate setting an IP address through the UI.
  {
    auto properties = network_mojom::ConfigProperties::New();
    properties->type_config =
        network_mojom::NetworkTypeConfigProperties::NewEthernet(
            network_mojom::EthernetConfigProperties::New());
    properties->ip_address_config_type =
        ::onc::network_config::kIPConfigTypeStatic;
    properties->static_ip_config = network_mojom::IPConfigProperties::New();
    properties->static_ip_config->ip_address = kStaticIpAddr;
    properties->static_ip_config->gateway = "192.168.1.1";
    properties->static_ip_config->routing_prefix = 4;
    ASSERT_NO_FATAL_FAILURE(CrosNetworkConfigSetProperties(
        kEthernetGuidUnmanaged, std::move(properties)));
  }

  // Verify that the Static IP config has been applied.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(kServiceEth);
    ASSERT_TRUE(shill_properties);
    EXPECT_EQ(GetStaticIPAddressFromShillProperties(*shill_properties),
              kStaticIpAddr);
  }

  // Apply policy: Explicitly recommend both IP address and Nameservers,
  // allowing the user to modify them.
  const std::string kDeviceONCEverythingRecommended =
      base::StringPrintf(R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "%s",
          "Name": "EthernetName",
          "Type": "Ethernet",
          "Ethernet": {
             "Authentication": "None"
          },
          "StaticIPConfig": {
             "Recommended": ["Gateway", "IPAddress", "RoutingPrefix",
                             "NameServers"]
          },
          "Recommended": ["IPAddressConfigType", "NameServersConfigType"]
        }
      ]
    })",
                         kEthernetGuidManaged);
  SetDeviceOpenNetworkConfiguration(kDeviceONCEverythingRecommended,
                                    /*wait_applied=*/true);

  // Verify that the Static IP is still active.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(kServiceEth);
    ASSERT_TRUE(shill_properties);
    EXPECT_EQ(GetStaticIPAddressFromShillProperties(*shill_properties),
              kStaticIpAddr);
  }

  // Check that IP address is modifiable and "Recommended" by policy
  {
    auto properties =
        CrosNetworkConfigGetManagedProperties(kEthernetGuidManaged);
    ASSERT_TRUE(properties);
    EXPECT_EQ(properties->ip_address_config_type->policy_source,
              network_mojom::PolicySource::kDevicePolicyRecommended);
  }
}

IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       EphemeralActions_NotActive) {
  constexpr char kGuidWifi1[] = "guid_wifi_1";
  AddSharedDevicePolicyMschapv2Service(kServiceWifi1, kGuidWifi1, "wifi1",
                                       kUserIdentity);
  AddPskWifiService(kServiceWifi2, "unmanaged_wifi2_guid", "UnmanagedWifi2",
                    shill::kStateIdle);
  shill_profile_client_test_->AddService(kSharedProfilePath, kServiceWifi2);

  const std::string kDeviceONC = base::StringPrintf(
      R"(
      {
        "GlobalNetworkConfiguration": {
        },
        "NetworkConfigurations": [
          {
            "GUID": "%s",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "HexSSID": "7769666931", // "wifi1"
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Inner": "MSCHAPv2",
                "SaveCredentials": true,
                "Recommended": ["Identity", "Password"]
              }
            }
          }
        ],
        "Type": "UnencryptedConfiguration"
      })",
      kGuidWifi1);
  SetDeviceOpenNetworkConfiguration(kDeviceONC,
                                    /*wait_applied=*/true);

  // Verify that the recommended EAP.Identity of the managed wifi service has
  // not been wiped.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(shill_properties);
    EXPECT_THAT(*shill_properties,
                DictionaryHasValue(shill::kEapIdentityProperty,
                                   base::Value(kUserIdentity)));
  }

  // Verify that the unmanaged wifi service has not been wiped.
  EXPECT_TRUE(shill_profile_client_test_->HasService(kServiceWifi2));
}

IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationEphemeralActionsEnabledTest,
                       EphemeralActions_Active) {
  constexpr char kGuidWifi1[] = "guid_wifi_1";
  AddSharedDevicePolicyMschapv2Service(kServiceWifi1, kGuidWifi1, "wifi1",
                                       kUserIdentity);
  AddPskWifiService(kServiceWifi2, "unmanaged_wifi2_guid", "UnmanagedWifi2",
                    shill::kStateIdle);
  shill_profile_client_test_->AddService(kSharedProfilePath, kServiceWifi2);

  const std::string kDeviceONC = base::StringPrintf(
      R"(
      {
        "GlobalNetworkConfiguration": {
          "RecommendedValuesAreEphemeral": true,
          "UserCreatedNetworkConfigurationsAreEphemeral": true
        },
        "NetworkConfigurations": [
          {
            "GUID": "%s",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "HexSSID": "7769666931", // "wifi1"
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Inner": "MSCHAPv2",
                "SaveCredentials": true,
                "Recommended": ["Identity", "Password"]
              }
            }
          }
        ],
        "Type": "UnencryptedConfiguration"
      })",
      kGuidWifi1);
  SetDeviceOpenNetworkConfiguration(kDeviceONC,
                                    /*wait_applied=*/true);

  // Verify that the recommended EAP.Identity of the managed wifi service has
  // been wiped.
  std::optional<std::string> new_service_path =
      shill_service_client_test_->FindServiceMatchingGUID(kGuidWifi1);
  ASSERT_TRUE(new_service_path);
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(*new_service_path);
    ASSERT_TRUE(shill_properties);
    EXPECT_THAT(shill_properties->FindString(shill::kEapIdentityProperty),
                IsNull());
    // Also check some other property to see that it has not been wiped.
    EXPECT_THAT(shill_properties->FindString(shill::kEapPhase2AuthProperty),
                Pointee(Eq("auth=MSCHAPV2")));
  }

  // Verify that the unmanaged wifi service has been wiped.
  EXPECT_FALSE(shill_profile_client_test_->HasService(kServiceWifi2));

  // Set EAP.Identity back to a non-empty value.
  shill_service_client_test_->SetServiceProperty(*new_service_path,
                                                 shill::kEapIdentityProperty,
                                                 base::Value(kUserIdentity));

  // Simulate that the device goes to sleep and wakes up, and expect that it
  // leads to this entry getting deleted and then re-created without
  // "EAP.Identity" again. Internally this process is implemented as a policy
  // application, so we can use that to detect the end of the process.
  ScopedNetworkPolicyApplicationObserver network_policy_application_observer;
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone(base::Minutes(10));
  network_policy_application_observer.WaitPoliciesApplied(
      /*userhash=*/std::string());

  // Verify that the recommended EAP.Identity of the recreated managed wifi
  // service has been wiped again.
  new_service_path =
      shill_service_client_test_->FindServiceMatchingGUID(kGuidWifi1);
  ASSERT_TRUE(new_service_path);
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(*new_service_path);
    ASSERT_TRUE(shill_properties);
    EXPECT_THAT(shill_properties->FindString(shill::kEapIdentityProperty),
                IsNull());
  }

  // Simulate user log-in
  LoginUser(test_account_id_);

  // Set EAP.Identity back to a non-empty value.
  shill_service_client_test_->SetServiceProperty(*new_service_path,
                                                 shill::kEapIdentityProperty,
                                                 base::Value(kUserIdentity));

  // Simulate that the device goes to sleep and wakes up.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone(base::Minutes(10));
  base::RunLoop().RunUntilIdle();

  // Verify that the recommended EAP.Identity of the managed wifi service has
  // not been wiped because the "ephemeral actions" don't apply within active
  // sessions.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(*new_service_path);
    ASSERT_TRUE(shill_properties);
    EXPECT_THAT(*shill_properties,
                DictionaryHasValue(shill::kEapIdentityProperty,
                                   base::Value("user_identity")));
  }
}

IN_PROC_BROWSER_TEST_F(
    NetworkPolicyApplicationEphemeralActionsEnabledUnenrolledTest,
    EphemeralActions_NoWipeOnEnterpriseEnrollment) {
  constexpr char kGuidWifi1[] = "guid_wifi_1";
  AddSharedDevicePolicyMschapv2Service(kServiceWifi1, kGuidWifi1, "wifi1",
                                       kUserIdentity);
  AddPskWifiService(kServiceWifi2, "unmanaged_wifi2_guid", "UnmanagedWifi2",
                    shill::kStateIdle);
  shill_profile_client_test_->AddService(kSharedProfilePath, kServiceWifi2);

  // The device changes state to enterprise-enrolled here, simulating enterprise
  // enrollment with incoming device policy.
  MarkEnterpriseEnrolled();
  const std::string kDeviceONC = base::StringPrintf(
      R"(
      {
        "GlobalNetworkConfiguration": {
          "RecommendedValuesAreEphemeral": true,
          "UserCreatedNetworkConfigurationsAreEphemeral": true
        },
        "NetworkConfigurations": [
          {
            "GUID": "%s",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "HexSSID": "7769666931", // "wifi1"
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Inner": "MSCHAPv2",
                "SaveCredentials": true,
                "Recommended": ["Identity", "Password"]
              }
            }
          }
        ],
        "Type": "UnencryptedConfiguration"
      })",
      kGuidWifi1);
  SetDeviceOpenNetworkConfiguration(kDeviceONC,
                                    /*wait_applied=*/true);

  // Verify that the unmanaged wifi service has not been wiped.
  EXPECT_TRUE(shill_profile_client_test_->HasService(kServiceWifi2));

  // Simulate that the device goes to sleep and wakes up, and check that the
  // unmanaged wifi gets wiped then.
  ScopedNetworkPolicyApplicationObserver network_policy_application_observer;
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone(base::Minutes(10));
  network_policy_application_observer.WaitPoliciesApplied(
      /*userhash=*/std::string());

  EXPECT_FALSE(shill_profile_client_test_->HasService(kServiceWifi2));
}

IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationEphemeralActionsDisabledTest,
                       EphemeralActions_ActiveByPolicy) {
  constexpr char kGuidWifi1[] = "guid_wifi_1";
  AddSharedDevicePolicyMschapv2Service(kServiceWifi1, kGuidWifi1, "wifi1",
                                       kUserIdentity);
  AddPskWifiService(kServiceWifi2, "unmanaged_wifi2_guid", "UnmanagedWifi2",
                    shill::kStateIdle);
  shill_profile_client_test_->AddService(kSharedProfilePath, kServiceWifi2);

  SetDeviceEphemeralNetworkPoliciesEnabledPolicy(true);

  const std::string kDeviceONC = base::StringPrintf(
      R"(
      {
        "GlobalNetworkConfiguration": {
          "RecommendedValuesAreEphemeral": true,
          "UserCreatedNetworkConfigurationsAreEphemeral": true
        },
        "NetworkConfigurations": [
          {
            "GUID": "%s",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "HexSSID": "7769666931", // "wifi1"
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Inner": "MSCHAPv2",
                "SaveCredentials": true,
                "Recommended": ["Identity", "Password"]
              }
            }
          }
        ],
      "Type": "UnencryptedConfiguration"
      })",
      kGuidWifi1);
  SetDeviceOpenNetworkConfiguration(kDeviceONC,
                                    /*wait_applied=*/true);

  // Verify that the recommended EAP.Identity of the managed wifi service has
  // been wiped.
  {
    std::optional<std::string> new_service_path =
        shill_service_client_test_->FindServiceMatchingGUID(kGuidWifi1);
    ASSERT_TRUE(new_service_path);
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(*new_service_path);
    ASSERT_TRUE(shill_properties);
    EXPECT_THAT(shill_properties->FindString(shill::kEapIdentityProperty),
                IsNull());
    // Also check some other property to see that it has not been wiped.
    EXPECT_THAT(shill_properties->FindString(shill::kEapPhase2AuthProperty),
                Pointee(Eq("auth=MSCHAPV2")));
  }

  // Verify that the unmanaged wifi service has been wiped.
  EXPECT_FALSE(shill_profile_client_test_->HasService(kServiceWifi2));
}

IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationEphemeralActionsDisabledTest,
                       EphemeralActions_Active) {
  constexpr char kGuidWifi1[] = "guid_wifi_1";
  AddSharedDevicePolicyMschapv2Service(kServiceWifi1, kGuidWifi1, "wifi1",
                                       kUserIdentity);
  AddPskWifiService(kServiceWifi2, "unmanaged_wifi2_guid", "UnmanagedWifi2",
                    shill::kStateIdle);
  shill_profile_client_test_->AddService(kSharedProfilePath, kServiceWifi2);

  const std::string kDeviceONC = base::StringPrintf(
      R"(
      {
        "GlobalNetworkConfiguration": {
          "RecommendedValuesAreEphemeral": true,
          "UserCreatedNetworkConfigurationsAreEphemeral": true
        },
        "NetworkConfigurations": [
          {
            "GUID": "%s",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "HexSSID": "7769666931", // "wifi1"
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Inner": "MSCHAPv2",
                "SaveCredentials": true,
                "Recommended": ["Identity", "Password"]
              }
            }
          }
        ],
      "Type": "UnencryptedConfiguration"
      })",
      kGuidWifi1);
  SetDeviceOpenNetworkConfiguration(kDeviceONC,
                                    /*wait_applied=*/true);

  // Verify that the recommended EAP.Identity of the managed wifi service has
  // not been wiped.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(shill_properties);
    EXPECT_THAT(*shill_properties,
                DictionaryHasValue(shill::kEapIdentityProperty,
                                   base::Value(kUserIdentity)));
  }

  // Verify that the unmanaged wifi service has not been wiped.
  EXPECT_TRUE(shill_profile_client_test_->HasService(kServiceWifi2));
}

IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationEphemeralActionsKillSwitchTest,
                       EphemeralActions_ActiveByPolicy) {
  constexpr char kGuidWifi1[] = "guid_wifi_1";
  AddSharedDevicePolicyMschapv2Service(kServiceWifi1, kGuidWifi1, "wifi1",
                                       kUserIdentity);
  AddPskWifiService(kServiceWifi2, "unmanaged_wifi2_guid", "UnmanagedWifi2",
                    shill::kStateIdle);
  shill_profile_client_test_->AddService(kSharedProfilePath, kServiceWifi2);

  SetDeviceEphemeralNetworkPoliciesEnabledPolicy(true);

  const std::string kDeviceONC = base::StringPrintf(
      R"(
      {
        "GlobalNetworkConfiguration": {
          "RecommendedValuesAreEphemeral": true,
          "UserCreatedNetworkConfigurationsAreEphemeral": true
        },
        "NetworkConfigurations": [
          {
            "GUID": "%s",
            "Type": "WiFi",
            "Name": "Managed wifi1",
            "WiFi": {
              "HexSSID": "7769666931", // "wifi1"
              "SSID": "wifi1",
              "Security": "WPA-EAP",
              "EAP": {
                "Outer": "PEAP",
                "Inner": "MSCHAPv2",
                "SaveCredentials": true,
                "Recommended": ["Identity", "Password"]
              }
            }
          }
        ],
        "Type": "UnencryptedConfiguration"
      })",
      kGuidWifi1);
  SetDeviceOpenNetworkConfiguration(kDeviceONC,
                                    /*wait_applied=*/true);

  // Verify that the recommended EAP.Identity of the managed wifi service has
  // not been wiped.
  {
    const base::Value::Dict* shill_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(shill_properties);
    EXPECT_THAT(*shill_properties,
                DictionaryHasValue(shill::kEapIdentityProperty,
                                   base::Value(kUserIdentity)));
  }

  // Verify that the unmanaged wifi service has not been wiped.
  EXPECT_TRUE(shill_profile_client_test_->HasService(kServiceWifi2));
}

IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       CheckCaptivePortal_AllValues) {
  constexpr char kGuidWifiTrue[] = "guid_wifi_true";
  constexpr char kGuidWifiFalse[] = "guid_wifi_false";
  constexpr char kGuidWifiHTTPOnly[] = "guid_wifi_http_only";

  constexpr char kNetworkNameTrue[] = "NetworkTrue";
  constexpr char kNetworkNameFalse[] = "NetworkFalse";
  constexpr char kNetworkNameHTTPOnly[] = "NetworkHTTPOnly";

  constexpr char kWifiNameTrue[] = "WifiTrue";
  constexpr char kWifiNameFalse[] = "WifiFalse";
  constexpr char kWifiNameHTTPOnly[] = "WifiHTTPOnly";

  AddPskWifiService(kServiceWifi1, kGuidWifiTrue, kWifiNameTrue,
                    shill::kStateIdle);
  AddPskWifiService(kServiceWifi2, kGuidWifiFalse, kWifiNameFalse,
                    shill::kStateIdle);
  AddPskWifiService(kServiceWifi3, kGuidWifiHTTPOnly, kWifiNameHTTPOnly,
                    shill::kStateIdle);

  static constexpr char kConfig[] = R"(
      {
        "GlobalNetworkConfiguration": {
        },
        "NetworkConfigurations": [
          {
            "GUID": "%s",
            "Name": "%s",
            "Type": "WiFi",
            "CheckCaptivePortal": "%s",
            "WiFi": {
              "AutoConnect": true,
              "HiddenSSID": false,
              "Passphrase": "DeviceLevelWifiPwd",
              "SSID": "%s",
              "Security": "WPA-PSK"
            }
          },
          {
            "GUID": "%s",
            "Name": "%s",
            "Type": "WiFi",
            "CheckCaptivePortal": "%s",
            "WiFi": {
              "AutoConnect": true,
              "HiddenSSID": false,
              "Passphrase": "DeviceLevelWifiPwd",
              "SSID": "%s",
              "Security": "WPA-PSK"
            }
          },
          {
            "GUID": "%s",
            "Name": "%s",
            "Type": "WiFi",
            "CheckCaptivePortal": "%s",
            "WiFi": {
              "AutoConnect": true,
              "HiddenSSID": false,
              "Passphrase": "DeviceLevelWifiPwd",
              "SSID": "%s",
              "Security": "WPA-PSK"
            }
          }
        ],
        "Type": "UnencryptedConfiguration"
      })";

  const std::string kDeviceONC = base::StringPrintf(
      kConfig, kGuidWifiTrue, kNetworkNameTrue,
      ::onc::check_captive_portal::kTrue, kWifiNameTrue, kGuidWifiFalse,
      kNetworkNameFalse, ::onc::check_captive_portal::kFalse, kWifiNameFalse,
      kGuidWifiHTTPOnly, kNetworkNameHTTPOnly,
      ::onc::check_captive_portal::kHTTPOnly, kWifiNameHTTPOnly);
  SetDeviceOpenNetworkConfiguration(kDeviceONC,
                                    /*wait_applied=*/true);

  // Verify that the CheckCaptivePortal of the managed Wi-Fi services are set
  // correctly.
  {
    const base::Value::Dict* shill_properties1 =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(shill_properties1);
    EXPECT_THAT(
        *shill_properties1,
        DictionaryHasValue(shill::kCheckPortalProperty, base::Value("true")));

    const base::Value::Dict* shill_properties2 =
        shill_service_client_test_->GetServiceProperties(kServiceWifi2);
    ASSERT_TRUE(shill_properties2);
    EXPECT_THAT(
        *shill_properties2,
        DictionaryHasValue(shill::kCheckPortalProperty, base::Value("false")));

    const base::Value::Dict* shill_properties3 =
        shill_service_client_test_->GetServiceProperties(kServiceWifi3);
    ASSERT_TRUE(shill_properties3);
    EXPECT_THAT(*shill_properties3,
                DictionaryHasValue(shill::kCheckPortalProperty,
                                   base::Value("http-only")));
  }
}

// Tests that when
// - a policy-provided network has a ClientCertPattern which contains an
//   EnrollmentURI and
// - the ClientCertPattern doesn't match any installed client certificate,
// then, on a connection attempt through the system tray, a dialog is triggered
// which suggests to enroll a client certificate (the somewhat confusingly named
// "enrollment dialog" - it's not related to enterprise enrollment).
// Also tests that when accepting that dialog, a browser tab is opened,
// navigating to the provided EnrollmentURI.
//
// This is a regression test for b/319188170.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       ClientCertEnrollmentUriTriggered) {
  Add8021xWifiService(kServiceWifi1, "UserLevelWifiGuidOrig",
                      "UserLevelWifiSsid", shill::kStateIdle);

  LoginUser(test_account_id_);
  const std::string user_hash = user_manager::UserManager::Get()
                                    ->FindUser(test_account_id_)
                                    ->username_hash();
  shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);

  // Set a policy that uses a ClientCertPattern which has an EnrollmentURI and
  // will not resolve to any client certificate (no client certificate has been
  // installed/imported at all).
  static constexpr char kUserONC[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{user-policy-for-wifi}",
          "Name": "OncPolicyToSelectClientCert",
          "Type": "WiFi",
          "WiFi": {
             "EAP":  {
              "Outer": "EAP-TLS",
              "ClientCertType": "Pattern",
              "Identity": "SomeIdentity",
              "ClientCertPattern": {
                "Issuer": {
                  "CommonName": "DoesntMatchAnything"
                },
                "EnrollmentURI": ["chrome://version"]
              }
             },
             "SSID": "UserLevelWifiSsid",
             "Security": "WPA-EAP"
          }
        }
      ]
    })";
  SetUserOpenNetworkConfiguration(user_hash, kUserONC,
                                  /*wait_applied=*/true);

  // Click on the SSID in the system tray and expect the (client certificate)
  // enrollment dialog to appear.
  views::NamedWidgetShownWaiter dialog_widget_waiter(
      views::test::AnyWidgetTestPasskey(), ash::enrollment::kWidgetName);
  ASSERT_NO_FATAL_FAILURE(ConnectToSsidUsingSystemTray("UserLevelWifiSsid"));

  views::Widget* dialog_widget = dialog_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog_widget);

  // Accept the enrollment dialog and expect a corresponding tab with the
  // EnrollmentURI to be opened.
  ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
  ASSERT_NO_FATAL_FAILURE(AcceptCertEnrollmentDialog(dialog_widget));

  content::WebContents* const tab_contents = tab_waiter.Wait();
  ASSERT_TRUE(tab_contents);

  EXPECT_EQ(tab_contents->GetVisibleURL(), GURL("chrome://version"));
}

}  // namespace policy
