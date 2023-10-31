// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_proxy_service_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/ash/printing/cups_proxy_service_delegate_impl.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/dbus/cups_proxy/cups_proxy_client.h"
#include "content/public/browser/browser_context.h"

namespace ash {

CupsProxyServiceManager::CupsProxyServiceManager(Profile* profile)
    : profile_(profile) {
  // Don't wait for the daemon if the feature is turned off anyway.
  if (base::FeatureList::IsEnabled(features::kPluginVm)) {
    CupsProxyClient::Get()->WaitForServiceToBeAvailable(
        base::BindOnce(&CupsProxyServiceManager::OnDaemonAvailable,
                       weak_factory_.GetWeakPtr()));
  }
}

CupsProxyServiceManager::~CupsProxyServiceManager() = default;

void CupsProxyServiceManager::OnDaemonAvailable(bool daemon_available) {
  if (!daemon_available) {
    DVLOG(1) << "CupsProxyDaemon startup error";
    return;
  }

  // Attempt to start the service, which will then bootstrap a connection
  // with the daemon.
  service_was_started_ = true;
  cups_proxy::CupsProxyService::Spawn(
      std::make_unique<CupsProxyServiceDelegateImpl>(profile_));
}

void CupsProxyServiceManager::Shutdown() {
  if (service_was_started_) {
    cups_proxy::CupsProxyService::Shutdown();
  }
  profile_ = nullptr;
}

}  // namespace ash
