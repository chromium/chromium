// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_IMPL_H_

#include <memory>
#include <optional>

#include "ash/boca/spotlight/spotlight_notification_bubble_controller.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session_provider.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_crd_manager.h"
#include "components/prefs/pref_service.h"

namespace ash::boca {

class SpotlightCrdManagerImpl : public SpotlightCrdManager {
  using ConnectionCodeCallback = base::OnceCallback<void(const std::string&)>;

 public:
  explicit SpotlightCrdManagerImpl(PrefService* pref_service);
  // Constructor used in unit tests. We use this controller to
  // provide a fake `policy::SharedCrdSession`.
  explicit SpotlightCrdManagerImpl(
      std::unique_ptr<policy::SharedCrdSession> crd_session,
      std::unique_ptr<SpotlightNotificationBubbleController>
          persistent_bubble_controller);
  ~SpotlightCrdManagerImpl() override;

  // SpotlightCrdManager:
  void OnSessionEnded() override;
  void InitiateSpotlightSession(ConnectionCodeCallback callback,
                                const std::string& requester_email) override;
  void ShowPersistentNotification(const std::string& teacher_name) override;
  void HidePersistentNotification() override;

 private:
  // Owns the `policy::CrdAdminSessionController` which provides a
  // `policy::StartCrdSessionJobDelegate`. The delegate is what actually
  // interacts with the CRD host and the `policy::SharedCrdSession` calls makes
  // calls to the delegate. Ensure the provider is declared before the shared
  // crd session so that the CRD internals are destructed properly.
  std::unique_ptr<policy::SharedCrdSessionProvider> provider_;

  // The CrdSession handles talking directly with the CRD service.
  std::unique_ptr<policy::SharedCrdSession> crd_session_;

  const std::unique_ptr<ash::SpotlightNotificationBubbleController>
      persistent_bubble_controller_;

  base::WeakPtrFactory<SpotlightCrdManagerImpl> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_IMPL_H_
