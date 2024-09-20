// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/kcer_notifier_net.h"

#include "base/callback_list.h"

namespace kcer::internal {

KcerNotifierNet::KcerNotifierNet() = default;

KcerNotifierNet::~KcerNotifierNet() {
  net::CertDatabase::GetInstance()->RemoveObserver(this);
}

void KcerNotifierNet::Initialize() {
  net::CertDatabase::GetInstance()->AddObserver(this);
}

base::CallbackListSubscription KcerNotifierNet::AddObserver(
    base::RepeatingClosure callback) {
  return observers_.Add(std::move(callback));
}

void KcerNotifierNet::OnClientCertStoreChanged() {
  observers_.Notify();
}

}  // namespace kcer::internal
