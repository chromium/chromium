// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_CONSTANTS_H_
#define CHROME_BROWSER_SHARING_SHARING_CONSTANTS_H_

#include "base/time/time.h"
#include "net/base/backoff_entry.h"

// App ID linked to FCM messages for Sharing.
extern const char kSharingFCMAppID[];

// Sender ID for Sharing.
extern const char kSharingSenderID[];

// Amount of time before a message is considered timeout if no ack is received.
extern const base::TimeDelta kSendMessageTimeout;

// Amount of time before an ack message is expired.
extern const base::TimeDelta kAckTimeToLive;

// Amount of offline time before decice is consider expired.
extern const base::TimeDelta kDeviceExpiration;

// Amount of time before FCM registration should happen again.
extern const base::TimeDelta kRegistrationExpiration;

// Backoff policy for registration retry.
extern const net::BackoffEntry::Policy kRetryBackoffPolicy;

// Maximum number of devices to be shown in dialog and context menu.
extern const int kMaxDevicesShown;

// Command id for first device shown in submenu.
extern const int kSubMenuFirstDeviceCommandId;

// Command id for last device shown in submenu.
extern const int kSubMenuLastDeviceCommandId;

// The feature name prefix used in metrics name.
enum class SharingFeatureName {
  kUnknown,
  kClickToCall,
  kSharedClipboard,
  kMaxValue = kSharedClipboard,
};

#endif  // CHROME_BROWSER_SHARING_SHARING_CONSTANTS_H_
