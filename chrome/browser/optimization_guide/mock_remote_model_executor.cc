// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/mock_remote_model_executor.h"

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
    ConnectionState state) {
  for (Observer& observer : observers_) {
    observer.OnConnectionStateChanged(state);
  }
}

}  // namespace optimization_guide
