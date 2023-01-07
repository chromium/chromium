// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"

#include "base/check_op.h"

namespace ash {

// static
ArcNotificationSurfaceManager* ArcNotificationSurfaceManager::instance_ =
    nullptr;

ArcNotificationSurfaceManager::ArcNotificationSurfaceManager() {
  DCHECK(!instance_);
  instance_ = this;
}

ArcNotificationSurfaceManager::~ArcNotificationSurfaceManager() {
  DCHECK_EQ(this, instance_);
  instance_ = nullptr;
}

// static
ArcNotificationSurfaceManager* ArcNotificationSurfaceManager::Get() {
  return instance_;
}

}  // namespace ash
