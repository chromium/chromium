// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SESSION_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SESSION_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

namespace policy {

enum class ResultCode;

class CrdSessionObserver : public base::CheckedObserver {
 public:
  CrdSessionObserver() = default;
  CrdSessionObserver(CrdSessionObserver&) = delete;
  CrdSessionObserver& operator=(CrdSessionObserver&) = delete;
  ~CrdSessionObserver() override = default;

  // Invoked when the CRD host has successfully been started and when it was
  // able to generate an access code.
  virtual void OnHostStarted(const std::string& access_code) {}

  // Invoked when the remote admin used the access code to actually start a
  // CRD connection.
  virtual void OnClientConnected() {}

  // Invoked when the remote admin disconnects.
  virtual void OnClientDisconnected() {}

  // Invoked when the CRD host stopped, or when it failed to start.
  virtual void OnHostStopped(ResultCode result, const std::string& message) {}
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SESSION_OBSERVER_H_
