// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/discovery/open_screen_listener.h"

#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using openscreen::osp::ServiceInfo;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace media_router {

const char kServiceType[] = "openscreen_.udp_";

class MockServiceListenerObserver
    : public openscreen::osp::ServiceListener::Observer {
 public:
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnSuspended, void());
  MOCK_METHOD0(OnSearching, void());

  MOCK_METHOD1(OnReceiverAdded, void(const ServiceInfo&));
  MOCK_METHOD1(OnReceiverChanged, void(const ServiceInfo&));
  MOCK_METHOD1(OnReceiverRemoved, void(const ServiceInfo&));
  MOCK_METHOD0(OnAllReceiversRemoved, void());

  MOCK_METHOD1(OnError, void(openscreen::osp::ServiceListenerError));
  MOCK_METHOD1(OnMetrics, void(openscreen::osp::ServiceListener::Metrics));
};

// Although the testing framework can do a byte comparison, when it fails
// it's difficult to figure out *exactly* what is wrong with the actual
// Service Info class.
MATCHER_P(ServiceInfoEquals, expected, "") {
  return (expected.service_id == arg.service_id) &&
         (expected.friendly_name == arg.friendly_name) &&
         (expected.network_interface_index == arg.network_interface_index) &&
         (expected.v4_endpoint == arg.v4_endpoint) &&
         (expected.v6_endpoint == arg.v6_endpoint);
}

class OpenScreenListenerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    valid_description_.service_name = "mock_service.test_service_type";
    valid_description_.address = net::HostPortPair("192.168.0.10", 8888);
    valid_description_.metadata = {"foo", "bar", "baz"};
    valid_description_.ip_address = net::IPAddress(192, 168, 0, 10);
    valid_description_.last_seen = base::Time();

    service_info_.service_id = "mock_service.test_service_type";
    service_info_.friendly_name = "mock_service";
    service_info_.v4_endpoint =
        openscreen::IPEndpoint{openscreen::IPAddress(192, 168, 0, 10), 8888};
    service_info_.v6_endpoint = {};
  }

  OpenScreenListenerTest() : listener(kServiceType), observer() {
    listener.AddObserver(&observer);
  }

  void ExpectReceiverAdded(const ServiceInfo& info) {
    EXPECT_CALL(observer, OnReceiverAdded(ServiceInfoEquals(info)));
  }

  void ExpectReceiverChanged(const ServiceInfo& info) {
    EXPECT_CALL(observer, OnReceiverChanged(ServiceInfoEquals(info)));
  }

  void ExpectReceiverRemoved(const ServiceInfo& info) {
    EXPECT_CALL(observer, OnReceiverRemoved(ServiceInfoEquals(info)));
  }

  OpenScreenListener listener;
  StrictMock<MockServiceListenerObserver> observer;
  local_discovery::ServiceDescription valid_description_;
  ServiceInfo service_info_;
};

TEST_F(OpenScreenListenerTest, DeviceAddedNotifiesObserversIfStarted) {
  listener.OnDeviceChanged(kServiceType, true, valid_description_);

  EXPECT_CALL(observer, OnStarted()).Times(1);
  listener.Start();
  ExpectReceiverAdded(service_info_);
  listener.OnDeviceChanged(kServiceType, true, valid_description_);
}

TEST_F(OpenScreenListenerTest, DeviceChangedNotifiesObserversIfStarted) {
  listener.OnDeviceChanged(kServiceType, false, valid_description_);

  EXPECT_CALL(observer, OnStarted()).Times(1);
  listener.Start();
  ExpectReceiverAdded(service_info_);
  listener.OnDeviceChanged(kServiceType, true, valid_description_);

  ExpectReceiverChanged(service_info_);
  listener.OnDeviceChanged(kServiceType, false, valid_description_);
}

