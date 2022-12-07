// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/sensor_disabled_notification_delegate.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

SensorDisabledNotificationDelegate* g_instance = nullptr;

}  // namespace

// static
SensorDisabledNotificationDelegate* SensorDisabledNotificationDelegate::Get() {
  return g_instance;
}

SensorDisabledNotificationDelegate::SensorDisabledNotificationDelegate() {
  DCHECK(!g_instance);
  g_instance = this;
}

SensorDisabledNotificationDelegate::~SensorDisabledNotificationDelegate() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
