// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_CONNECTION_NOTIFIER_H_
#define ASH_COMPONENTS_ARC_SESSION_CONNECTION_NOTIFIER_H_

#include "base/observer_list.h"
#include "base/threading/thread_checker.h"

namespace arc {
namespace internal {

class ConnectionObserverBase;

// Manages events related to connection. Designed to be used only by
// ConnectionHolder.
class ConnectionNotifier {
 public:
  ConnectionNotifier();

  ConnectionNotifier(const ConnectionNotifier&) = delete;
  ConnectionNotifier& operator=(const ConnectionNotifier&) = delete;

  ~ConnectionNotifier();

  void AddObserver(ConnectionObserverBase* observer);
  void RemoveObserver(ConnectionObserverBase* observer);

  // Notifies observers that connection gets ready.
  void NotifyConnectionReady();

  // Notifies observers that connection is closed.
  void NotifyConnectionClosed();

 private:
  THREAD_CHECKER(thread_checker_);
  base::ObserverList<ConnectionObserverBase>::UncheckedAndDanglingUntriaged
      observer_list_;
};

}  // namespace internal
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_CONNECTION_NOTIFIER_H_
