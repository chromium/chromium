// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_SESSION_ACTIVATION_OBSERVER_HOLDER_H_
#define ASH_SESSION_SESSION_ACTIVATION_OBSERVER_HOLDER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/account_id/account_id.h"

namespace ash {

class SessionActivationObserver;

class SessionActivationObserverHolder {
 public:
  SessionActivationObserverHolder();
  ~SessionActivationObserverHolder();

  void AddForAccountId(const AccountId& account_id,
                       SessionActivationObserver* observer);
  void RemoveForAccountId(const AccountId& account_id,
                          SessionActivationObserver* observer);

  void NotifyActiveSessionChanged(const AccountId& from, const AccountId& to);

  void NotifyLockStateChanged(bool locked);

 private:
  void PruneObserverMap();

  using Observers = base::ObserverList<SessionActivationObserver>;
  std::map<AccountId, std::unique_ptr<Observers>> observer_map_;

  DISALLOW_COPY_AND_ASSIGN(SessionActivationObserverHolder);
};

}  // namespace ash

#endif  // ASH_SESSION_SESSION_ACTIVATION_OBSERVER_HOLDER_H_
