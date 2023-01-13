// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"

namespace ash {
namespace secure_channel {
namespace util {

// Logs the result of Nearby Connections API functions.
void RecordStartDiscoveryResult(::nearby::connections::mojom::Status status);
void RecordInjectEndpointResult(::nearby::connections::mojom::Status status);
void RecordStopDiscoveryResult(::nearby::connections::mojom::Status status);
void RecordRequestConnectionResult(::nearby::connections::mojom::Status status);
void RecordAcceptConnectionResult(::nearby::connections::mojom::Status status);
void RecordSendPayloadResult(::nearby::connections::mojom::Status status);
void RecordDisconnectFromEndpointResult(
    ::nearby::connections::mojom::Status status);
void RecordRegisterPayloadFilesResult(
    ::nearby::connections::mojom::Status status);

// Enumeration of possible file payload transfer actions via Nearby Connection
// library. Keep in sync with corresponding enum in
// tools/metrics/histograms/enums.xml. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class FileAction {
  kRegisteredFileReceived = 0,
  kUnexpectedFileReceived = 1,
  kMaxValue = kUnexpectedFileReceived,
};

// Logs an action related to a file transfer.
void LogFileAction(FileAction file_action);

// Enumeration of possible results of a file transfer via Nearby Connections
// library. Keep in sync with corresponding enum in
// tools/metrics/histograms/enums.xml. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class FileTransferResult {
  kFileTransferSuccess = 0,
  kFileTransferFailure = 1,
  kFileTransferCanceled = 2,
  kMaxValue = kFileTransferCanceled,
};

// Logs the result of a file transfer.
void LogFileTransferResult(FileTransferResult file_transfer_result);

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
  kReceivedUnregisteredFilePayload = 14,
  kMaxValue = kReceivedUnregisteredFilePayload
};

void RecordNearbyDisconnection(NearbyDisconnectionReason reason);

}  // namespace util
}  // namespace secure_channel
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_UTIL_HISTOGRAM_UTIL_H_
