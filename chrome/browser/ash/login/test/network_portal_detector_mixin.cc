// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"

#include "base/run_loop.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/components/onc/onc_utils.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

std::string StatusToState(NetworkPortalDetector::CaptivePortalStatus status) {
  switch (status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
      [[fallthrough]];
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      return shill::kStateNoConnectivity;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      return shill::kStateOnline;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      return shill::kStateRedirectFound;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      return shill::kStatePortalSuspected;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT:
      NOTREACHED();
      return shill::kStateNoConnectivity;
  }
}

}  // namespace

NetworkPortalDetectorMixin::NetworkPortalDetectorMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

NetworkPortalDetectorMixin::~NetworkPortalDetectorMixin() = default;

void NetworkPortalDetectorMixin::SetDefaultNetwork(
    const std::string& network_guid,
    const std::string& network_type,
    NetworkPortalDetector::CaptivePortalStatus status) {
  SetShillDefaultNetwork(network_guid, network_type, status);
  network_portal_detector_->SetDefaultNetworkForTesting(network_guid);

  SimulateDefaultNetworkState(status);
}

void NetworkPortalDetectorMixin::SimulateNoNetwork() {
  SetShillDefaultNetwork("", "",
                         NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
  network_portal_detector_->SetDefaultNetworkForTesting("");
}

void NetworkPortalDetectorMixin::SimulateDefaultNetworkState(
    NetworkPortalDetector::CaptivePortalStatus status) {
  std::string default_network_guid =
      network_portal_detector_->GetDefaultNetworkGuid();
  DCHECK(!default_network_guid.empty());
  std::string default_network_type =
      default_network_guid.compare(0, 4, "wifi") == 0 ? shill::kTypeWifi
                                                      : shill::kTypeEthernet;

  int response_code;
  if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
    response_code = 204;
  } else if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL) {
    response_code = 200;
  } else {
    response_code = -1;
  }

  if (NetworkHandler::IsInitialized()) {
    const NetworkState* default_network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    if (default_network_guid != default_network_guid_ || !default_network ||
        default_network->guid() != default_network_guid) {
      SetShillDefaultNetwork(default_network_guid, default_network_type,
                             status);
    } else {
      std::string state = StatusToState(status);
      ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
          default_network->path(), shill::kStateProperty, base::Value(state));
      base::RunLoop().RunUntilIdle();
    }
  }

  network_portal_detector_->SetDetectionResultsForTesting(
      default_network_guid, status, response_code);
}

void NetworkPortalDetectorMixin::SetUpOnMainThread() {
  // Setup network portal detector to return online for the default network.
  network_portal_detector_ = new NetworkPortalDetectorTestImpl();
  network_portal_detector::InitializeForTesting(network_portal_detector_);
  network_portal_detector_->Enable();
  SetDefaultNetwork(FakeShillManagerClient::kFakeEthernetNetworkGuid,
                    shill::kTypeEthernet,
                    NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
}

void NetworkPortalDetectorMixin::TearDownOnMainThread() {
  network_portal_detector::Shutdown();
}

void NetworkPortalDetectorMixin::SetShillDefaultNetwork(
    const std::string& network_guid,
    const std::string& network_type,
    NetworkPortalDetector::CaptivePortalStatus status) {
  default_network_guid_ = network_guid;
  if (!NetworkHandler::IsInitialized()) {
    return;
  }

  ShillServiceClient::Get()->GetTestInterface()->ClearServices();
  if (network_guid.empty()) {
    base::RunLoop().RunUntilIdle();
    return;
  }

  std::string state = StatusToState(status);
  static constexpr char kJson[] =
      R"({"GUID": "%s", "Type": "%s", "SSID": "wifi_ssid",
          "State": "%s", "Strength": 100, "AutoConnect": true})";
  std::string json_str = base::StringPrintf(
      kJson, network_guid.c_str(), network_type.c_str(), state.c_str());
  absl::optional<base::Value::Dict> json_dict =
      chromeos::onc::ReadDictionaryFromJson(json_str);
  CHECK(json_dict.has_value());
  ShillManagerClient::Get()->ConfigureServiceForProfile(
      dbus::ObjectPath(NetworkProfileHandler::GetSharedProfilePath()),
      *json_dict, base::BindOnce([](const dbus::ObjectPath& result) {}),
      base::BindOnce([](const std::string& err, const std::string& msg) {
        LOG(WARNING) << "Error: " << err << " Msg: " << msg;
      }));
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
