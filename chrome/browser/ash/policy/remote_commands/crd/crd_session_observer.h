// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_SESSION_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_SESSION_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"

namespace policy {

class CrdSessionObserver : public base::CheckedObserver {
 public:
  CrdSessionObserver() = default;
  CrdSessionObserver(CrdSessionObserver&) = delete;
  CrdSessionObserver& operator=(CrdSessionObserver&) = delete;
  ~CrdSessionObserver() override = default;

  // Invoked when the CRD host has successfully been started.
  virtual void OnHostStarted() {}

  // Invoked when the CRD host was able to generate an access code.
  virtual void OnAccessCodeReceived(const std::string& access_code) {}

  // Invoked when the remote admin used the access code to actually start a
  // CRD connection.
  virtual void OnClientConnecting() {}
  virtual void OnClientConnected() {}

  // Invoked when the remote admin disconnects.
  virtual void OnClientDisconnected() {}

  // Invoked when the CRD host stopped, or when it failed to start.
  virtual void OnHostStopped(ExtendedStartCrdSessionResultCode result,
                             const std::string& message) {}
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_SESSION_OBSERVER_H_
