// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager.h"

#include <memory>

#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"

namespace ash {
// Static
BocaManager* BocaManager::GetForProfile(Profile* profile) {
  return static_cast<BocaManager*>(
      BocaManagerFactory::GetInstance()->GetForProfile(profile));
}

BocaManager::BocaManager(Profile* profile) {
  boca_session_manager_ = std::make_unique<boca::BocaSessionManager>(
      ash::BrowserContextHelper::Get()
          ->GetUserByBrowserContext(profile)
          ->GetAccountId());
}

BocaManager::~BocaManager() = default;

}  // namespace ash
