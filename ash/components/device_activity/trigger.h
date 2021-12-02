// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_TRIGGER_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_TRIGGER_H_

namespace ash {
namespace device_activity {

// Device actives are measured according to trigger enums.
// TODO(https://crbug.com/1262178): Add another trigger for when sign-in occurs.
enum class Trigger {
  kNetwork  // Network state becomes connected.
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_TRIGGER_H_
