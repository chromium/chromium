// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "url/origin.h"

namespace {

static KioskSessionServiceLacros* g_kiosk_session_service = nullptr;

}  // namespace

// static
KioskSessionServiceLacros* KioskSessionServiceLacros::Get() {
  CHECK(g_kiosk_session_service);
  return g_kiosk_session_service;
}

// static
void KioskSessionServiceLacros::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  chromeos::AppSession::RegisterLocalStatePrefs(registry);
}

// static
void KioskSessionServiceLacros::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  chromeos::AppSession::RegisterProfilePrefs(registry);
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
      profile,
      base::BindOnce(&KioskSessionServiceLacros::AttemptUserExit,
                     weak_factory_.GetWeakPtr()),
      g_browser_process->local_state());
  app_session_->Init(app_id);
}

void KioskSessionServiceLacros::InitWebKioskSession(Browser* browser,
                                                    const GURL& install_url) {
  LOG_IF(FATAL, app_session_) << "Kiosk session is already initialized.";
  app_session_ = std::make_unique<chromeos::AppSession>(
      browser->profile(),
      base::BindOnce(&KioskSessionServiceLacros::AttemptUserExit,
                     weak_factory_.GetWeakPtr()),
      g_browser_process->local_state());
  app_session_->InitForWebKiosk(browser->app_name());
  browser->profile()
      ->GetExtensionSpecialStoragePolicy()
      ->AddOriginWithUnlimitedStorage(url::Origin::Create(install_url));
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
