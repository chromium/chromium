// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/secure_channel/pending_connection_request_delegate.h"

namespace chromeos {

namespace secure_channel {

namespace {
using Reason = PendingConnectionRequestDelegate::FailedConnectionReason;
}  // namespace

PendingConnectionRequestDelegate::PendingConnectionRequestDelegate() = default;

PendingConnectionRequestDelegate::~PendingConnectionRequestDelegate() = default;

std::ostream& operator<<(std::ostream& stream, const Reason& reason) {
  switch (reason) {
    case Reason::kRequestCanceledByClient:
      stream << "[request canceled by client]";
      break;
    case Reason::kRequestFailed:
      stream << "[request failed]";
      break;
  }

  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
