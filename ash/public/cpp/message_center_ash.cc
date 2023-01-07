// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/message_center_ash.h"

#include "base/check_op.h"
#include "base/logging.h"

namespace ash {

namespace {
MessageCenterAsh* g_instance = nullptr;
}  // namespace

MessageCenterAsh::MessageCenterAsh() {
  g_instance = this;
}

MessageCenterAsh::~MessageCenterAsh() {
  g_instance = nullptr;
}

// static
MessageCenterAsh* MessageCenterAsh::Get() {
  return g_instance;
}

// static
void MessageCenterAsh::SetForTesting(MessageCenterAsh* message_center_ash) {
  g_instance = message_center_ash;
}

void MessageCenterAsh::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MessageCenterAsh::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MessageCenterAsh ::NotifyOnQuietModeChanged(bool in_quiet_mode) {
  for (auto& observer : observer_list_)
    observer.OnQuietModeChanged(in_quiet_mode);
}

}  // namespace ash
