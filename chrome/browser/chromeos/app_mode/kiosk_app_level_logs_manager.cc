// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager.h"

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

namespace chromeos {

KioskAppLevelLogsManager::KioskAppLevelLogsManager(Profile* profile) {}

KioskAppLevelLogsManager::~KioskAppLevelLogsManager() = default;

void KioskAppLevelLogsManager::SaveLog(
    const KioskAppLevelLogsSaver::KioskLogMessage& log) {
  logs_saver_.SaveLog(log);
}

}  // namespace chromeos
