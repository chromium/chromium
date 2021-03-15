// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONSTANTS_H_

#include "base/time/time.h"

// Timeout for reading a response frame from remote device.
constexpr base::TimeDelta kReadResponseFrameTimeout =
    base::TimeDelta::FromSeconds(60);

// Timeout for initiating a connection to a remote device.
constexpr base::TimeDelta kInitiateNearbyConnectionTimeout =
    base::TimeDelta::FromSeconds(60);

// The delay before the sender will disconnect from the receiver after sending a
// file. Note that the receiver is expected to immediately disconnect, so this
// delay is a worst-effort disconnection. Disconnecting too early may interrupt
// in flight packets, especially over WiFi LAN.
constexpr base::TimeDelta kOutgoingDisconnectionDelay =
    base::TimeDelta::FromSeconds(60);

// The delay before the receiver will disconnect from the sender after rejecting
// an incoming file. The sender is expected to disconnect immediately after
// reading the rejection frame.
constexpr base::TimeDelta kIncomingRejectionDelay =
    base::TimeDelta::FromSeconds(2);

// The delay before the initiator of the cancellation will disconnect from the
// other device. The device that did not initiate the cancellation is expected
// to disconnect immediately after reading the cancellation frame.
constexpr base::TimeDelta kInitiatorCancelDelay =
    base::TimeDelta::FromSeconds(5);

// Timeout for reading a frame from remote device.
constexpr base::TimeDelta kReadFramesTimeout = base::TimeDelta::FromSeconds(15);

// Time to delay running the task to invalidate send and receive surfaces.
constexpr base::TimeDelta kInvalidateDelay =
    base::TimeDelta::FromMilliseconds(500);

// Time between successive progress updates.
constexpr base::TimeDelta kMinProgressUpdateFrequency =
    base::TimeDelta::FromMilliseconds(100);

// TODO(crbug.com/1129069): Set this to true when WiFi LAN is supported to
// enable logic that checks for an internet connection for managing surfaces and
// the utility process lifecycle.
constexpr bool kIsWifiLanSupported = false;

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONSTANTS_H_
