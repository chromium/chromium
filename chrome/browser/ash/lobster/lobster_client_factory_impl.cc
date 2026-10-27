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
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"

LobsterClientFactoryImpl::LobsterClientFactoryImpl(
    ash::LobsterController* controller)
    : controller_(controller) {
  controller_->SetClientFactory(this);
}

LobsterClientFactoryImpl::~LobsterClientFactoryImpl() {
  controller_->SetClientFactory(nullptr);
}

std::unique_ptr<ash::LobsterClient> LobsterClientFactoryImpl::CreateClient() {
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const auto* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  if (!active_session) {
    return nullptr;
  }
  LobsterService* service =
      LobsterServiceProvider::GetForProfile(Profile::FromBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
              active_session->account_id())));
  return service != nullptr ? std::make_unique<LobsterClientImpl>(service)
                            : nullptr;
}
