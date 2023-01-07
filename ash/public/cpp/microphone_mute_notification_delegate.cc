// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/microphone_mute_notification_delegate.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

MicrophoneMuteNotificationDelegate* g_instance = nullptr;

}  // namespace

// static
MicrophoneMuteNotificationDelegate* MicrophoneMuteNotificationDelegate::Get() {
  return g_instance;
}

MicrophoneMuteNotificationDelegate::MicrophoneMuteNotificationDelegate() {
  DCHECK(!g_instance);
  g_instance = this;
}

MicrophoneMuteNotificationDelegate::~MicrophoneMuteNotificationDelegate() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
