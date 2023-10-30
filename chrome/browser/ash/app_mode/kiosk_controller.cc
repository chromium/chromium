// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_controller.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
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
                                       const std::string& chrome_app_id) {
  KioskAppManager::App manager_app;
  if (!manager.GetApp(chrome_app_id, &manager_app)) {
    return absl::nullopt;
  }
  return KioskApp(
      KioskAppId::ForChromeApp(chrome_app_id, manager_app.account_id),
      manager_app.name, manager_app.icon);
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

std::vector<KioskApp> KioskController::GetApps() const {
  std::vector<KioskApp> apps;
  for (const KioskAppManagerBase::App& web_app : web_app_manager_->GetApps()) {
    apps.emplace_back(KioskAppId::ForWebApp(web_app.account_id), web_app.name,
                      web_app.icon);
  }
  for (const KioskAppManagerBase::App& chrome_app :
       chrome_app_manager_->GetApps()) {
    apps.emplace_back(
        KioskAppId::ForChromeApp(chrome_app.app_id, chrome_app.account_id),
        chrome_app.name, chrome_app.icon);
  }
  for (const KioskAppManagerBase::App& arc_app : arc_app_manager_->GetApps()) {
    apps.emplace_back(KioskAppId::ForArcApp(arc_app.account_id), arc_app.name,
                      arc_app.icon);
  }
  return apps;
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
