// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/browser/local_discovery/service_discovery_client_impl.h"
#include "net/base/net_errors.h"
#include "net/dns/mdns_client_impl.h"
#include "net/dns/mock_mdns_socket_factory.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::Return;
using ::testing::Exactly;

namespace local_discovery {

namespace {

const uint8_t kSamplePacketPTR[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 1 second.
  0x00, 0x01,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x0c
};

const uint8_t kSamplePacketSRV[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x05, 'h', 'e', 'l', 'l', 'o',
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x21,        // TYPE is SRV.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 1 second.
  0x00, 0x01,
  0x00, 0x15,        // RDLENGTH is 21 bytes.
  0x00, 0x00,
  0x00, 0x00,
  0x22, 0xb8,  // port 8888
  0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
};

const uint8_t kSamplePacketTXT[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x05, 'h', 'e', 'l', 'l', 'o',
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x10,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  0x00, 0x01,
  0x00, 0x06,        // RDLENGTH is 21 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o'
};

const uint8_t kSamplePacketSRVA[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x02,               // 2 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x05, 'h', 'e', 'l', 'l', 'o',
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x21,        // TYPE is SRV.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 16 seconds.
  0x00, 0x10,
  0x00, 0x15,        // RDLENGTH is 21 bytes.
  0x00, 0x00,
  0x00, 0x00,
  0x22, 0xb8,  // port 8888
  0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,

  0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x01,        // TYPE is A.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 16 seconds.
  0x00, 0x10,
  0x00, 0x04,        // RDLENGTH is 4 bytes.
  0x01, 0x02,
  0x03, 0x04,
};

const uint8_t kSamplePacketPTR2[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x02,               // 2 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x02, 0x00,        // TTL (4 bytes) is 1 second.
  0x00, 0x01,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'g', 'd', 'b', 'y', 'e',
  0xc0, 0x0c,

  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x02, 0x00,        // TTL (4 bytes) is 1 second.
  0x00, 0x01,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x0c
};

const uint8_t kSamplePacketQuerySRV[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x00, 0x00,               // No flags.
  0x00, 0x01,               // One question.
  0x00, 0x00,               // 0 RRs (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  // Question
  0x05, 'h', 'e', 'l', 'l', 'o',
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x21,        // TYPE is SRV.
  0x00, 0x01,        // CLASS is IN.
};


class MockServiceWatcherClient {
 public:
  MOCK_METHOD2(OnServiceUpdated,
               void(ServiceWatcher::UpdateType, const std::string&));

  ServiceWatcher::UpdatedCallback GetCallback() {
    return base::BindRepeating(&MockServiceWatcherClient::OnServiceUpdated,
                               base::Unretained(this));
  }
};

class ServiceDiscoveryTest : public ::testing::Test {
 public:
  ServiceDiscoveryTest()
      : service_discovery_client_(&mdns_client_) {
    EXPECT_EQ(net::OK, mdns_client_.StartListening(&socket_factory_));
  }

  ~ServiceDiscoveryTest() override {}

 protected:
  void RunFor(base::TimeDelta time_period) {
    base::RunLoop run_loop;
    base::CancelableOnceClosure callback(run_loop.QuitWhenIdleClosure());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);
    run_loop.Run();
    callback.Cancel();
  }


