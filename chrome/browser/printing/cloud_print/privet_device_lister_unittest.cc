// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/printing/cloud_print/privet_device_lister_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using local_discovery::LocalDomainResolver;
using local_discovery::ServiceDescription;
using local_discovery::ServiceDiscoveryClient;
using local_discovery::ServiceResolver;
using local_discovery::ServiceWatcher;
using testing::_;
using testing::SaveArg;

namespace cloud_print {

namespace {

class MockServiceResolver;
class MockServiceWatcher;

class ServiceDiscoveryMockDelegate {
 public:
  virtual ~ServiceDiscoveryMockDelegate() {}
  virtual void ServiceWatcherStarted(const std::string& service_type,
                                     MockServiceWatcher* watcher) = 0;
  virtual void ServiceResolverStarted(const std::string& service_type,
                                      MockServiceResolver* resolver) = 0;
};

class MockServiceWatcher : public ServiceWatcher {
 public:
  MockServiceWatcher(const std::string& service_type,
                     ServiceWatcher::UpdatedCallback callback,
                     ServiceDiscoveryMockDelegate* mock_delegate)
      : started_(false),
        service_type_(service_type),
        callback_(std::move(callback)),
        mock_delegate_(mock_delegate) {}

  ~MockServiceWatcher() override {}

  void Start() override {
    DCHECK(!started_);
    started_ = true;
    mock_delegate_->ServiceWatcherStarted(service_type_, this);
  }

  MOCK_METHOD0(DiscoverNewServices, void());

  MOCK_METHOD1(SetActivelyRefreshServices, void(
      bool actively_refresh_services));

  std::string GetServiceType() const override { return service_type_; }

  bool started() {
    return started_;
  }

  ServiceWatcher::UpdatedCallback callback() {
    return callback_;
  }

 private:
  bool started_;
  std::string service_type_;
  ServiceWatcher::UpdatedCallback callback_;
  ServiceDiscoveryMockDelegate* mock_delegate_;
};

class MockServiceResolver : public ServiceResolver {
 public:
  MockServiceResolver(const std::string& service_name,
                      ServiceResolver::ResolveCompleteCallback callback,
                      ServiceDiscoveryMockDelegate* mock_delegate)
      : started_resolving_(false),
        service_name_(service_name),
        callback_(std::move(callback)),
        mock_delegate_(mock_delegate) {}

  ~MockServiceResolver() override {}

  void StartResolving() override {
    started_resolving_ = true;
    mock_delegate_->ServiceResolverStarted(service_name_, this);
  }

  bool IsResolving() const {
    return started_resolving_;
  }

  std::string GetName() const override { return service_name_; }

  ServiceResolver::ResolveCompleteCallback* callback() { return &callback_; }

 private:
  bool started_resolving_;
  std::string service_name_;
  ServiceResolver::ResolveCompleteCallback callback_;
  ServiceDiscoveryMockDelegate* mock_delegate_;
};

class MockServiceDiscoveryClient : public ServiceDiscoveryClient {
 public:
  explicit MockServiceDiscoveryClient(
      ServiceDiscoveryMockDelegate* mock_delegate)
      : mock_delegate_(mock_delegate) {
  }

  ~MockServiceDiscoveryClient() override {}

  // Create a service watcher object listening for DNS-SD service announcements
  // on service type |service_type|.
  std::unique_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      ServiceWatcher::UpdatedCallback callback) override {
    return std::make_unique<MockServiceWatcher>(
        service_type, std::move(callback), mock_delegate_);
  }

  // Create a service resolver object for getting detailed service information
  // for the service called |service_name|.
  std::unique_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      ServiceResolver::ResolveCompleteCallback callback) override {
    return std::make_unique<MockServiceResolver>(
        service_name, std::move(callback), mock_delegate_);
  }

  // Not used in this test.
  std::unique_ptr<LocalDomainResolver> CreateLocalDomainResolver(
      const std::string& domain,
      net::AddressFamily address_family,
      LocalDomainResolver::IPAddressCallback callback) override {
    NOTREACHED();
    return std::unique_ptr<LocalDomainResolver>();
  }

 private:
  ServiceDiscoveryMockDelegate* mock_delegate_;
};

class MockServiceDiscoveryMockDelegate : public ServiceDiscoveryMockDelegate {
 public:
  MOCK_METHOD2(ServiceWatcherStarted, void(const std::string& service_type,
                                           MockServiceWatcher* watcher));
  MOCK_METHOD2(ServiceResolverStarted, void(const std::string& service_type,
                                            MockServiceResolver* resolver));
};

class MockDeviceListerDelegate : public PrivetDeviceLister::Delegate {
 public:
  MockDeviceListerDelegate() {}
  ~MockDeviceListerDelegate() override {}

  MOCK_METHOD2(DeviceChanged,
               void(const std::string& name,
                    const DeviceDescription& description));

  MOCK_METHOD1(DeviceRemoved, void(const std::string& name));

  MOCK_METHOD0(DeviceCacheFlushed, void());
};

class PrivetDeviceListerTest : public testing::Test {
 public:
  PrivetDeviceListerTest() : mock_client_(&mock_delegate_) {}
  ~PrivetDeviceListerTest() override {}

