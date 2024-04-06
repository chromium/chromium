// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_MOCK_ADB_SERVER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_MOCK_ADB_SERVER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

// Single instance mock ADB server for use in browser tests. Runs on IO thread.

// These methods can be called from any thread.
enum FlushMode {
  FlushWithoutSize,
  FlushWithSize,
  FlushWithData
};

void StartMockAdbServer(FlushMode flush_mode);
void StopMockAdbServer();

// Part of mock server independent of transport.
class MockAndroidConnection {
 public:
  class Delegate {
   public:
    virtual void SendSuccess(const std::string& message) {}
    virtual void SendRaw(const std::string& data) {}
    virtual void Close() {}
    virtual ~Delegate() {}
  };

  MockAndroidConnection(Delegate* delegate,
                        const std::string& serial,
                        const std::string& command);
  virtual ~MockAndroidConnection();

  void Receive(const std::string& data);

 private:
  void ProcessCommand(const std::string& command);
  void SendHTTPResponse(const std::string& body);

  raw_ptr<Delegate> delegate_;
  std::string serial_;
  std::string socket_name_;
  std::string request_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_MOCK_ADB_SERVER_H_
