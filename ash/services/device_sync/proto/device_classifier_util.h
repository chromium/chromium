// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_PROTO_DEVICE_CLASSIFIER_UTIL_H_
#define ASH_SERVICES_DEVICE_SYNC_PROTO_DEVICE_CLASSIFIER_UTIL_H_

#include "ash/services/device_sync/proto/cryptauth_api.pb.h"

namespace ash {

namespace device_sync {

namespace device_classifier_util {

const cryptauth::DeviceClassifier& GetDeviceClassifier();

}  // namespace device_classifier_util

}  // namespace device_sync

}  // namespace ash

#endif  // ASH_SERVICES_DEVICE_SYNC_PROTO_DEVICE_CLASSIFIER_UTIL_H_
