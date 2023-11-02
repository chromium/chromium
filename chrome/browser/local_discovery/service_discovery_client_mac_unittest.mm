// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chrome/browser/local_discovery/service_discovery_client_mac.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/ip_endpoint.h"
#include "net/base/sockaddr_storage.h"
#include "testing/gtest_mac.h"

@interface TestNSNetService : NSNetService {
 @private
  base::scoped_nsobject<NSData> _data;
  base::scoped_nsobject<NSArray> _addresses;
}
- (instancetype)initWithData:(NSData*)data;
- (void)setAddresses:(NSArray*)addresses;
@end

@implementation TestNSNetService

- (instancetype)initWithData:(NSData*)data {
  if ((self = [super initWithDomain:@"" type:@"_tcp." name:@"Test.123"])) {
    _data.reset([data retain]);
  }
  return self;
}

- (void)setAddresses:(NSArray*)addresses {
  _addresses.reset([addresses copy]);
}

- (NSArray*)addresses {
  return _addresses;
}

- (NSData*)TXTRecordData {
  return _data;
}

@end

namespace local_discovery {

class ServiceDiscoveryClientMacTest : public CocoaTest {
 public:
  ServiceDiscoveryClientMacTest()
      : client_(new ServiceDiscoveryClientMac()),
        num_updates_(0),
        num_resolves_(0) {
  }

  void OnServiceUpdated(
      ServiceWatcher::UpdateType update,
      const std::string& service_name) {
    last_update_ = update;
    last_service_name_ = service_name;
    num_updates_++;
  }

  void OnResolveComplete(
      ServiceResolver::RequestStatus status,
      const ServiceDescription& service_description) {
    last_status_ = status;
    last_service_description_ = service_description;
    num_resolves_++;
  }

