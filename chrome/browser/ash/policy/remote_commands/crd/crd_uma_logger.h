// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_UMA_LOGGER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_UMA_LOGGER_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"

namespace policy {

// This class helps log CRD related UMA metrics.
class CrdUmaLogger {
 public:
  CrdUmaLogger(CrdSessionType session_type, UserSessionType user_session_type);
  CrdUmaLogger(const CrdUmaLogger&) = default;
  CrdUmaLogger& operator=(const CrdUmaLogger&) = default;

  // Logs the CRD session launch result to UMA.
  void LogSessionLaunchResult(ExtendedStartCrdSessionResultCode result_code);

  // Logs the CRD session duration to UMA.
  void LogSessionDuration(base::TimeDelta duration);

 private:
  std::string GetUmaHistogramName(const char* name_template) const;
  const char* FormatCrdSessionType() const;
  const char* FormatUserSessionType() const;

  CrdSessionType session_type_;
  UserSessionType user_session_type_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_UMA_LOGGER_H_
