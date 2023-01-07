// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_blocker.h"

#include "base/observer_list.h"

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
