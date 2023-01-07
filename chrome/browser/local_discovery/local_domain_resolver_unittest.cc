// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/browser/local_discovery/service_discovery_client_impl.h"
#include "net/base/net_errors.h"
#include "net/dns/mdns_client_impl.h"
#include "net/dns/mock_mdns_socket_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace local_discovery {

namespace {

const uint8_t kSamplePacketA[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

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

const uint8_t kSamplePacketAAAA[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x1C,        // TYPE is AAAA.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 16 seconds.
  0x00, 0x10,
  0x00, 0x10,        // RDLENGTH is 4 bytes.
  0x00, 0x0A, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x00, 0x02,
  0x00, 0x03, 0x00, 0x04,
};

class LocalDomainResolverTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_EQ(net::OK, mdns_client_.StartListening(&socket_factory_));
  }

  std::string IPAddressToStringWithInvalid(const net::IPAddress& address) {
    if (!address.IsValid())
      return "";
    return address.ToString();
  }

  void AddressCallback(bool resolved,
                       const net::IPAddress& address_ipv4,
                       const net::IPAddress& address_ipv6) {
    AddressCallbackInternal(resolved,
                            IPAddressToStringWithInvalid(address_ipv4),
                            IPAddressToStringWithInvalid(address_ipv6));
  }

  void RunFor(base::TimeDelta time_period) {
    base::RunLoop run_loop;
    base::CancelableOnceClosure callback(run_loop.QuitWhenIdleClosure());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);
    run_loop.Run();
    callback.Cancel();
  }

  MOCK_METHOD3(AddressCallbackInternal,
               void(bool resolved,
                    std::string address_ipv4,
                    std::string address_ipv6));

  net::MockMDnsSocketFactory socket_factory_;
  net::MDnsClientImpl mdns_client_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(LocalDomainResolverTest, ResolveDomainA) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_IPV4,
      base::BindOnce(&LocalDomainResolverTest::AddressCallback,
                     base::Unretained(this)),
      &mdns_client_);

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(2);  // Twice per query

  resolver.Start();

  EXPECT_CALL(*this, AddressCallbackInternal(true, "1.2.3.4", ""));

  socket_factory_.SimulateReceive(kSamplePacketA, sizeof(kSamplePacketA));
}

TEST_F(LocalDomainResolverTest, ResolveDomainAAAA) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_IPV6,
      base::BindOnce(&LocalDomainResolverTest::AddressCallback,
                     base::Unretained(this)),
      &mdns_client_);

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(2);  // Twice per query

  resolver.Start();

  EXPECT_CALL(*this, AddressCallbackInternal(true, "", "a::1:2:3:4"));

  socket_factory_.SimulateReceive(kSamplePacketAAAA, sizeof(kSamplePacketAAAA));
}

TEST_F(LocalDomainResolverTest, ResolveDomainAnyOneAvailable) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_UNSPECIFIED,
      base::BindOnce(&LocalDomainResolverTest::AddressCallback,
                     base::Unretained(this)),
      &mdns_client_);

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(4);  // Twice per query

  resolver.Start();

  socket_factory_.SimulateReceive(kSamplePacketAAAA, sizeof(kSamplePacketAAAA));

  EXPECT_CALL(*this, AddressCallbackInternal(true, "", "a::1:2:3:4"));

  RunFor(base::Milliseconds(150));
}


TEST_F(LocalDomainResolverTest, ResolveDomainAnyBothAvailable) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_UNSPECIFIED,
      base::BindOnce(&LocalDomainResolverTest::AddressCallback,
                     base::Unretained(this)),
      &mdns_client_);

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(4);  // Twice per query

  resolver.Start();

  EXPECT_CALL(*this, AddressCallbackInternal(true, "1.2.3.4", "a::1:2:3:4"));

  socket_factory_.SimulateReceive(kSamplePacketAAAA, sizeof(kSamplePacketAAAA));

  socket_factory_.SimulateReceive(kSamplePacketA, sizeof(kSamplePacketA));
}

TEST_F(LocalDomainResolverTest, ResolveDomainNone) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_UNSPECIFIED,
      base::BindOnce(&LocalDomainResolverTest::AddressCallback,
                     base::Unretained(this)),
      &mdns_client_);

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(4);  // Twice per query

  resolver.Start();

  EXPECT_CALL(*this, AddressCallbackInternal(false, "", ""));

  RunFor(base::Seconds(4));
}

}  // namespace

}  // namespace local_discovery
