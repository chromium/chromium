// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/mock_remote_model_executor.h"

#include "base/check_op.h"

namespace optimization_guide {

MockRemoteModelExecutionSession::MockRemoteModelExecutionSession() = default;

MockRemoteModelExecutionSession::~MockRemoteModelExecutionSession() = default;

void MockRemoteModelExecutionSession::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockRemoteModelExecutionSession::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MockRemoteModelExecutionSession::SetConnectionState(
    ConnectionState state,
    std::optional<DisconnectReason> disconnect_reason) {
  CHECK_EQ(state == ConnectionState::kDisconnected,
           disconnect_reason.has_value());
  for (Observer& observer : observers_) {
    observer.OnConnectionStateChanged(state, disconnect_reason);
  }
}

}  // namespace optimization_guide
