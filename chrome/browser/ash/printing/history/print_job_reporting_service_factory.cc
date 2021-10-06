// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_reporting_service_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback.h"
#include "chrome/browser/ash/printing/history/print_job_reporting_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {

// static
PrintJobReportingService* PrintJobReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrintJobReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrintJobReportingServiceFactory*
PrintJobReportingServiceFactory::GetInstance() {
  return base::Singleton<PrintJobReportingServiceFactory>::get();
}

PrintJobReportingServiceFactory::PrintJobReportingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PrintJobReportingServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

PrintJobReportingServiceFactory::~PrintJobReportingServiceFactory() = default;

KeyedService* PrintJobReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  policy::DMToken dm_token = policy::GetDMToken(profile);
  // TODO(1229994, marcgrimme) remove the logs as part of refactoring.
  if (!dm_token.is_valid()) {
    LOG(ERROR) << "DMToken must be valid";
    return nullptr;
  }

  auto reporting_service = PrintJobReportingService::Create(dm_token.value());

  return reporting_service.release();
}

bool PrintJobReportingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
