// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SHARED_CRD_SESSION_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SHARED_CRD_SESSION_IMPL_H_

#include <memory>

#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/policy/remote_commands/crd/start_crd_session_job_delegate.h"
#include "components/prefs/pref_service.h"

namespace policy {

// Implementation of the `SharedCrdSession`. Owns a
// `StartCrdSessionJobDelegate` to start the CRD session. The lifetime of the
//  Crd session is tied to this class.
class SharedCrdSessionImpl : public SharedCrdSession {
 public:
  using Delegate = StartCrdSessionJobDelegate;

  explicit SharedCrdSessionImpl(Delegate& delegate);

  // Constructor used in unit tests. By using this constructor we avoid the need
  // for a `DeviceOAuth2TokenService` to exist.
  SharedCrdSessionImpl(Delegate& delegate, std::string_view robot_account_id);

  ~SharedCrdSessionImpl() override;

  void StartCrdHost(const SessionParameters& parameters,
                    AccessCodeCallback success_callback,
                    ErrorCallback error_callback,
                    SessionFinishedCallback session_finished_callback) override;

  void TerminateSession() override;

 private:
  // The `StartCrdSessionJobDelegate` is used to interact with chrome services
  // and the CRD host.
  const raw_ref<Delegate> delegate_;

  std::string robot_account_id_;
};
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SHARED_CRD_SESSION_IMPL_H_
