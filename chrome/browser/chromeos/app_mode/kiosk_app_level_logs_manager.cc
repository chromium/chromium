// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/chromeos/app_mode/kiosk_service_workers_logs_collector.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

KioskAppLevelLogsManager::KioskAppLevelLogsManager(
    Profile* profile,
    const ash::KioskAppId& app_id)
    : service_workers_logs_collector_(
          profile,
          base::BindRepeating(&KioskAppLevelLogsManager::SaveLog,
                              base::Unretained(this))),
      browser_logs_collector_(
          base::BindRepeating(&KioskAppLevelLogsManager::SaveLog,
                              base::Unretained(this))),
      app_windows_logs_collector_(
          profile,
          base::BindRepeating(&KioskAppLevelLogsManager::SaveLog,
                              base::Unretained(this))) {
  SYSLOG(INFO) << "Starting log collection for kiosk app: " << app_id;
}

KioskAppLevelLogsManager::~KioskAppLevelLogsManager() = default;

void KioskAppLevelLogsManager::SaveLog(
    const KioskAppLevelLogsSaver::KioskLogMessage& log) {
  logs_saver_.SaveLog(log);
}

}  // namespace chromeos
