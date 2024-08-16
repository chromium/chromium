// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_activation_observer_holder.h"

#include <map>
#include <utility>

#include "ash/public/cpp/session/session_activation_observer.h"
#include "base/notreached.h"

namespace ash {

SessionActivationObserverHolder::SessionActivationObserverHolder() = default;

SessionActivationObserverHolder::~SessionActivationObserverHolder() = default;

void SessionActivationObserverHolder::AddForAccountId(
    const AccountId& account_id,
    SessionActivationObserver* observer) {
  if (!account_id.is_valid())
    return;
  auto& observers = observer_map_[account_id];
  if (!observers)
    observers = std::make_unique<Observers>();
  observers->AddObserver(observer);
}

void SessionActivationObserverHolder::RemoveForAccountId(
    const AccountId& account_id,
    SessionActivationObserver* observer) {
  auto it = observer_map_.find(account_id);
  if (it == observer_map_.end()) {
    NOTREACHED();
  }
  it->second->RemoveObserver(observer);
}

void SessionActivationObserverHolder::NotifyActiveSessionChanged(
    const AccountId& from,
    const AccountId& to) {
  auto it = observer_map_.find(from);
  if (it != observer_map_.end()) {
    for (auto& observer : *it->second)
      observer.OnSessionActivated(false);
  }

  it = observer_map_.find(to);
  if (it != observer_map_.end()) {
    for (auto& observer : *it->second)
      observer.OnSessionActivated(true);
  }

  PruneObserverMap();
}

void SessionActivationObserverHolder::NotifyLockStateChanged(bool locked) {
  for (const auto& it : observer_map_) {
    for (auto& observer : *it.second)
      observer.OnLockStateChanged(locked);
  }

  PruneObserverMap();
}

void SessionActivationObserverHolder::PruneObserverMap() {
  std::erase_if(observer_map_, [](auto& item) { return item.second->empty(); });
}

}  // namespace ash
