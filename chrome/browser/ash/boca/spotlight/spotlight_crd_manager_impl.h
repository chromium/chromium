// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "chrome/browser/ash/policy/remote_commands/crd/device_command_start_crd_session_job.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_crd_manager.h"
#include "components/prefs/pref_service.h"

namespace policy {

enum class ResultType;
}  // namespace policy

namespace ash::boca {

class SpotlightCrdManagerImpl : public SpotlightCrdManager {
  using ConnectionCodeCallback =
      base::OnceCallback<void(std::optional<std::string>)>;

 public:
  explicit SpotlightCrdManagerImpl(PrefService* pref_service);
  // Constructor used in unit tests. We use this controller to
  // initialize crd_job_ with `FakeStartCrdSessionJobDelegate`.
  explicit SpotlightCrdManagerImpl(
      std::unique_ptr<policy::DeviceCommandStartCrdSessionJob> crd_job);
  ~SpotlightCrdManagerImpl() override;

  // SpotlightCrdManager:
  void OnSessionStarted(const std::string& teacher_email) override;
  void OnSessionEnded() override;
  void InitiateSpotlightSession(ConnectionCodeCallback callback) override;

 private:
  void StartCrdResult(ConnectionCodeCallback,
                      policy::ResultType result,
                      std::optional<std::string> payload);

  // The CRD controller is used to interact with chrome services and the CRD
  // host. It provides a delegate for the crd_job to make these calls to CRD for
  // us.
  std::unique_ptr<policy::CrdAdminSessionController> crd_controller_;

  // The CRD job handles starting the CRD session and returning the connection
  // code.
  std::unique_ptr<policy::DeviceCommandStartCrdSessionJob> crd_job_;

  base::WeakPtrFactory<SpotlightCrdManagerImpl> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_IMPL_H_
