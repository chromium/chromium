// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_CONNECTIONS_H_
#define CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_CONNECTIONS_H_

#include "chrome/services/sharing/public/mojom/nearby_connections.mojom.h"

#include "testing/gmock/include/gmock/gmock.h"

using NearbyConnectionsMojom =
    location::nearby::connections::mojom::NearbyConnections;
using AdvertisingOptionsPtr =
    location::nearby::connections::mojom::AdvertisingOptionsPtr;
using ConnectionLifecycleListener =
    location::nearby::connections::mojom::ConnectionLifecycleListener;
using ConnectionOptionsPtr =
    location::nearby::connections::mojom::ConnectionOptionsPtr;
using DiscoveryOptionsPtr =
    location::nearby::connections::mojom::DiscoveryOptionsPtr;
using EndpointDiscoveryListener =
    location::nearby::connections::mojom::EndpointDiscoveryListener;
using PayloadListener = location::nearby::connections::mojom::PayloadListener;
using PayloadPtr = location::nearby::connections::mojom::PayloadPtr;

class MockNearbyConnections : public NearbyConnectionsMojom {
 public:
  MockNearbyConnections();
  MockNearbyConnections(const MockNearbyConnections&) = delete;
  MockNearbyConnections& operator=(const MockNearbyConnections&) = delete;
  ~MockNearbyConnections() override;

  MOCK_METHOD(void,
              StartAdvertising,
              (const std::vector<uint8_t>& endpoint_info,
               const std::string& service_id,
               AdvertisingOptionsPtr,
               mojo::PendingRemote<ConnectionLifecycleListener>,
               StartDiscoveryCallback),
              (override));
  MOCK_METHOD(void, StopAdvertising, (StopAdvertisingCallback), (override));
  MOCK_METHOD(void,
              StartDiscovery,
              (const std::string& service_id,
               DiscoveryOptionsPtr,
               mojo::PendingRemote<EndpointDiscoveryListener>,
               StartDiscoveryCallback),
              (override));
  MOCK_METHOD(void, StopDiscovery, (StopDiscoveryCallback), (override));
  MOCK_METHOD(void,
              RequestConnection,
              (const std::vector<uint8_t>& endpoint_info,
               const std::string& endpoint_id,
               ConnectionOptionsPtr options,
               mojo::PendingRemote<ConnectionLifecycleListener>,
               RequestConnectionCallback),
              (override));
  MOCK_METHOD(void,
              DisconnectFromEndpoint,
              (const std::string& endpoint_id, DisconnectFromEndpointCallback),
              (override));
  MOCK_METHOD(void,
              AcceptConnection,
              (const std::string& endpoint_id,
               mojo::PendingRemote<PayloadListener> listener,
               AcceptConnectionCallback callback),
              (override));
  MOCK_METHOD(void,
              RejectConnection,
              (const std::string& endpoint_id,
               RejectConnectionCallback callback),
              (override));
  MOCK_METHOD(void,
              SendPayload,
              (const std::vector<std::string>& endpoint_ids,
               PayloadPtr payload,
               SendPayloadCallback callback),
              (override));
  MOCK_METHOD(void,
              CancelPayload,
              (int64_t payload_id, CancelPayloadCallback callback),
              (override));
  MOCK_METHOD(void,
              StopAllEndpoints,
              (DisconnectFromEndpointCallback callback),
              (override));
  MOCK_METHOD(void,
              InitiateBandwidthUpgrade,
              (const std::string& endpoint_id,
               InitiateBandwidthUpgradeCallback callback),
              (override));
  MOCK_METHOD(void,
              RegisterPayloadFile,
              (int64_t payload_id,
               base::File input_file,
               base::File output_file,
               RegisterPayloadFileCallback callback),
              (override));
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_CONNECTIONS_H_
