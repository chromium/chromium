// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager.h"

#include <memory>

#include "chrome/browser/ash/boca/on_task/on_task_extensions_manager_impl.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/user_manager/user.h"

namespace ash {

BocaManager::BocaManager(
    std::unique_ptr<boca::OnTaskSessionManager> on_task_session_manager,
    std::unique_ptr<boca::SessionClientImpl> session_client_impl,
    std::unique_ptr<boca::BocaSessionManager> boca_session_manager,
    std::unique_ptr<boca::InvalidationServiceImpl> invalidation_service_impl,
    std::unique_ptr<boca::BabelOrcaManager> babel_orca_manager)
    : on_task_session_manager_(std::move(on_task_session_manager)),
      session_client_impl_(std::move(session_client_impl)),
      boca_session_manager_(std::move(boca_session_manager)),
      invalidation_service_impl_(std::move(invalidation_service_impl)),
      babel_orca_manager_(std::move(babel_orca_manager)) {
  AddObservers();
}

BocaManager::BocaManager(Profile* profile) {
  session_client_impl_ = std::make_unique<boca::SessionClientImpl>();
  boca_session_manager_ = std::make_unique<boca::BocaSessionManager>(
      session_client_impl_.get(), ash::BrowserContextHelper::Get()
                                      ->GetUserByBrowserContext(profile)
                                      ->GetAccountId());
  babel_orca_manager_ = std::make_unique<boca::BabelOrcaManager>();

  if (ash::boca_util::IsConsumer()) {
    auto on_task_system_web_app_manager =
        std::make_unique<boca::OnTaskSystemWebAppManagerImpl>(profile);
    auto on_task_extensions_manager =
        std::make_unique<boca::OnTaskExtensionsManagerImpl>(profile);
    on_task_session_manager_ = std::make_unique<boca::OnTaskSessionManager>(
        std::move(on_task_system_web_app_manager),
        std::move(on_task_extensions_manager));
  }

  gcm::GCMDriver* gcm_driver =
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver();
  invalidation_service_impl_ = std::make_unique<boca::InvalidationServiceImpl>(
      gcm_driver, instance_id_driver,
      ash::BrowserContextHelper::Get()
          ->GetUserByBrowserContext(profile)
          ->GetAccountId(),
      boca_session_manager_.get(), session_client_impl_.get());
  AddObservers();
}

BocaManager::~BocaManager() {}

void BocaManager::Shutdown() {
  invalidation_service_impl_->ShutDown();
  // Dependencies like GCM driver is teardown in Shutdown phase. Reset now to
  // avoid dangling pointer.
  invalidation_service_impl_.reset();
  for (auto& obs : boca_session_manager_->observers()) {
    boca_session_manager_->RemoveObserver(&obs);
  }
}

void BocaManager::AddObservers() {
  boca_session_manager_->AddObserver(babel_orca_manager_.get());
  if (ash::boca_util::IsConsumer()) {
    boca_session_manager_->AddObserver(on_task_session_manager_.get());
  }
}

}  // namespace ash
