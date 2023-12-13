// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_START_CRD_SESSION_JOB_DELEGATE_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_START_CRD_SESSION_JOB_DELEGATE_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"

namespace policy {

// Delegate of `DeviceCommandStartCrdSessionJob` that will handle all required
// communication with the remoting code to start and end CRD sessions.
class StartCrdSessionJobDelegate {
 public:
  using AccessCodeCallback = base::OnceCallback<void(const std::string&)>;
  using ErrorCallback =
      base::OnceCallback<void(ExtendedStartCrdSessionResultCode,
                              const std::string&)>;
  using SessionEndCallback = base::OnceCallback<void(base::TimeDelta)>;

  // Session parameters used to start the CRD host.
  struct SessionParameters {
    SessionParameters();
    ~SessionParameters();

    SessionParameters(const SessionParameters&);
    SessionParameters& operator=(const SessionParameters&);
    SessionParameters(SessionParameters&&);
    SessionParameters& operator=(SessionParameters&&);

    std::string user_name = "";
    std::optional<std::string> admin_email;
    bool terminate_upon_input = false;
    bool show_confirmation_dialog = false;
    bool curtain_local_user_session = false;
    bool allow_troubleshooting_tools = false;
    bool show_troubleshooting_tools = false;
    bool allow_reconnections = false;
    bool allow_file_transfer = false;
  };

  virtual ~StartCrdSessionJobDelegate() = default;

  // Checks if an active CRD session exists.
  virtual bool HasActiveSession() const = 0;

  // Terminates the currently active CRD session.
  virtual void TerminateSession() = 0;

  // Attempts to start CRD host and get Auth Code.
  // `session_finished_callback` is invoked when an active crd session is
  // terminated.
  virtual void StartCrdHostAndGetCode(
      const SessionParameters& parameters,
      AccessCodeCallback success_callback,
      ErrorCallback error_callback,
      SessionEndCallback session_finished_callback) = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_START_CRD_SESSION_JOB_DELEGATE_H_
