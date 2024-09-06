// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager.h"

#include <memory>

#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"

namespace ash {
BocaManager::BocaManager(
    std::unique_ptr<boca::OnTaskSessionManager> on_task_session_manager,
    std::unique_ptr<boca::SessionClientImpl> session_client_impl,
    std::unique_ptr<boca::BocaSessionManager> boca_session_manager)
    : on_task_session_manager_(std::move(on_task_session_manager)),
      session_client_impl_(std::move(session_client_impl)),
      boca_session_manager_(std::move(boca_session_manager)) {
  AddObservers();
}

// Static
BocaManager* BocaManager::GetForProfile(Profile* profile) {
  return static_cast<BocaManager*>(
      BocaManagerFactory::GetInstance()->GetForProfile(profile));
}

BocaManager::BocaManager(Profile* profile) {
  session_client_impl_ = std::make_unique<boca::SessionClientImpl>();
  boca_session_manager_ = std::make_unique<boca::BocaSessionManager>(
      session_client_impl_.get(), ash::BrowserContextHelper::Get()
                                      ->GetUserByBrowserContext(profile)
                                      ->GetAccountId());
  if (ash::boca_util::IsConsumer()) {
    auto on_task_system_web_app_manager =
        std::make_unique<boca::OnTaskSystemWebAppManagerImpl>(profile);
    on_task_session_manager_ = std::make_unique<boca::OnTaskSessionManager>(
        std::move(on_task_system_web_app_manager));
  }
  AddObservers();
}

void BocaManager::AddObservers() {
  if (ash::boca_util::IsConsumer()) {
    boca_session_manager_->AddObserver(on_task_session_manager_.get());
  }
}

BocaManager::~BocaManager() {
  boca_session_manager_->RemoveObserver(on_task_session_manager_.get());
}

}  // namespace ash
