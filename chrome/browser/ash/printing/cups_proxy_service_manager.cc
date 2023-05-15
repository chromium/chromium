// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_proxy_service_manager.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/printing/cups_proxy_service_delegate_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/cups_proxy/cups_proxy_service.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/cups_proxy/cups_proxy_client.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {

// Returns true iff the primary profile has been created.
bool IsPrimaryProfileCreated() {
  if (!user_manager::UserManager::IsInitialized()) {
    return false;
  }

  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  return primary_user && primary_user->is_profile_created();
}

}  // namespace

CupsProxyServiceManager::CupsProxyServiceManager()
    : profile_manager_(g_browser_process->profile_manager()) {
  // Don't wait for the daemon or subscribe to ProfileManager if the feature is
  // turned off anyway.
  if (!base::FeatureList::IsEnabled(features::kPluginVm)) {
    return;
  }

  // The primary profile might have been created already. If so, there's no
  // need to subscribe to ProfileManager.
  primary_profile_available_ = IsPrimaryProfileCreated();

  if (!primary_profile_available_ && profile_manager_) {
    profile_manager_->AddObserver(this);
  }

  CupsProxyClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &CupsProxyServiceManager::OnDaemonAvailable, weak_factory_.GetWeakPtr()));
}

CupsProxyServiceManager::~CupsProxyServiceManager() {
  if (profile_manager_) {
    profile_manager_->RemoveObserver(this);
  }

  if (cups_proxy::CupsProxyService::GetInstance() != nullptr) {
    cups_proxy::CupsProxyService::Shutdown();
  }
}

void CupsProxyServiceManager::OnDaemonAvailable(bool daemon_available) {
  if (!daemon_available) {
    DVLOG(1) << "CupsProxyDaemon startup error";
    return;
  }

  daemon_available_ = true;

  MaybeSpawnCupsProxyService();
}

void CupsProxyServiceManager::OnProfileAdded(Profile* profile) {
  if (!profile) {
    return;
  }

  BrowserContextHelper* browser_context_helper = BrowserContextHelper::Get();
  if (!browser_context_helper) {
    return;
  }

  const user_manager::User* user =
      browser_context_helper->GetUserByBrowserContext(profile);
  if (!user) {
    return;
  }

  DCHECK(user_manager::UserManager::IsInitialized());
  if (!user_manager::UserManager::Get()->IsPrimaryUser(user)) {
    return;
  }

  // Now that we've seen the primary profile, there's no need to keep our
  // subscription to ProfileManager.
  profile_manager_->RemoveObserver(this);
  profile_manager_ = nullptr;

  primary_profile_available_ = true;

  MaybeSpawnCupsProxyService();
}

void CupsProxyServiceManager::OnProfileManagerDestroying() {
  if (profile_manager_) {
    profile_manager_->RemoveObserver(this);
    profile_manager_ = nullptr;
  }
}

void CupsProxyServiceManager::MaybeSpawnCupsProxyService() {
  if (!primary_profile_available_ || !daemon_available_) {
    return;
  }

  // Attempt to start the service, which will then bootstrap a connection
  // with the daemon.
  cups_proxy::CupsProxyService::Spawn(
      std::make_unique<CupsProxyServiceDelegateImpl>());
}

}  // namespace ash
