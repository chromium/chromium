// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/message_center_ash_impl.h"

#include "ui/message_center/message_center.h"

namespace ash {

MessageCenterAshImpl::MessageCenterAshImpl() {
  message_center::MessageCenter::Get()->AddObserver(this);
}

MessageCenterAshImpl::~MessageCenterAshImpl() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
}

void MessageCenterAshImpl ::OnQuietModeChanged(bool in_quiet_mode) {
  NotifyOnQuietModeChanged(in_quiet_mode);
}

void MessageCenterAshImpl ::SetQuietMode(bool in_quiet_mode) {
  message_center::MessageCenter::Get()->SetQuietMode(in_quiet_mode);
}

bool MessageCenterAshImpl ::IsQuietMode() const {
  return message_center::MessageCenter::Get()->IsQuietMode();
}

}  // namespace ash
