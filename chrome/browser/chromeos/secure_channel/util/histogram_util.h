// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_

#include "chromeos/services/nearby/public/mojom/nearby_connections_types.mojom.h"

namespace chromeos {
namespace secure_channel {
namespace util {

// Logs the result of Nearby Connections API functions.
void RecordStartDiscoveryResult(
    location::nearby::connections::mojom::Status status);
void RecordInjectEndpointResult(
    location::nearby::connections::mojom::Status status);
void RecordStopDiscoveryResult(
    location::nearby::connections::mojom::Status status);
void RecordRequestConnectionResult(
    location::nearby::connections::mojom::Status status);
void RecordAcceptConnectionResult(
    location::nearby::connections::mojom::Status status);
void RecordSendPayloadResult(
    location::nearby::connections::mojom::Status status);
void RecordDisconnectFromEndpointResult(
    location::nearby::connections::mojom::Status status);

// Enumeration of possible message transfer action via Nearby Connection
// library. Keep in sync with corresponding enum in
// tools/metrics/histograms/enums.xml. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class MessageAction {
  kMessageSent = 0,
  kMessageReceived = 1,
  kMaxValue = kMessageReceived,
};

// Logs a given message transfer action.
void LogMessageAction(MessageAction message_action);

// Reasons why a Nearby Connection may become disconnected. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class NearbyDisconnectionReason {
  kDisconnectionRequestedByClient = 0,
  kFailedDiscovery = 1,
  kTimeoutDuringDiscovery = 2,
  kFailedRequestingConnection = 3,
  kTimeoutDuringRequestConnection = 4,
  kFailedAcceptingConnection = 5,
  kTimeoutDuringAcceptConnection = 6,
  kConnectionRejected = 7,
  kTimeoutWaitingForConnectionAccepted = 8,
  kSendMessageFailed = 9,
  kReceivedUnexpectedPayloadType = 10,
  kConnectionLost = 11,
  kNearbyProcessCrash = 12,
  kNearbyProcessMojoDisconnection = 13,
  kMaxValue = kNearbyProcessMojoDisconnection
};

void RecordNearbyDisconnection(NearbyDisconnectionReason reason);

}  // namespace util
}  // namespace secure_channel
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_
