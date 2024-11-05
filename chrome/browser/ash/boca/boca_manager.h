// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_H_
#define CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_H_

#include "chrome/browser/ash/boca/boca_app_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"
#include "chromeos/ash/components/boca/boca_metrics_manager.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {
// Manages boca main business logic.
class BocaManager : public KeyedService {
 public:
  // Constructor used only in test
  BocaManager(
      std::unique_ptr<boca::OnTaskSessionManager> on_task_session_manager,
      std::unique_ptr<boca::SessionClientImpl> session_client_impl,
      std::unique_ptr<boca::BocaSessionManager> boca_session_manager,
      std::unique_ptr<boca::InvalidationServiceImpl> invalidation_service_impl,
      std::unique_ptr<boca::BabelOrcaManager> babel_orca_manager,
      std::unique_ptr<boca::BocaMetricsManager> boca_metrics_manager);

  BocaManager(Profile* profile, const std::string& application_locale);
  ~BocaManager() override;

  // KeyedService:
  void Shutdown() override;

  boca::BocaSessionManager* GetBocaSessionManager() {
    return boca_session_manager_.get();
  }

  boca::OnTaskSessionManager* GetOnTaskSessionManagerForTesting() {
    return on_task_session_manager_.get();
  }
  boca::BabelOrcaManager* GetBabelOrcaManagerForTesting() {
    return babel_orca_manager_.get();
  }

  boca::BocaMetricsManager* GetBocaMetricsManagerForTesting() {
    return boca_metrics_manager_.get();
  }

 private:
  void AddObservers(const user_manager::User* user);

  std::unique_ptr<boca::OnTaskSessionManager> on_task_session_manager_;
  std::unique_ptr<boca::SessionClientImpl> session_client_impl_;
  std::unique_ptr<boca::BocaSessionManager> boca_session_manager_;
  std::unique_ptr<boca::InvalidationServiceImpl> invalidation_service_impl_;
  std::unique_ptr<boca::BabelOrcaManager> babel_orca_manager_;
  std::unique_ptr<boca::BocaMetricsManager> boca_metrics_manager_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_H_