TEST_F(OpenScreenListenerTest, DeviceRemovedNotifiesObserversIfStarted) {
  listener.OnDeviceRemoved(kServiceType, valid_description_.service_name);

  EXPECT_CALL(observer, OnStarted()).Times(1);
  listener.Start();
  ExpectReceiverAdded(service_info_);
  listener.OnDeviceChanged(kServiceType, true, valid_description_);

  ExpectReceiverRemoved(service_info_);
  listener.OnDeviceRemoved(kServiceType, valid_description_.service_name);
}

TEST_F(OpenScreenListenerTest, CachedFlushNotifiesObserversIfStarted) {
  listener.OnDeviceCacheFlushed(kServiceType);

  EXPECT_CALL(observer, OnStarted()).Times(1);
  listener.Start();
  EXPECT_CALL(observer, OnAllReceiversRemoved()).Times(1);
  listener.OnDeviceCacheFlushed(kServiceType);
}

TEST_F(OpenScreenListenerTest, CachedFlushEmptiesReceiverList) {
  EXPECT_CALL(observer, OnStarted()).Times(1);
  listener.Start();

  ExpectReceiverAdded(service_info_);
  listener.OnDeviceChanged(kServiceType, true, valid_description_);

  ExpectReceiverAdded(service_info_);
  listener.OnDeviceChanged(kServiceType, true, valid_description_);

  EXPECT_EQ(2ul, listener.GetReceivers().size());
  EXPECT_CALL(observer, OnAllReceiversRemoved()).Times(1);
  listener.OnDeviceCacheFlushed(kServiceType);
  EXPECT_EQ(0ul, listener.GetReceivers().size());
}

TEST_F(OpenScreenListenerTest, StartNotifiesObservers) {
  EXPECT_CALL(observer, OnStarted()).Times(1);
  listener.Start();
}

TEST_F(OpenScreenListenerTest, StopNotifiesObservers) {
  EXPECT_CALL(observer, OnStarted()).Times(1);
  EXPECT_CALL(observer, OnStopped()).Times(1);

  listener.Start();
  listener.Stop();
}

TEST_F(OpenScreenListenerTest, SuspendNotifiesObservers) {
  EXPECT_CALL(observer, OnStarted()).Times(2);
  EXPECT_CALL(observer, OnSuspended()).Times(2);

  listener.Start();
  listener.Suspend();
  listener.StartAndSuspend();
}

TEST_F(OpenScreenListenerTest, ResumeNotifiesObservers) {
  EXPECT_CALL(observer, OnStarted()).Times(2);
  EXPECT_CALL(observer, OnSuspended()).Times(1);

  listener.Start();
  listener.Suspend();
  listener.Resume();
}

TEST_F(OpenScreenListenerTest, SearchingNotifiesObservers) {
  EXPECT_CALL(observer, OnStarted()).Times(1);
  listener.Start();
  EXPECT_CALL(observer, OnSearching()).Times(1);
  listener.SearchNow();
}

TEST_F(OpenScreenListenerTest, RemovedObserversDoNotGetNotified) {
  listener.RemoveObserver(&observer);

  listener.Start();
  listener.Stop();
  listener.StartAndSuspend();
  listener.Resume();
  listener.SearchNow();
  listener.Suspend();
  listener.Resume();
  listener.OnDeviceCacheFlushed(kServiceType);
  listener.OnDeviceChanged(kServiceType, true, valid_description_);
  listener.OnDeviceChanged(kServiceType, true, valid_description_);
  listener.OnDeviceRemoved(kServiceType, valid_description_.service_name);
}

TEST_F(OpenScreenListenerTest, DoesNotRecordReceiversIfNotStarted) {
  EXPECT_EQ(0ul, listener.GetReceivers().size());

  listener.OnDeviceChanged(kServiceType, true, valid_description_);
  listener.OnDeviceChanged(kServiceType, false, valid_description_);
  listener.OnDeviceChanged(kServiceType, true, valid_description_);
  EXPECT_EQ(0ul, listener.GetReceivers().size());
}
}  // namespace media_router
