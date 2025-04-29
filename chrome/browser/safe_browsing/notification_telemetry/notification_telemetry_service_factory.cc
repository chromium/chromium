// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service_factory.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
NotificationTelemetryService*
NotificationTelemetryServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<NotificationTelemetryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
NotificationTelemetryServiceFactory*
NotificationTelemetryServiceFactory::GetInstance() {
  static base::NoDestructor<NotificationTelemetryServiceFactory> instance;
  return instance.get();
}

NotificationTelemetryServiceFactory::NotificationTelemetryServiceFactory()
    : ProfileKeyedServiceFactory(
          "NotificationTelemetryService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

std::unique_ptr<KeyedService>
NotificationTelemetryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!g_browser_process || !g_browser_process->safe_browsing_service() ||
      !g_browser_process->safe_browsing_service()->database_manager()) {
    return nullptr;
  }

  return std::make_unique<NotificationTelemetryService>(
      Profile::FromBrowserContext(context),
      g_browser_process->shared_url_loader_factory(),
      g_browser_process->safe_browsing_service()->database_manager());
}

}  // namespace safe_browsing
