// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SERVICE_WORKERS_LOGS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SERVICE_WORKERS_LOGS_COLLECTOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "url/gurl.h"

namespace chromeos {

class KioskServiceWorkersLogsCollector
    : public content::ServiceWorkerContextObserver {
 public:
  KioskServiceWorkersLogsCollector(
      Profile* profile,
      base::RepeatingCallback<void(
          const KioskAppLevelLogsSaver::KioskLogMessage&)> logger_callback);
  KioskServiceWorkersLogsCollector(
      content::ServiceWorkerContext* service_worker_context,
      base::RepeatingCallback<void(
          const KioskAppLevelLogsSaver::KioskLogMessage&)> logger_callback);

  KioskServiceWorkersLogsCollector(const KioskServiceWorkersLogsCollector&) =
      delete;
  KioskServiceWorkersLogsCollector& operator=(
      const KioskServiceWorkersLogsCollector&) = delete;

  ~KioskServiceWorkersLogsCollector() override;

  // content::ServiceWorkerContextObserver:
  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const content::ConsoleMessage& message) override;

 private:
  base::RepeatingCallback<void(const KioskAppLevelLogsSaver::KioskLogMessage&)>
      logger_callback_;

  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      service_worker_context_observer_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_SERVICE_WORKERS_LOGS_COLLECTOR_H_
