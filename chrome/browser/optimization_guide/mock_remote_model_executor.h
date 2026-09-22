// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_REMOTE_MODEL_EXECUTOR_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_REMOTE_MODEL_EXECUTOR_H_

#include "base/observer_list.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

// Mocks the streaming session returned by
// RemoteModelExecutor::StartStreamingSession so tests can observe the frames a
// feature sends without any network access.
class MockRemoteModelExecutionSession : public RemoteModelExecutionSession {
 public:
  MockRemoteModelExecutionSession();
  ~MockRemoteModelExecutionSession() override;

  MOCK_METHOD(void, Send, (const google::protobuf::MessageLite&), (override));

  // These are real implementations, rather than mocks, since the observer list
  // bookkeeping they do is required for SetConnectionState() to work.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Notifies observers that the connection changed to `state`, as the real
  // session does when its WebSocket connects or disconnects.
  void SetConnectionState(ConnectionState state);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_REMOTE_MODEL_EXECUTOR_H_
