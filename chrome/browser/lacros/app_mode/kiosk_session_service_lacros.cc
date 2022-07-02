// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"

#include "base/bind.h"
#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/prefs/pref_registry_simple.h"

namespace {

static KioskSessionServiceLacros* g_kiosk_session_service = nullptr;

bool IsKioskSession(crosapi::mojom::SessionType session_type) {
  return (session_type == crosapi::mojom::SessionType::kWebKioskSession) ||
         (session_type == crosapi::mojom::SessionType::kAppKioskSession);
}

}  // namespace

// static
KioskSessionServiceLacros* KioskSessionServiceLacros::Get() {
  CHECK(g_kiosk_session_service);
  return g_kiosk_session_service;
}

// static
void KioskSessionServiceLacros::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  chromeos::AppSession::RegisterPrefs(registry);
}

KioskSessionServiceLacros::KioskSessionServiceLacros() {
  g_kiosk_session_service = this;
}

KioskSessionServiceLacros::~KioskSessionServiceLacros() {
  g_kiosk_session_service = nullptr;
}

void KioskSessionServiceLacros::InitChromeKioskSession(
    Profile* profile,
    const std::string& app_id) {
  LOG_IF(FATAL, app_session_) << "Kiosk session is already initialized.";
  app_session_ = std::make_unique<chromeos::AppSession>(
      base::BindOnce(&KioskSessionServiceLacros::AttemptUserExit,
                     weak_factory_.GetWeakPtr()),
      g_browser_process->local_state());
  app_session_->Init(profile, app_id);
}

void KioskSessionServiceLacros::InitWebKioskSession(Browser* browser,
                                                    const GURL& install_url) {
  LOG_IF(FATAL, app_session_) << "Kiosk session is already initialized.";
  app_session_ = std::make_unique<chromeos::AppSession>(
      base::BindOnce(&KioskSessionServiceLacros::AttemptUserExit,
                     weak_factory_.GetWeakPtr()),
      g_browser_process->local_state());
  app_session_->InitForWebKiosk(browser);
  install_url_ = install_url;
}

void KioskSessionServiceLacros::AttemptUserExit() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  CHECK(service);

  if (!service->IsAvailable<crosapi::mojom::KioskSessionService>()) {
    LOG(ERROR) << "Kiosk session service is not available.";
    return;
  }

  service->GetRemote<crosapi::mojom::KioskSessionService>()->AttemptUserExit();
}

bool KioskSessionServiceLacros::RestartDevice(const std::string& description) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  CHECK(service);

  if (IsKioskSession(chromeos::BrowserInitParams::Get()->session_type) &&
      service->IsAvailable<crosapi::mojom::KioskSessionService>()) {
    int remote_version = service->GetInterfaceVersion(
        crosapi::mojom::KioskSessionService::Uuid_);
    if (remote_version >= 0 &&
        static_cast<uint32_t>(remote_version) >=
            crosapi::mojom::KioskSessionService::kRestartDeviceMinVersion) {
      auto callback = base::BindOnce([](bool status) {
        if (!status) {
          LOG(ERROR) << "Restart device was called but failed";
        }
      });

      auto& remote = service->GetRemote<crosapi::mojom::KioskSessionService>();
      remote->RestartDevice(description, std::move(callback));
      return true;
    } else {
      LOG(ERROR) << "Current KioskSessionService " << remote_version
                 << " does not support RestartDevice";
    }
  } else {
    LOG(ERROR) << "Kiosk session service is not available.";
  }
  return false;
}
