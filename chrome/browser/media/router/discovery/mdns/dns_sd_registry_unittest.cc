// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_delegate.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_device_lister.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;

namespace media_router {

class MockDnsSdDeviceLister : public DnsSdDeviceLister {
 public:
  MockDnsSdDeviceLister() : DnsSdDeviceLister(nullptr, nullptr, "") {}
  ~MockDnsSdDeviceLister() override {}

  MOCK_METHOD0(Discover, void());
};

class TestDnsSdRegistry : public DnsSdRegistry {
 public:
  TestDnsSdRegistry() : DnsSdRegistry(nullptr), delegate_(nullptr) {}
  ~TestDnsSdRegistry() override {}

  MockDnsSdDeviceLister* GetListerForService(const std::string& service_type) {
    return listers_[service_type];
  }

  int GetServiceListenerCount(const std::string& service_type) {
    if (service_data_map_.find(service_type) == service_data_map_.end())
      return 0;

    return service_data_map_[service_type]->GetListenerCount();
  }

  MockDnsSdDeviceLister* GetLister(const std::string& service_type) {
    return listers_.find(service_type) != listers_.end()
               ? listers_[service_type]
               : nullptr;
  }

  DnsSdDelegate* GetDelegate() { return delegate_; }

 protected:
  DnsSdDeviceLister* CreateDnsSdDeviceLister(
      DnsSdDelegate* delegate,
      const std::string& service_type,
      local_discovery::ServiceDiscoverySharedClient* discovery_client)
      override {
    delegate_ = delegate;
    MockDnsSdDeviceLister* lister = new NiceMock<MockDnsSdDeviceLister>();
    listers_[service_type] = lister;
    return lister;
  }

 private:
  std::map<std::string, raw_ptr<MockDnsSdDeviceLister, CtnExperimental>>
      listers_;
  // The last delegate used or NULL.
  raw_ptr<DnsSdDelegate> delegate_;
};

class MockDnsSdObserver : public DnsSdRegistry::DnsSdObserver {
 public:
  MOCK_METHOD(void,
              OnDnsSdEvent,
              (const std::string&, const DnsSdRegistry::DnsSdServiceList&));
  MOCK_METHOD(void, OnDnsSdPermissionRejected, ());
};

class DnsSdRegistryTest : public testing::Test {
 public:
  DnsSdRegistryTest() {}
  ~DnsSdRegistryTest() override {}

  void SetUp() override {
    registry_ = std::make_unique<TestDnsSdRegistry>();
    registry_->AddObserver(&observer_);
  }

 protected:
  std::unique_ptr<TestDnsSdRegistry> registry_;
  MockDnsSdObserver observer_;
};

// Tests registering 2 listeners and removing one. The device lister should
// not be destroyed.
TEST_F(DnsSdRegistryTest, RegisterUnregisterListeners) {
  const std::string service_type = "_testing._tcp.local";

  EXPECT_CALL(observer_,
              OnDnsSdEvent(service_type, DnsSdRegistry::DnsSdServiceList()))
      .Times(2);

  registry_->RegisterDnsSdListener(service_type);
  registry_->RegisterDnsSdListener(service_type);
  registry_->UnregisterDnsSdListener(service_type);
  EXPECT_EQ(1, registry_->GetServiceListenerCount(service_type));

  registry_->UnregisterDnsSdListener(service_type);
  EXPECT_EQ(0, registry_->GetServiceListenerCount(service_type));
}

// Tests that verify calls to ForceDiscovery() are propagated to registered
// listeners.
TEST_F(DnsSdRegistryTest, ForceDiscovery) {
  const std::string service_type1 = "_testing1._tcp.local";
  const std::string service_type2 = "_testing2._tcp.local";

  EXPECT_CALL(observer_,
              OnDnsSdEvent(service_type1, DnsSdRegistry::DnsSdServiceList()));
  EXPECT_CALL(observer_,
              OnDnsSdEvent(service_type2, DnsSdRegistry::DnsSdServiceList()));

  registry_->RegisterDnsSdListener(service_type1);
  registry_->RegisterDnsSdListener(service_type2);
  EXPECT_EQ(1, registry_->GetServiceListenerCount(service_type1));
  EXPECT_EQ(1, registry_->GetServiceListenerCount(service_type2));

  EXPECT_CALL(*registry_->GetLister(service_type1), Discover());
  EXPECT_CALL(*registry_->GetLister(service_type2), Discover());
  registry_->ResetAndDiscover();
}

// Tests registering a listener and receiving an added and updated event.
TEST_F(DnsSdRegistryTest, AddAndUpdate) {
  const std::string service_type = "_testing._tcp.local";
  const std::string ip_address1 = "192.168.0.100";
  const std::string ip_address2 = "192.168.0.101";

  DnsSdService service;
  service.service_name = "_myDevice." + service_type;
  service.ip_address = ip_address1;

  DnsSdRegistry::DnsSdServiceList service_list;

  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));

  // Add first service.
  service_list.push_back(service);
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));

  // Clear services and add same one with different IP address.
  service_list.clear();
  service.ip_address = ip_address2;
  service_list.push_back(service);
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));

  registry_->RegisterDnsSdListener(service_type);
  service.ip_address = ip_address1;
  registry_->GetDelegate()->ServiceChanged(service_type, true, service);
  service.ip_address = ip_address2;
  registry_->GetDelegate()->ServiceChanged(service_type, false, service);
}

