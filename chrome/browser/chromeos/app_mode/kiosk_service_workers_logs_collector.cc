// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_service_workers_logs_collector.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const auto* kFailureMessage =
    u"Unable to collect service worker logs as service worker context doesn't "
    u"exist.";

std::u16string GetLogMessageSource(const content::ConsoleMessage& log) {
  return l10n_util::FormatString(
      u"$1 [$2]",
      {base::ASCIIToUTF16(log.source_url.GetContent()),
       base::ASCIIToUTF16(content::MessageSourceToString(log.source))},
      nullptr);
}

content::ServiceWorkerContext* GetServiceWorkerContext(Profile* profile) {
  if (!profile || !profile->GetDefaultStoragePartition()) {
    return nullptr;
  }

  return profile->GetDefaultStoragePartition()->GetServiceWorkerContext();
}

}  // namespace

KioskServiceWorkersLogsCollector::KioskServiceWorkersLogsCollector(
    Profile* profile,
    base::RepeatingCallback<
        void(const KioskAppLevelLogsSaver::KioskLogMessage&)> logger_callback)
    : KioskServiceWorkersLogsCollector(GetServiceWorkerContext(profile),
                                       std::move(logger_callback)) {}

KioskServiceWorkersLogsCollector::KioskServiceWorkersLogsCollector(
    content::ServiceWorkerContext* service_worker_context,
    base::RepeatingCallback<
        void(const KioskAppLevelLogsSaver::KioskLogMessage&)> logger_callback)
    : logger_callback_(std::move(logger_callback)) {
  if (service_worker_context) {
    service_worker_context_observer_.Observe(service_worker_context);
  } else {
    KioskAppLevelLogsSaver::KioskLogMessage log{
        kFailureMessage, blink::mojom::ConsoleMessageLevel::kError};
    logger_callback_.Run(std::move(log));
  }
}

KioskServiceWorkersLogsCollector::~KioskServiceWorkersLogsCollector() = default;

void KioskServiceWorkersLogsCollector::OnReportConsoleMessage(
    int64_t version_id,
    const GURL& scope,
    const content::ConsoleMessage& message) {
  KioskAppLevelLogsSaver::KioskLogMessage log{
      message.message, message.message_level, message.line_number,
      GetLogMessageSource(message), std::nullopt};

  logger_callback_.Run(std::move(log));
}

}  // namespace chromeos
