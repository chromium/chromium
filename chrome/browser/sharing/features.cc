// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/features.h"

const base::Feature kSharingDeviceRegistration{
    "SharingDeviceRegistration", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSharingUseDeviceInfo{"SharingUseDeviceInfo",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharingQRCodeGenerator{"SharingQRCodeGenerator",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharingDeriveVapidKey{"SharingDeriveVapidKey",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharingRenameDevices{"SharingRenameDevices",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
