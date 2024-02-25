// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace reporting::metrics {

// static
MetricReportingManagerLacrosFactory*
MetricReportingManagerLacrosFactory::GetInstance() {
  static base::NoDestructor<MetricReportingManagerLacrosFactory> g_factory;
  return g_factory.get();
}

MetricReportingManagerLacros*
MetricReportingManagerLacrosFactory::GetForProfile(Profile* profile) {
  CHECK(profile);
  if (!profile->IsMainProfile()) {
    // We only report metrics and events for main profile today.
    return nullptr;
  }
  return static_cast<MetricReportingManagerLacros*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

MetricReportingManagerLacrosFactory::MetricReportingManagerLacrosFactory()
    : ProfileKeyedServiceFactory(
          "MetricReportingManagerLacros",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

MetricReportingManagerLacrosFactory::~MetricReportingManagerLacrosFactory() =
    default;

std::unique_ptr<KeyedService>
MetricReportingManagerLacrosFactory::BuildServiceInstanceForBrowserContext(
    ::content::BrowserContext* context) const {
  auto* const profile = Profile::FromBrowserContext(context);
  return std::make_unique<MetricReportingManagerLacros>(
      profile, std::make_unique<MetricReportingManagerLacros::Delegate>());
}

}  // namespace reporting::metrics