  net::MockMDnsSocketFactory socket_factory_;
  net::MDnsClientImpl mdns_client_;
  ServiceDiscoveryClientImpl service_discovery_client_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ServiceDiscoveryTest, AddRemoveService) {
  StrictMock<MockServiceWatcherClient> delegate;

  std::unique_ptr<ServiceWatcher> watcher(
      service_discovery_client_.CreateServiceWatcher("_privet._tcp.local",
                                                     delegate.GetCallback()));

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_.SimulateReceive(kSamplePacketPTR, sizeof(kSamplePacketPTR));

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_REMOVED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  RunFor(base::Seconds(2));
}

TEST_F(ServiceDiscoveryTest, DiscoverNewServices) {
  StrictMock<MockServiceWatcherClient> delegate;

  std::unique_ptr<ServiceWatcher> watcher(
      service_discovery_client_.CreateServiceWatcher("_privet._tcp.local",
                                                     delegate.GetCallback()));

  watcher->Start();

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(2);

  watcher->DiscoverNewServices();

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(2);

  RunFor(base::Seconds(2));
}

// Test that we can query the network with a service name that includes
// characters beyond those explicitly allowed in DNS such as spaces and parens.
TEST_F(ServiceDiscoveryTest, DiscoverNewServicesUnrestricted) {
  StrictMock<MockServiceWatcherClient> delegate;

  std::unique_ptr<ServiceWatcher> watcher =
      service_discovery_client_.CreateServiceWatcher(
          "foo bar(A1B2)_ipp._tcp.local", delegate.GetCallback());

  watcher->Start();

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(2);

  watcher->DiscoverNewServices();

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(2);

  RunFor(base::Seconds(2));
}

TEST_F(ServiceDiscoveryTest, ReadCachedServices) {
  socket_factory_.SimulateReceive(kSamplePacketPTR, sizeof(kSamplePacketPTR));
  StrictMock<MockServiceWatcherClient> delegate;

  std::unique_ptr<ServiceWatcher> watcher(
      service_discovery_client_.CreateServiceWatcher("_privet._tcp.local",
                                                     delegate.GetCallback()));

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  base::RunLoop().RunUntilIdle();
}


TEST_F(ServiceDiscoveryTest, ReadCachedServicesMultiple) {
  socket_factory_.SimulateReceive(kSamplePacketPTR2, sizeof(kSamplePacketPTR2));

  StrictMock<MockServiceWatcherClient> delegate;
  std::unique_ptr<ServiceWatcher> watcher =
      service_discovery_client_.CreateServiceWatcher("_privet._tcp.local",
                                                     delegate.GetCallback());

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "gdbye._privet._tcp.local"))
      .Times(Exactly(1));

  base::RunLoop().RunUntilIdle();
}


TEST_F(ServiceDiscoveryTest, OnServiceChanged) {
  StrictMock<MockServiceWatcherClient> delegate;
  std::unique_ptr<ServiceWatcher> watcher(
      service_discovery_client_.CreateServiceWatcher("_privet._tcp.local",
                                                     delegate.GetCallback()));

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_.SimulateReceive(kSamplePacketPTR, sizeof(kSamplePacketPTR));

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_CHANGED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_.SimulateReceive(kSamplePacketSRV, sizeof(kSamplePacketSRV));

  socket_factory_.SimulateReceive(kSamplePacketTXT, sizeof(kSamplePacketTXT));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceDiscoveryTest, SinglePacket) {
  StrictMock<MockServiceWatcherClient> delegate;
  std::unique_ptr<ServiceWatcher> watcher(
      service_discovery_client_.CreateServiceWatcher("_privet._tcp.local",
                                                     delegate.GetCallback()));

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_.SimulateReceive(kSamplePacketPTR, sizeof(kSamplePacketPTR));

  // Reset the "already updated" flag.
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_CHANGED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_.SimulateReceive(kSamplePacketSRV, sizeof(kSamplePacketSRV));

  socket_factory_.SimulateReceive(kSamplePacketTXT, sizeof(kSamplePacketTXT));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceDiscoveryTest, ActivelyRefreshServices) {
  StrictMock<MockServiceWatcherClient> delegate;
  std::unique_ptr<ServiceWatcher> watcher(
      service_discovery_client_.CreateServiceWatcher("_privet._tcp.local",
                                                     delegate.GetCallback()));

  watcher->Start();
  watcher->SetActivelyRefreshServices(true);

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  std::string query_packet = std::string((const char*)(kSamplePacketQuerySRV),
                                         sizeof(kSamplePacketQuerySRV));

  EXPECT_CALL(socket_factory_, OnSendTo(query_packet))
      .Times(2);

  socket_factory_.SimulateReceive(kSamplePacketPTR, sizeof(kSamplePacketPTR));

  base::RunLoop().RunUntilIdle();

  socket_factory_.SimulateReceive(kSamplePacketSRV, sizeof(kSamplePacketSRV));

  EXPECT_CALL(socket_factory_, OnSendTo(query_packet))
      .Times(4);  // IPv4 and IPv6 at 85% and 95%

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_REMOVED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  RunFor(base::Seconds(2));

  base::RunLoop().RunUntilIdle();
}


