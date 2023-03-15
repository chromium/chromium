// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"

#include "chrome/browser/enterprise/connectors/reporting/crash_reporting_context.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"

namespace enterprise_connectors {

BrowserCrashEventRouter::BrowserCrashEventRouter(
    content::BrowserContext* context) {
  if (!base::FeatureList::IsEnabled(kBrowserCrashEventsEnabled)) {
    return;
  }
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  CrashReportingContext* crash_reporting_context =
      CrashReportingContext::GetInstance();
  Profile* profile = Profile::FromBrowserContext(context);
  crash_reporting_context->AddProfile(this, profile);

#endif
}

BrowserCrashEventRouter::~BrowserCrashEventRouter() {
  if (!base::FeatureList::IsEnabled(kBrowserCrashEventsEnabled)) {
    return;
  }
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  CrashReportingContext* crash_reporting_context =
      CrashReportingContext::GetInstance();
  crash_reporting_context->RemoveProfile(this);
#endif
}

}  // namespace enterprise_connectors