TEST_F(DnsSdRegistryTest, AddServiceWithInvalidIPAddress) {
  const std::string service_type = "_testing._tcp.local";
  const std::string ip_address1 = "invalid";

  DnsSdService service;
  service.service_name = "_myDevice." + service_type;

  DnsSdRegistry::DnsSdServiceList service_list;

  EXPECT_CALL(observer_, OnDnsSdEvent(_, _)).Times(1);
  registry_->RegisterDnsSdListener(service_type);
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer_));

  EXPECT_CALL(observer_, OnDnsSdEvent(_, _)).Times(0);
  service.ip_address = ip_address1;
  registry_->GetDelegate()->ServiceChanged(service_type, true, service);
}

// Tests registering a listener and receiving an added and removed event.
TEST_F(DnsSdRegistryTest, AddAndRemove) {
  const std::string service_type = "_testing._tcp.local";

  DnsSdService service;
  service.service_name = "_myDevice." + service_type;
  service.ip_address = "192.168.0.100";

  DnsSdRegistry::DnsSdServiceList service_list;
  // Expect to be called twice with empty list (once on register, once after
  // removing).
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list)).Times(2);
  service_list.push_back(service);
  // Expect to be called twice with 1 item (once after adding, once after adding
  // again after removal).
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list)).Times(2);

  registry_->RegisterDnsSdListener(service_type);
  registry_->GetDelegate()->ServiceChanged(service_type, true, service);
  registry_->GetDelegate()->ServiceRemoved(service_type, service.service_name);
  registry_->GetDelegate()->ServiceChanged(service_type, true, service);
}

// Tests adding multiple services.
TEST_F(DnsSdRegistryTest, AddMultipleServices) {
  const std::string service_type = "_testing._tcp.local";

  DnsSdService service;
  service.service_name = "_myDevice." + service_type;
  service.ip_address = "192.168.0.100";

  DnsSdService service2;
  service2.service_name = "_myDevice2." + service_type;
  service2.ip_address = "192.168.0.101";

  DnsSdRegistry::DnsSdServiceList service_list;
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));
  service_list.push_back(service);
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));
  service_list.push_back(service2);
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));

  registry_->RegisterDnsSdListener(service_type);
  registry_->GetDelegate()->ServiceChanged(service_type, true, service);
  registry_->GetDelegate()->ServiceChanged(service_type, true, service2);
}

// Tests adding multiple services and handling a flush event.
TEST_F(DnsSdRegistryTest, FlushCache) {
  testing::InSequence s;
  const std::string service_type = "_testing._tcp.local";

  DnsSdService service;
  service.service_name = "_myDevice." + service_type;
  service.ip_address = "192.168.0.100";

  DnsSdService service2;
  service2.service_name = "_myDevice2." + service_type;
  service2.ip_address = "192.168.0.101";

  DnsSdRegistry::DnsSdServiceList service_list;
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));
  service_list.push_back(service);
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));
  service_list.push_back(service2);
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));
  service_list.clear();
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));

  registry_->RegisterDnsSdListener(service_type);
  registry_->GetDelegate()->ServiceChanged(service_type, true, service);
  registry_->GetDelegate()->ServiceChanged(service_type, true, service2);
  registry_->GetDelegate()->ServicesFlushed(service_type);
}

// Tests receiving an update from the DnsSdDelegate that does not change the
// service object does not notify the observer.
TEST_F(DnsSdRegistryTest, UpdateOnlyIfChanged) {
  const std::string service_type = "_testing._tcp.local";
  const std::string ip_address = "192.168.0.100";

  DnsSdService service;
  service.service_name = "_myDevice." + service_type;
  service.ip_address = ip_address;

  DnsSdRegistry::DnsSdServiceList service_list;
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));

  // Expect service_list with initial service.
  service_list.push_back(service);
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));

  // Expect service_list with updated service.
  service_list.clear();
  service.ip_address = "192.168.0.101";
  service_list.push_back(service);
  EXPECT_CALL(observer_, OnDnsSdEvent(service_type, service_list));
  // No more calls to observer_.

  registry_->RegisterDnsSdListener(service_type);
  service.ip_address = "192.168.0.100";
  registry_->GetDelegate()->ServiceChanged(service_type, true, service);
  // Update with changed ip address.
  service.ip_address = "192.168.0.101";
  registry_->GetDelegate()->ServiceChanged(service_type, false, service);
  // Update with no changes to the service.
  registry_->GetDelegate()->ServiceChanged(service_type, false, service);
}

}  // namespace media_router