  void SetUp() override {
    example_attrs_.push_back("tXtvers=1");
    example_attrs_.push_back("ty=My Printer");
    example_attrs_.push_back("nOte=This is my Printer");
    example_attrs_.push_back("CS=ONlInE");
    example_attrs_.push_back("id=");

    service_description_.service_name = "myprinter._privet._tcp.local";
    service_description_.address = net::HostPortPair("myprinter.local", 6006);
    service_description_.metadata = example_attrs_;
    service_description_.last_seen = base::Time() +
        base::TimeDelta::FromSeconds(5);
    ASSERT_TRUE(service_description_.ip_address.AssignFromIPLiteral("1.2.3.4"));
  }

 protected:
  testing::StrictMock<MockServiceDiscoveryMockDelegate> mock_delegate_;
  MockServiceDiscoveryClient mock_client_;
  MockDeviceListerDelegate delegate_;
  std::vector<std::string> example_attrs_;
  ServiceDescription service_description_;
};

TEST_F(PrivetDeviceListerTest, SimpleUpdateTest) {
  DeviceDescription outgoing_description;

  MockServiceWatcher* service_watcher;
  MockServiceResolver* service_resolver;

  EXPECT_CALL(mock_delegate_,
              ServiceWatcherStarted("_privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_watcher));
  PrivetDeviceListerImpl privet_lister(&mock_client_, &delegate_);
  privet_lister.Start();
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(mock_delegate_,
              ServiceResolverStarted("myprinter._privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_resolver));
  service_watcher->callback().Run(ServiceWatcher::UPDATE_ADDED,
                                  "myprinter._privet._tcp.local");
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(delegate_, DeviceChanged("myprinter._privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&outgoing_description));

  std::move(*service_resolver->callback())
      .Run(ServiceResolver::STATUS_SUCCESS, service_description_);

  EXPECT_EQ(service_description_.address.host(),
            outgoing_description.address.host());
  EXPECT_EQ(service_description_.address.port(),
            outgoing_description.address.port());
  EXPECT_EQ("My Printer", outgoing_description.name);
  EXPECT_EQ("This is my Printer", outgoing_description.description);
  EXPECT_EQ("", outgoing_description.id);

  EXPECT_CALL(delegate_, DeviceRemoved("myprinter._privet._tcp.local"));

  service_watcher->callback().Run(ServiceWatcher::UPDATE_REMOVED,
                                  "myprinter._privet._tcp.local");
}

TEST_F(PrivetDeviceListerTest, MultipleUpdatesPostResolve) {
  MockServiceWatcher* service_watcher;
  MockServiceResolver* service_resolver;

  EXPECT_CALL(mock_delegate_,
              ServiceWatcherStarted("_privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_watcher));
  PrivetDeviceListerImpl privet_lister(&mock_client_, &delegate_);
  privet_lister.Start();
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(mock_delegate_,
              ServiceResolverStarted("myprinter._privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_resolver));

  service_watcher->callback().Run(ServiceWatcher::UPDATE_CHANGED,
                                  "myprinter._privet._tcp.local");
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(delegate_, DeviceChanged("myprinter._privet._tcp.local", _));
  std::move(*service_resolver->callback())
      .Run(ServiceResolver::STATUS_SUCCESS, service_description_);

  EXPECT_CALL(mock_delegate_,
              ServiceResolverStarted("myprinter._privet._tcp.local", _));
  service_watcher->callback().Run(ServiceWatcher::UPDATE_CHANGED,
                                  "myprinter._privet._tcp.local");
  testing::Mock::VerifyAndClear(&mock_delegate_);
}

// Check that the device lister does not create a still-working resolver
TEST_F(PrivetDeviceListerTest, MultipleUpdatesPreResolve) {
  MockServiceWatcher* service_watcher;

  EXPECT_CALL(mock_delegate_,
              ServiceWatcherStarted("_privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_watcher));
  PrivetDeviceListerImpl privet_lister(&mock_client_, &delegate_);
  privet_lister.Start();
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(mock_delegate_,
              ServiceResolverStarted("myprinter._privet._tcp.local", _))
      .Times(1);
  service_watcher->callback().Run(ServiceWatcher::UPDATE_CHANGED,
                                  "myprinter._privet._tcp.local");
  service_watcher->callback().Run(ServiceWatcher::UPDATE_CHANGED,
                                  "myprinter._privet._tcp.local");
}

TEST_F(PrivetDeviceListerTest, DiscoverNewDevices) {
  MockServiceWatcher* service_watcher;

  EXPECT_CALL(mock_delegate_,
              ServiceWatcherStarted("_privet._tcp.local", _))
      .WillOnce(SaveArg<1>(&service_watcher));
  PrivetDeviceListerImpl privet_lister(&mock_client_, &delegate_);
  privet_lister.Start();
  testing::Mock::VerifyAndClear(&mock_delegate_);

  EXPECT_CALL(*service_watcher, DiscoverNewServices());
  privet_lister.DiscoverNewDevices();
}


}  // namespace

}  // namespace cloud_print
