// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/mojom/remote_support.mojom-forward.h"

namespace policy {

// Delegate that will start a session with the CRD native host.
// Will keep the session alive and active as long as this class lives.
// Deleting this class object will forcefully interrupt the active CRD session.
class CrdHostDelegate : public DeviceCommandStartCrdSessionJob::Delegate {
 public:
  // Proxy class to establish a connection with the Remoting service.
  // Overwritten in unittests to inject a test service.
  class RemotingServiceProxy {
   public:
    virtual ~RemotingServiceProxy() = default;

    using StartSessionCallback = base::OnceCallback<void(
        remoting::mojom::StartSupportSessionResponsePtr response)>;

    // Start a new remote support session. |callback| is
    // called with the result.
    virtual void StartSession(
        remoting::mojom::SupportSessionParamsPtr params,
        const remoting::ChromeOsEnterpriseParams& enterprise_params,
        StartSessionCallback callback) = 0;
  };

  CrdHostDelegate();
  explicit CrdHostDelegate(
      std::unique_ptr<RemotingServiceProxy> remoting_service);
  CrdHostDelegate(const CrdHostDelegate&) = delete;
  CrdHostDelegate& operator=(const CrdHostDelegate&) = delete;
  ~CrdHostDelegate() override;

  // DeviceCommandStartCrdSessionJob::Delegate implementation:
  bool HasActiveSession() const override;
  void TerminateSession(base::OnceClosure callback) override;
  void StartCrdHostAndGetCode(
      const SessionParameters& parameters,
      DeviceCommandStartCrdSessionJob::AccessCodeCallback success_callback,
      DeviceCommandStartCrdSessionJob::ErrorCallback error_callback,
      DeviceCommandStartCrdSessionJob::SessionEndCallback
          session_finished_callback) override;

 private:
  class CrdHostSession;

  std::unique_ptr<RemotingServiceProxy> remoting_service_;
  std::unique_ptr<CrdHostSession> active_session_;
};
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
