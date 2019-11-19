// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_FEATURES_H_
#define CHROME_BROWSER_SHARING_FEATURES_H_

#include "base/feature_list.h"

// Feature to allow device registration for sharing features.
extern const base::Feature kSharingDeviceRegistration;

// Feature flag to allow sharing infrastructure to register devices in
// DeviceInfo.
extern const base::Feature kSharingUseDeviceInfo;

// Feature flag to enable QR Code Generator (currently desktop-only).
extern const base::Feature kSharingQRCodeGenerator;

// Feature flag to enable deriving VAPID key from Sync.
extern const base::Feature kSharingDeriveVapidKey;

// Feature flag to enable device renaming.
extern const base::Feature kSharingRenameDevices;

#endif  // CHROME_BROWSER_SHARING_FEATURES_H_
