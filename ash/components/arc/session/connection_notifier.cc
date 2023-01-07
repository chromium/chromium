// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/connection_notifier.h"

#include "ash/components/arc/session/connection_observer.h"

namespace arc {
namespace internal {

ConnectionNotifier::ConnectionNotifier() = default;

ConnectionNotifier::~ConnectionNotifier() = default;

void ConnectionNotifier::AddObserver(ConnectionObserverBase* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
}

void ConnectionNotifier::RemoveObserver(ConnectionObserverBase* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void ConnectionNotifier::NotifyConnectionReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observer_list_)
    observer.OnConnectionReady();
}

void ConnectionNotifier::NotifyConnectionClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observer_list_)
    observer.OnConnectionClosed();
}

}  // namespace internal
}  // namespace arc