class ServiceResolverTest : public ServiceDiscoveryTest {
 public:
  ServiceResolverTest() {
    metadata_expected_.push_back("hello");
    address_expected_ = net::HostPortPair("myhello.local", 8888);
    EXPECT_TRUE(ip_address_expected_.AssignFromIPLiteral("1.2.3.4"));
  }

  ~ServiceResolverTest() override {}

  void SetUp() override {
    resolver_ = service_discovery_client_.CreateServiceResolver(
        "hello._privet._tcp.local",
        base::BindOnce(&ServiceResolverTest::OnFinishedResolving,
                       base::Unretained(this)));
  }

  void OnFinishedResolving(ServiceResolver::RequestStatus request_status,
                           const ServiceDescription& service_description) {
    OnFinishedResolvingInternal(request_status,
                                service_description.address.ToString(),
                                service_description.metadata,
                                service_description.ip_address);
  }

  MOCK_METHOD4(OnFinishedResolvingInternal,
               void(ServiceResolver::RequestStatus,
                    const std::string&,
                    const std::vector<std::string>&,
                    const net::IPAddress&));

 protected:
  std::unique_ptr<ServiceResolver> resolver_;
  net::IPAddress ip_address_;
  net::HostPortPair address_expected_;
  std::vector<std::string> metadata_expected_;
  net::IPAddress ip_address_expected_;
};

TEST_F(ServiceResolverTest, TxtAndSrvButNoA) {
  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(4);

  resolver_->StartResolving();

  socket_factory_.SimulateReceive(kSamplePacketSRV, sizeof(kSamplePacketSRV));

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(
      *this, OnFinishedResolvingInternal(ServiceResolver::STATUS_SUCCESS,
                                         address_expected_.ToString(),
                                         metadata_expected_, net::IPAddress()));

  socket_factory_.SimulateReceive(kSamplePacketTXT, sizeof(kSamplePacketTXT));
}

TEST_F(ServiceResolverTest, TxtSrvAndA) {
  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(4);

  resolver_->StartResolving();

  EXPECT_CALL(*this,
              OnFinishedResolvingInternal(ServiceResolver::STATUS_SUCCESS,
                                          address_expected_.ToString(),
                                          metadata_expected_,
                                          ip_address_expected_));

  socket_factory_.SimulateReceive(kSamplePacketTXT, sizeof(kSamplePacketTXT));

  socket_factory_.SimulateReceive(kSamplePacketSRVA, sizeof(kSamplePacketSRVA));
}

TEST_F(ServiceResolverTest, JustSrv) {
  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(4);

  resolver_->StartResolving();

  EXPECT_CALL(*this,
              OnFinishedResolvingInternal(ServiceResolver::STATUS_SUCCESS,
                                          address_expected_.ToString(),
                                          std::vector<std::string>(),
                                          ip_address_expected_));

  socket_factory_.SimulateReceive(kSamplePacketSRVA, sizeof(kSamplePacketSRVA));

  // TODO(noamsml): When NSEC record support is added, change this to use an
  // NSEC record.
  RunFor(base::Seconds(4));
}

TEST_F(ServiceResolverTest, WithNothing) {
  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(4);

  resolver_->StartResolving();

  EXPECT_CALL(*this, OnFinishedResolvingInternal(
                         ServiceResolver::STATUS_REQUEST_TIMEOUT, _, _, _));

  // TODO(noamsml): When NSEC record support is added, change this to use an
  // NSEC record.
  RunFor(base::Seconds(4));
}

}  // namespace

}  // namespace local_discovery