  ServiceDiscoveryClient* client() { return client_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<ServiceDiscoveryClientMac> client_;

  ServiceWatcher::UpdateType last_update_;
  std::string last_service_name_;
  int num_updates_;
  ServiceResolver::RequestStatus last_status_;
  ServiceDescription last_service_description_;
  int num_resolves_;
};

TEST_F(ServiceDiscoveryClientMacTest, ServiceWatcher) {
  const std::string test_service_type = "_testing._tcp.local";
  const std::string test_service_name = "Test.123";

  std::unique_ptr<ServiceWatcher> watcher = client()->CreateServiceWatcher(
      test_service_type,
      base::BindRepeating(&ServiceDiscoveryClientMacTest::OnServiceUpdated,
                          base::Unretained(this)));
  watcher->Start();

  // Weak pointer to implementation class.
  ServiceWatcherImplMac* watcher_impl =
      static_cast<ServiceWatcherImplMac*>(watcher.get());
  // Simulate service update events.
  watcher_impl->OnServicesUpdate(
      ServiceWatcher::UPDATE_ADDED, test_service_name);
  watcher_impl->OnServicesUpdate(
      ServiceWatcher::UPDATE_CHANGED, test_service_name);
  watcher_impl->OnServicesUpdate(
      ServiceWatcher::UPDATE_REMOVED, test_service_name);
  EXPECT_EQ(last_service_name_, test_service_name + "." + test_service_type);
  EXPECT_EQ(num_updates_, 3);
}

TEST_F(ServiceDiscoveryClientMacTest, DeleteWatcherAfterStart) {
  const std::string test_service_type = "_testing._tcp.local";

  std::unique_ptr<ServiceWatcher> watcher = client()->CreateServiceWatcher(
      test_service_type,
      base::BindRepeating(&ServiceDiscoveryClientMacTest::OnServiceUpdated,
                          base::Unretained(this)));
  watcher->Start();
  watcher.reset();

  EXPECT_EQ(0, num_updates_);
}

TEST_F(ServiceDiscoveryClientMacTest, DeleteResolverAfterStart) {
  const std::string test_service_name = "Test.123";

  std::unique_ptr<ServiceResolver> resolver = client()->CreateServiceResolver(
      test_service_name,
      base::BindRepeating(&ServiceDiscoveryClientMacTest::OnResolveComplete,
                          base::Unretained(this)));
  resolver->StartResolving();
  resolver.reset();

  EXPECT_EQ(0, num_resolves_);
}

TEST_F(ServiceDiscoveryClientMacTest, ParseServiceRecord) {
  const uint8_t record_bytes[] = {2, 'a', 'b', 3, 'd', '=', 'e'};
  base::scoped_nsobject<TestNSNetService> test_service([[TestNSNetService alloc]
      initWithData:[NSData dataWithBytes:record_bytes
                                  length:std::size(record_bytes)]]);

  const std::string kIp = "2001:4860:4860::8844";
  const uint16_t kPort = 4321;
  net::IPAddress ip_address;
  ASSERT_TRUE(ip_address.AssignFromIPLiteral(kIp));
  net::IPEndPoint endpoint(ip_address, kPort);
  net::SockaddrStorage storage;
  ASSERT_TRUE(endpoint.ToSockAddr(storage.addr, &storage.addr_len));
  NSData* discoveryHost =
      [NSData dataWithBytes:storage.addr length:storage.addr_len];
  NSArray* addresses = @[ discoveryHost ];
  [test_service setAddresses:addresses];

  ServiceDescription description;
  ParseNetService(test_service.get(), description);

  const std::vector<std::string>& metadata = description.metadata;
  EXPECT_EQ(2u, metadata.size());
  EXPECT_TRUE(base::Contains(metadata, "ab"));
  EXPECT_TRUE(base::Contains(metadata, "d=e"));

  EXPECT_EQ(ip_address, description.ip_address);
  EXPECT_EQ(kPort, description.address.port());
  EXPECT_EQ(kIp, description.address.host());
}

// https://crbug.com/586628
TEST_F(ServiceDiscoveryClientMacTest, ParseInvalidUnicodeRecord) {
  const uint8_t record_bytes[] = {
    3, 'a', '=', 'b',
    // The bytes after name= are the UTF-8 encoded representation of
    // U+1F4A9, with the first two bytes of the code unit sequence transposed.
    9, 'n', 'a', 'm', 'e', '=', 0x9F, 0xF0, 0x92, 0xA9,
    5, 'c', 'd', '=', 'e', '9',
  };
  base::scoped_nsobject<TestNSNetService> test_service([[TestNSNetService alloc]
      initWithData:[NSData dataWithBytes:record_bytes
                                  length:std::size(record_bytes)]]);

  const std::string kIp = "2001:4860:4860::8844";
  const uint16_t kPort = 4321;
  net::IPAddress ip_address;
  ASSERT_TRUE(ip_address.AssignFromIPLiteral(kIp));
  net::IPEndPoint endpoint(ip_address, kPort);
  net::SockaddrStorage storage;
  ASSERT_TRUE(endpoint.ToSockAddr(storage.addr, &storage.addr_len));
  NSData* discovery_host =
      [NSData dataWithBytes:storage.addr length:storage.addr_len];
  NSArray* addresses = @[ discovery_host ];
  [test_service setAddresses:addresses];

  ServiceDescription description;
  ParseNetService(test_service.get(), description);

  const std::vector<std::string>& metadata = description.metadata;
  EXPECT_EQ(2u, metadata.size());
  EXPECT_TRUE(base::Contains(metadata, "a=b"));
  EXPECT_TRUE(base::Contains(metadata, "cd=e9"));

  EXPECT_EQ(ip_address, description.ip_address);
  EXPECT_EQ(kPort, description.address.port());
  EXPECT_EQ(kIp, description.address.host());
}

TEST_F(ServiceDiscoveryClientMacTest, ResolveInvalidServiceName) {
  base::RunLoop run_loop;

  // This is the same invalid U+1F4A9 code unit sequence as in
  // ResolveInvalidUnicodeRecord.
  const std::string test_service_name =
      "Test\x9F\xF0\x92\xA9.123._testing._tcp.local";
  std::unique_ptr<ServiceResolver> resolver = client()->CreateServiceResolver(
      test_service_name, base::BindOnce(
                             [](ServiceDiscoveryClientMacTest* test,
                                base::OnceClosure quit_closure,
                                ServiceResolver::RequestStatus status,
                                const ServiceDescription& service_description) {
                               test->OnResolveComplete(status,
                                                       service_description);
                               std::move(quit_closure).Run();
                             },
                             base::Unretained(this), run_loop.QuitClosure()));
  resolver->StartResolving();

  run_loop.Run();

  EXPECT_EQ(1, num_resolves_);
  EXPECT_EQ(ServiceResolver::STATUS_KNOWN_NONEXISTENT, last_status_);
}

}  // namespace local_discovery
