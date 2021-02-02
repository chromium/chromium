// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/features.h"

#include "build/build_config.h"

const base::Feature kSharingQRCodeGenerator{"SharingQRCodeGenerator",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSharingDeviceExpiration{"SharingDeviceExpiration",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kSharingDeviceExpirationHours = {
    &kSharingDeviceExpiration, "SharingDeviceExpirationHours", 48};

const base::Feature kSharingMatchPulseInterval{
    "SharingMatchPulseInterval", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kSharingPulseDeltaDesktopHours = {
    &kSharingMatchPulseInterval, "SharingPulseDeltaDesktopHours", 24};

const base::FeatureParam<int> kSharingPulseDeltaAndroidHours = {
    &kSharingMatchPulseInterval, "SharingPulseDeltaAndroidHours", 24};

const base::Feature kSharingMessageTTL{"SharingMessageTTL",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kSharingMessageTTLSeconds = {
    &kSharingMessageTTL, "SharingMessageTTLSeconds", 16};

const base::Feature kSharingAckMessageTTL{"SharingAckMessageTTL",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kSharingAckMessageTTLSeconds = {
    &kSharingAckMessageTTL, "SharingAckMessageTTLSeconds", 8};

const base::Feature kSharingMessageBridgeTimeout{
    "SharingMessageBridgeTimeout", base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<int> kSharingMessageBridgeTimeoutSeconds = {
    &kSharingMessageBridgeTimeout, "SharingMessageBridgeTimeoutSeconds", 5};

const base::Feature kSharingSendViaSync{"SharingSendViaSync",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSharingPreferVapid {
  "SharingPreferVapid",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};
