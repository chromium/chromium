// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/mdns/nearby_connections_mdns_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/local_discovery/fake_service_discovery_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chromeos/ash/services/nearby/public/mojom/mdns.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// What would an actual service description look like?
static const char kNearbyServiceName[] = "Android1";
static const char kNearbyServiceType[] = "_8DC2285A81F6.tcp_";
static const net::IPAddress kNearbyServiceIpAddress =
    net::IPAddress(192u, 168u, 57u, 64u);
static const int kNearbyServicePort = 40;
static const char kNearbyEndpointInfoKey[] = "n";
static const char kNearbyEndpointInfo[] = "TestEndpointInfo";

local_discovery::ServiceDescription MakeServiceDescription(
    std::string service_name,
    std::string service_type) {
  local_discovery::ServiceDescription service_description;
  service_description.service_name =
      base::StrCat({service_name, ".", service_type});
  service_description.address.set_host(base::StrCat({service_name, ".local"}));
  service_description.address.set_port(kNearbyServicePort);
  service_description.ip_address = kNearbyServiceIpAddress;
  service_description.metadata.push_back(
      base::StrCat({kNearbyEndpointInfoKey, "=", kNearbyEndpointInfo}));
  return service_description;
}

}  // namespace

class FakeMdnsObserver : public ::sharing::mojom::MdnsObserver {
 public:
  void ServiceFound(sharing::mojom::NsdServiceInfoPtr service_info) override {
    ++num_services_found;
    found_info_ = std::move(service_info);
  }

  void ServiceLost(sharing::mojom::NsdServiceInfoPtr service_info) override {
    ++num_services_lost;
    lost_info_ = std::move(service_info);
  }

  int num_services_found = 0;
  int num_services_lost = 0;
  sharing::mojom::NsdServiceInfoPtr found_info_;
  sharing::mojom::NsdServiceInfoPtr lost_info_;
  mojo::Receiver<::sharing::mojom::MdnsObserver> receiver_{this};
};

class NearbyConnectionsMdnsManagerTest : public ::testing::Test {
 public:
  NearbyConnectionsMdnsManagerTest()
      : mdns_manager_(
            std::make_unique<nearby::sharing::NearbyConnectionsMdnsManager>()) {
  }
  ~NearbyConnectionsMdnsManagerTest() override = default;

  void SetUp() override {
    auto* runner = task_environment_.GetMainThreadTaskRunner().get();
    auto nearby_service_lister =
        std::make_unique<local_discovery::FakeServiceDiscoveryDeviceLister>(
            runner, kNearbyServiceType);
    nearby_service_lister_ = nearby_service_lister.get();
    std::map<std::string,
             std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister>>
        temp_listers;
    temp_listers[kNearbyServiceType] = std::move(nearby_service_lister);

    mdns_manager_->SetDeviceListersForTesting(&temp_listers);
    mdns_manager_->AddObserver(observer_.receiver_.BindNewPipeAndPassRemote());
    nearby_service_lister_->SetDelegate(mdns_manager_.get());
  }

 protected:
  void StartDiscoverySession(const std::string& service_type) {
    mdns_manager_->StartDiscoverySession(
        service_type,
        base::BindLambdaForTesting([&](bool result) { EXPECT_TRUE(result); }));
  }

  void StopDiscoverySessionWithResult(const std::string& service_type,
                                      bool expected) {
    mdns_manager_->StopDiscoverySession(
        service_type, base::BindLambdaForTesting(
                          [&](bool result) { EXPECT_EQ(result, expected); }));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<nearby::sharing::NearbyConnectionsMdnsManager> mdns_manager_;
  // Pointer dangles when StopDiscovery is called and destroys the pointer.
  raw_ptr<local_discovery::FakeServiceDiscoveryDeviceLister,
          DisableDanglingPtrDetection>
      nearby_service_lister_;
  FakeMdnsObserver observer_;
};

TEST_F(NearbyConnectionsMdnsManagerTest, StartDiscoverySession) {
  StartDiscoverySession(kNearbyServiceType);
}

TEST_F(NearbyConnectionsMdnsManagerTest, StopDiscoverySession) {
  StartDiscoverySession(kNearbyServiceType);
  StopDiscoverySessionWithResult(kNearbyServiceType, /*expected=*/true);
}

TEST_F(NearbyConnectionsMdnsManagerTest,
       StopDiscoverySession_FailsForUnknownSession) {
  StopDiscoverySessionWithResult("UnknownService", /*expected=*/false);
}

TEST_F(NearbyConnectionsMdnsManagerTest, RestartsDiscoveryAfterCacheFlushed) {
  StartDiscoverySession(kNearbyServiceType);

  nearby_service_lister_->Announce(
      MakeServiceDescription(kNearbyServiceName, kNearbyServiceType));
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(observer_.num_services_found, 1);

  nearby_service_lister_->Clear();
  task_environment_.FastForwardUntilNoTasksRemain();
  // Don't notify on cache flush, but expect discovery to have restarted.
  EXPECT_TRUE(nearby_service_lister_->discovery_started());
  EXPECT_EQ(observer_.num_services_lost, 0);
}

TEST_F(NearbyConnectionsMdnsManagerTest, NotifiesObservers_ServiceFound) {
  StartDiscoverySession(kNearbyServiceType);

  nearby_service_lister_->Announce(
      MakeServiceDescription(kNearbyServiceName, kNearbyServiceType));
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(observer_.num_services_found, 1);
  EXPECT_TRUE(observer_.found_info_);
  EXPECT_EQ(observer_.found_info_->service_name, kNearbyServiceName);
  EXPECT_EQ(observer_.found_info_->service_type, kNearbyServiceType);
  auto ip_vec = kNearbyServiceIpAddress.CopyBytesToVector();
  EXPECT_EQ(observer_.found_info_->ip_address,
            std::string(ip_vec.begin(), ip_vec.end()));
  EXPECT_EQ(observer_.found_info_->port, kNearbyServicePort);
  EXPECT_TRUE(observer_.found_info_->txt_records.has_value());
  EXPECT_EQ(observer_.found_info_->txt_records.value()[kNearbyEndpointInfoKey],
            kNearbyEndpointInfo);
}

TEST_F(NearbyConnectionsMdnsManagerTest, NotifiesObservers_ServiceLost) {
  StartDiscoverySession(kNearbyServiceType);
  nearby_service_lister_->Announce(
      MakeServiceDescription(kNearbyServiceName, kNearbyServiceType));
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(observer_.num_services_found, 1);

  nearby_service_lister_->Remove(kNearbyServiceName);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(observer_.num_services_lost, 1);
  EXPECT_TRUE(observer_.lost_info_);
  EXPECT_EQ(observer_.lost_info_->service_name, kNearbyServiceName);
  EXPECT_EQ(observer_.lost_info_->service_type, kNearbyServiceType);
  EXPECT_FALSE(observer_.lost_info_->ip_address);
  EXPECT_FALSE(observer_.lost_info_->port);
  EXPECT_FALSE(observer_.lost_info_->txt_records);
}
