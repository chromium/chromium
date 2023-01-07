// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/features.h"

#include "build/build_config.h"

BASE_FEATURE(kSharingMatchPulseInterval,
             "SharingMatchPulseInterval",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kSharingPulseDeltaDesktopHours = {
    &kSharingMatchPulseInterval, "SharingPulseDeltaDesktopHours", 24};

const base::FeatureParam<int> kSharingPulseDeltaAndroidHours = {
    &kSharingMatchPulseInterval, "SharingPulseDeltaAndroidHours", 24};

BASE_FEATURE(kSharingMessageBridgeTimeout,
             "SharingMessageBridgeTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kSharingMessageBridgeTimeoutSeconds = {
    &kSharingMessageBridgeTimeout, "SharingMessageBridgeTimeoutSeconds", 8};

BASE_FEATURE(kSharingSendViaSync,
             "SharingSendViaSync",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharingPreferVapid,
             "SharingPreferVapid",
             base::FEATURE_DISABLED_BY_DEFAULT);
