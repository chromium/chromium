// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_UMA_LOGGER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_UMA_LOGGER_H_

#include <string>

#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"

namespace policy {

// This class helps log CRD related UMA metrics.
class CrdUmaLogger {
 public:
  CrdUmaLogger(CrdSessionType session_type, UserSessionType user_session_type);

  // Logs the CRD session launch result to UMA.
  void LogSessionLaunchResult(ResultCode code);

 private:
  std::string GetUmaHistogramName(const char* name_template) const;
  const char* FormatCrdSessionType() const;
  const char* FormatUserSessionType() const;

  CrdSessionType session_type_;
  UserSessionType user_session_type_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_UMA_LOGGER_H_
