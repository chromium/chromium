// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/incoming_connection.h"

#include "crypto/random.h"

namespace ash::quick_start {

IncomingConnection::IncomingConnection() {
  crypto::RandBytes(shared_secret_);
}

IncomingConnection::~IncomingConnection() = default;

}  // namespace ash::quick_start
