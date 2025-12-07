// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_client_factory_impl.h"

#include <memory>

#include "ash/lobster/lobster_controller.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "chrome/browser/ash/lobster/lobster_client_impl.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "chrome/browser/ash/lobster/lobster_service_provider.h"
#include "chrome/browser/profiles/profile_manager.h"

LobsterClientFactoryImpl::LobsterClientFactoryImpl(
    ash::LobsterController* controller)
    : controller_(controller) {
  controller_->SetClientFactory(this);
}

LobsterClientFactoryImpl::~LobsterClientFactoryImpl() {
  controller_->SetClientFactory(nullptr);
}

std::unique_ptr<ash::LobsterClient> LobsterClientFactoryImpl::CreateClient() {
  LobsterService* service = LobsterServiceProvider::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  return service != nullptr ? std::make_unique<LobsterClientImpl>(service)
                            : nullptr;
}
