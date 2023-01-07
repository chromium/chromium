// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/message_center/arc_notifications_host_initializer.h"

#include "base/check_op.h"

namespace ash {

namespace {
ArcNotificationsHostInitializer* g_instance = nullptr;
}

// static
ArcNotificationsHostInitializer* ArcNotificationsHostInitializer::Get() {
  return g_instance;
}

ArcNotificationsHostInitializer::ArcNotificationsHostInitializer() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ArcNotificationsHostInitializer::~ArcNotificationsHostInitializer() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
