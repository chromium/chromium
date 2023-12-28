// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_SUPPORT_HOST_OBSERVER_PROXY_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_SUPPORT_HOST_OBSERVER_PROXY_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "remoting/host/mojom/remote_support.mojom.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace policy {

class CrdSessionObserver;

// A proxy that translates and forwards `SupportHostObserver` events to the
// corresponding `CrdSessionObserver` events.
class SupportHostObserverProxy : public remoting::mojom::SupportHostObserver {
 public:
  SupportHostObserverProxy();
  SupportHostObserverProxy(const SupportHostObserverProxy&) = delete;
  SupportHostObserverProxy& operator=(const SupportHostObserverProxy&) = delete;
  ~SupportHostObserverProxy() override;

  void AddObserver(CrdSessionObserver* observer);
  void AddOwnedObserver(std::unique_ptr<CrdSessionObserver> observer);

  void Bind(
      mojo::PendingReceiver<remoting::mojom::SupportHostObserver> receiver);

  // `remoting::mojom::SupportHostObserver` implementation:
  void OnHostStateStarting() override;
  void OnHostStateRequestedAccessCode() override;
  void OnHostStateReceivedAccessCode(const std::string& access_code,
                                     base::TimeDelta lifetime) override;
  void OnHostStateConnecting() override;
  void OnHostStateConnected(const std::string& remote_username) override;
  void OnHostStateDisconnected(
      const std::optional<std::string>& disconnect_reason) override;
  void OnNatPolicyChanged(
      remoting::mojom::NatPolicyStatePtr nat_policy_state) override;
  void OnHostStateError(int64_t error_code) override;
  void OnPolicyError() override;
  void OnInvalidDomainError() override;

  void ReportHostStopped(ExtendedStartCrdSessionResultCode result,
                         std::string_view error_message);

 private:
  void OnMojomConnectionDropped();

  mojo::Receiver<remoting::mojom::SupportHostObserver> receiver_{this};
  base::ObserverList<CrdSessionObserver> observers_;
  std::vector<std::unique_ptr<CrdSessionObserver>> owned_session_observers_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_SUPPORT_HOST_OBSERVER_PROXY_H_
