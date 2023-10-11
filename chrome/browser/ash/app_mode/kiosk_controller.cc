// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_controller.h"

#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

absl::optional<KioskApp> WebAppById(WebKioskAppManager& manager,
                                    const AccountId& account_id) {
  const WebKioskAppData* data = manager.GetAppByAccountId(account_id);
  if (!data) {
    return absl::nullopt;
  }
  return KioskApp(KioskAppId::ForWebApp(account_id), data->name(),
                  data->icon());
}

absl::optional<KioskApp> ChromeAppById(KioskAppManager& manager,
                                       const std::string& app_id) {
  KioskAppManager::App manager_app;
  if (!manager.GetApp(app_id, &manager_app)) {
    return absl::nullopt;
  }
  return KioskApp(KioskAppId::ForChromeApp(app_id), manager_app.name,
                  manager_app.icon);
}

absl::optional<KioskApp> ArcAppById(ArcKioskAppManager& manager,
                                    const AccountId& account_id) {
  const ArcKioskAppData* data = manager.GetAppByAccountId(account_id);
  if (!data) {
    return absl::nullopt;
  }
  return KioskApp(KioskAppId::ForArcApp(account_id), data->name(),
                  data->icon());
}

static KioskController* g_instance = nullptr;

}  // namespace

KioskController& KioskController::Get() {
  return CHECK_DEREF(g_instance);
}

KioskController::KioskController(WebKioskAppManager& web_app_manager,
                                 KioskAppManager& chrome_app_manager,
                                 ArcKioskAppManager& arc_app_manager)
    : web_app_manager_(web_app_manager),
      chrome_app_manager_(chrome_app_manager),
      arc_app_manager_(arc_app_manager) {
  CHECK(!g_instance);
  g_instance = this;
}

KioskController::~KioskController() {
  g_instance = nullptr;
}

absl::optional<KioskApp> KioskController::GetAutoLaunchApp() const {
  if (const auto& web_account_id = web_app_manager_->GetAutoLaunchAccountId();
      web_account_id.is_valid()) {
    return WebAppById(web_app_manager_.get(), web_account_id);
  } else if (chrome_app_manager_->IsAutoLaunchEnabled()) {
    std::string chrome_app_id = chrome_app_manager_->GetAutoLaunchApp();
    CHECK(!chrome_app_id.empty());
    return ChromeAppById(chrome_app_manager_.get(), chrome_app_id);
  } else if (const auto& arc_account_id =
                 arc_app_manager_->GetAutoLaunchAccountId();
             arc_account_id.is_valid()) {
    return ArcAppById(arc_app_manager_.get(), arc_account_id);
  }
  return absl::nullopt;
}

}  // namespace ash
