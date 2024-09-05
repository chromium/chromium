// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_KCER_NOTIFIER_NET_H_
#define ASH_COMPONENTS_KCER_KCER_NOTIFIER_NET_H_

#include "base/callback_list.h"
#include "net/cert/cert_database.h"

namespace kcer::internal {

// A helper class that implements notifications for Kcer. This implementation
// just listens to the notifications from net::CertDatabase and forwards them to
// the observers.
// In the future this is planned to be replaced by listening to notifications
// from Chaps. KcerToken-s will receive notifications related to them and
// forward them to KcerImpl to notify the observers.
class KcerNotifierNet : public net::CertDatabase::Observer {
 public:
  KcerNotifierNet();
  ~KcerNotifierNet() override;

  // Starts observing the notifications from net::CertDatabase.
  void Initialize();

  base::CallbackListSubscription AddObserver(base::RepeatingClosure callback);

  // Implements net::CertDatabase::Observer
  void OnClientCertStoreChanged() override;

 private:
  base::RepeatingCallbackList<void()> observers_;
};

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_KCER_NOTIFIER_NET_H_
