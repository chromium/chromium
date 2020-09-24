// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_blocker.h"

NotificationBlocker::NotificationBlocker() = default;

NotificationBlocker::~NotificationBlocker() = default;

void NotificationBlocker::NotifyBlockingStateChanged() {
  for (auto& observer : observers_)
    observer.OnBlockingStateChanged();
}

void NotificationBlocker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NotificationBlocker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
