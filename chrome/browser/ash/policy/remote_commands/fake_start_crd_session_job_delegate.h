// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_START_CRD_SESSION_JOB_DELEGATE_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_START_CRD_SESSION_JOB_DELEGATE_H_

#include "chrome/browser/ash/policy/remote_commands/start_crd_session_job_delegate.h"

#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

class FakeStartCrdSessionJobDelegate : public StartCrdSessionJobDelegate {
 public:
  static constexpr char kTestAccessCode[] = "111122223333";

  FakeStartCrdSessionJobDelegate();
  ~FakeStartCrdSessionJobDelegate() override;

  void SetHasActiveSession(bool value) { has_active_session_ = value; }
  void MakeAccessCodeFetchFail() { access_code_success_ = false; }
  void TerminateCrdSession(const base::TimeDelta& session_duration);

  // Returns if TerminateSession() was called to terminate the active session.
  bool IsActiveSessionTerminated() const { return terminate_session_called_; }

  // Returns the `SessionParameters` sent to the last StartCrdHostAndGetCode()
  // call.
  SessionParameters session_parameters() const;

  // `StartCrdSessionJobDelegate` implementation:
  bool HasActiveSession() const override;
  void TerminateSession(base::OnceClosure callback) override;
  void TryToReconnect(base::OnceClosure done_callback) override;
  void StartCrdHostAndGetCode(
      const SessionParameters& parameters,
      AccessCodeCallback success_callback,
      ErrorCallback error_callback,
      SessionEndCallback session_finished_callback) override;

 private:
  bool has_active_session_ = false;
  bool access_code_success_ = true;
  bool terminate_session_called_ = false;
  absl::optional<SessionParameters> received_session_parameters_;
  absl::optional<SessionEndCallback> session_finished_callback_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_START_CRD_SESSION_JOB_DELEGATE_H_
