// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/util/histogram_util.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {
namespace secure_channel {
namespace util {
namespace {
using location::nearby::connections::mojom::Status;
}  // namespace

void RecordStartDiscoveryResult(Status status) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.OperationResult.StartDiscovery",
      status);
}

void RecordInjectEndpointResult(Status status) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.OperationResult.InjectEndpoint",
      status);
}

void RecordStopDiscoveryResult(Status status) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.OperationResult.StopDiscovery", status);
}

void RecordRequestConnectionResult(Status status) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.OperationResult.RequestConnection",
      status);
}

void RecordAcceptConnectionResult(Status status) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.OperationResult.AcceptConnection",
      status);
}

void RecordSendPayloadResult(Status status) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.OperationResult.SendPayload", status);
}

void RecordDisconnectFromEndpointResult(Status status) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.OperationResult.DisconnectFromEndpoint",
      status);
}

void LogMessageAction(MessageAction message_action) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.MessageAction", message_action);
}

void RecordNearbyDisconnection(NearbyDisconnectionReason reason) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.DisconnectionReason", reason);
}

}  // namespace util
}  // namespace secure_channel
}  // namespace chromeos
