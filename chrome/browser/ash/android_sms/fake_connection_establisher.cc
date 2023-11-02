// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/fake_connection_establisher.h"

namespace ash {
namespace android_sms {

FakeConnectionEstablisher::FakeConnectionEstablisher() = default;
FakeConnectionEstablisher::~FakeConnectionEstablisher() = default;

void FakeConnectionEstablisher::EstablishConnection(
    const GURL& url,
    ConnectionMode connection_mode,
    content::ServiceWorkerContext* service_worker_context_) {
  establish_connection_calls_.emplace_back(url, connection_mode,
                                           service_worker_context_);
}

void FakeConnectionEstablisher::TearDownConnection(
    const GURL& url,
    content::ServiceWorkerContext* service_worker_context_) {
  tear_down_connection_calls_.emplace_back(url, service_worker_context_);
}

}  // namespace android_sms
}  // namespace ash
