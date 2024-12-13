// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/discovery/open_screen_listener.h"

#include "base/time/time.h"
#include "chrome/browser/local_discovery/fake_service_discovery_device_lister.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/osp/public/osp_constants.h"

namespace media_router {

using openscreen::osp::kOpenScreenServiceType;
using openscreen::osp::ServiceInfo;

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

  MOCK_METHOD1(OnError, void(const openscreen::Error&));
  MOCK_METHOD1(OnMetrics, void(openscreen::osp::ServiceListener::Metrics));
};

// Although the testing framework can do a byte comparison, when it fails
// it's difficult to figure out *exactly* what is wrong with the actual
// Service Info class.
MATCHER_P(ServiceInfoEquals, expected, "") {
  return (expected.instance_name == arg.instance_name) &&
         (expected.auth_token == arg.auth_token) &&
         (expected.fingerprint == arg.fingerprint) &&
         (expected.network_interface_index == arg.network_interface_index) &&
         (expected.v4_endpoint == arg.v4_endpoint) &&
         (expected.v6_endpoint == arg.v6_endpoint);
}

class FakeOpenScreenListener : public OpenScreenListener {
 public:
  explicit FakeOpenScreenListener(
      scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
          service_discovery_client)
      : OpenScreenListener(service_discovery_client) {}
  FakeOpenScreenListener(const FakeOpenScreenListener&) = delete;
  FakeOpenScreenListener& operator=(const FakeOpenScreenListener&) = delete;
  FakeOpenScreenListener(FakeOpenScreenListener&&) = delete;
  FakeOpenScreenListener& operator=(FakeOpenScreenListener&&) = delete;
  ~FakeOpenScreenListener() override = default;

 private:
  // Use a fake ServiceDiscoveryDeviceLister to avoid performing actual service
  // discovery operations.
  void CreateDeviceLister() override {
    CHECK(!device_lister_);

    device_lister_ =
        std::make_unique<local_discovery::FakeServiceDiscoveryDeviceLister>(
            /*task_runner=*/nullptr, kOpenScreenServiceType);
    device_lister_->Start();
  }
};

class OpenScreenListenerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    valid_description_.service_name = "mock_instance_name.test_service_type";
    valid_description_.address = net::HostPortPair("192.168.0.10", 8888);
    valid_description_.metadata = {"at=mock_auth_token", "fp=mock_fingerprint"};
    valid_description_.ip_address = net::IPAddress(192, 168, 0, 10);
    valid_description_.last_seen = base::Time();

    service_info_.instance_name = "mock_instance_name";
    service_info_.auth_token = "mock_auth_token";
    service_info_.fingerprint = "mock_fingerprint";
    service_info_.v4_endpoint =
        openscreen::IPEndpoint{openscreen::IPAddress(192, 168, 0, 10), 8888};
  }

  OpenScreenListenerTest() : listener_(nullptr), observer_() {
    listener_.AddObserver(observer_);
  }

  void ExpectReceiverAdded(const ServiceInfo& info) {
    EXPECT_CALL(observer_, OnReceiverAdded(ServiceInfoEquals(info)));
  }

  void ExpectReceiverChanged(const ServiceInfo& info) {
    EXPECT_CALL(observer_, OnReceiverChanged(ServiceInfoEquals(info)));
  }

  void ExpectReceiverRemoved(const ServiceInfo& info) {
    EXPECT_CALL(observer_, OnReceiverRemoved(ServiceInfoEquals(info)));
  }

  FakeOpenScreenListener listener_;
  ::testing::StrictMock<MockServiceListenerObserver> observer_;
  local_discovery::ServiceDescription valid_description_;
  ServiceInfo service_info_;
};

TEST_F(OpenScreenListenerTest, DeviceAddedNotifiesObserversIfStarted) {
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);

  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(1);
  listener_.Start();
  ExpectReceiverAdded(service_info_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);
}

TEST_F(OpenScreenListenerTest, DeviceChangedNotifiesObserversIfStarted) {
  listener_.OnDeviceChanged(kOpenScreenServiceType, false, valid_description_);

  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(1);
  listener_.Start();
  ExpectReceiverAdded(service_info_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);

  ExpectReceiverChanged(service_info_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, false, valid_description_);
}

TEST_F(OpenScreenListenerTest, DeviceRemovedNotifiesObserversIfStarted) {
  listener_.OnDeviceRemoved(kOpenScreenServiceType,
                            valid_description_.service_name);

  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(1);
  listener_.Start();
  ExpectReceiverAdded(service_info_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);

  ExpectReceiverRemoved(service_info_);
  listener_.OnDeviceRemoved(kOpenScreenServiceType,
                            valid_description_.service_name);
}

TEST_F(OpenScreenListenerTest, CachedFlushNotifiesObserversIfStarted) {
  listener_.OnDeviceCacheFlushed(kOpenScreenServiceType);

  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(1);
  listener_.Start();
  EXPECT_CALL(observer_, OnAllReceiversRemoved()).Times(1);
  listener_.OnDeviceCacheFlushed(kOpenScreenServiceType);
}

TEST_F(OpenScreenListenerTest, CachedFlushEmptiesReceiverList) {
  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(1);
  listener_.Start();

  ExpectReceiverAdded(service_info_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);

  ExpectReceiverAdded(service_info_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);

  EXPECT_EQ(2ul, listener_.GetReceivers().size());
  EXPECT_CALL(observer_, OnAllReceiversRemoved()).Times(1);
  listener_.OnDeviceCacheFlushed(kOpenScreenServiceType);
  EXPECT_EQ(0ul, listener_.GetReceivers().size());
}

TEST_F(OpenScreenListenerTest, StartNotifiesObservers) {
  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(1);
  listener_.Start();
}

TEST_F(OpenScreenListenerTest, StopNotifiesObservers) {
  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(1);
  EXPECT_CALL(observer_, OnStopped()).Times(1);

  listener_.Start();
  listener_.Stop();
}

TEST_F(OpenScreenListenerTest, SuspendAndResumeNotifyObservers) {
  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(2);
  EXPECT_CALL(observer_, OnSuspended()).Times(1);

  listener_.Start();
  listener_.Suspend();
  listener_.Resume();
}

TEST_F(OpenScreenListenerTest, SearchingNotifiesObservers) {
  EXPECT_CALL(observer_, OnStarted()).Times(1);
  EXPECT_CALL(observer_, OnSearching()).Times(2);

  listener_.Start();
  listener_.SearchNow();
}

TEST_F(OpenScreenListenerTest, RemovedObserversDoNotGetNotified) {
  listener_.RemoveObserver(observer_);

  listener_.Start();
  listener_.Stop();
  listener_.StartAndSuspend();
  listener_.Resume();
  listener_.SearchNow();
  listener_.Suspend();
  listener_.Resume();
  listener_.OnDeviceCacheFlushed(kOpenScreenServiceType);
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);
  listener_.OnDeviceRemoved(kOpenScreenServiceType,
                            valid_description_.service_name);
}

TEST_F(OpenScreenListenerTest, DoesNotRecordReceiversIfNotStarted) {
  EXPECT_EQ(0ul, listener_.GetReceivers().size());

  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, false, valid_description_);
  listener_.OnDeviceChanged(kOpenScreenServiceType, true, valid_description_);
  EXPECT_EQ(0ul, listener_.GetReceivers().size());
}

}  // namespace media_router
