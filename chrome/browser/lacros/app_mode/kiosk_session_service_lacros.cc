// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"

#include "base/bind.h"
#include "base/check.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/lacros/lacros_service.h"

namespace {

static KioskSessionServiceLacros* g_kiosk_session_service = nullptr;

}  // namespace

// static
KioskSessionServiceLacros* KioskSessionServiceLacros::Get() {
  CHECK(g_kiosk_session_service);
  return g_kiosk_session_service;
}

KioskSessionServiceLacros::KioskSessionServiceLacros() {
  g_kiosk_session_service = this;
}

KioskSessionServiceLacros::~KioskSessionServiceLacros() {
  g_kiosk_session_service = nullptr;
}

void KioskSessionServiceLacros::InitWebKioskSession(Browser* browser) {
  LOG_IF(FATAL, app_session_) << "Web Kiosk session is already initialized.";
  app_session_ = std::make_unique<chromeos::AppSession>(base::BindOnce(
      &KioskSessionServiceLacros::AttemptUserExit, weak_factory_.GetWeakPtr()));
  app_session_->InitForWebKiosk(browser);
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
