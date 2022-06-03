// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_proxy_service_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/chromeos/printing/cups_proxy_service_delegate_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/cups_proxy/cups_proxy_client.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

CupsProxyServiceManager::CupsProxyServiceManager() {
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
  cups_proxy::CupsProxyService::Spawn(
      std::make_unique<CupsProxyServiceDelegateImpl>());
}

}  // namespace chromeos
