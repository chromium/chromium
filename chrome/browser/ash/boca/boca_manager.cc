// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager.h"

#include <memory>

#include "chrome/browser/ash/boca/boca_app_client_impl.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"

namespace ash {
// Static
BocaManager* BocaManager::GetForProfile(Profile* profile) {
  return static_cast<BocaManager*>(
      BocaManagerFactory::GetInstance()->GetForProfile(profile));
}

BocaManager::BocaManager(Profile* profile)
    : boca_app_client_impl_{std::make_unique<BocaAppClientImpl>()} {}

BocaManager::~BocaManager() = default;

}  // namespace ash
