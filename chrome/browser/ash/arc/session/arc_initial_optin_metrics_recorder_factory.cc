// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_initial_optin_metrics_recorder_factory.h"

#include "chrome/browser/ash/arc/session/arc_initial_optin_metrics_recorder.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace arc {

// static
ArcInitialOptInMetricsRecorder*
ArcInitialOptInMetricsRecorderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcInitialOptInMetricsRecorder*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcInitialOptInMetricsRecorderFactory*
ArcInitialOptInMetricsRecorderFactory::GetInstance() {
  return base::Singleton<ArcInitialOptInMetricsRecorderFactory>::get();
}

ArcInitialOptInMetricsRecorderFactory::ArcInitialOptInMetricsRecorderFactory()
    : ProfileKeyedServiceFactory(
          "ArcInitialOptInMetricsRecorderFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

ArcInitialOptInMetricsRecorderFactory::
    ~ArcInitialOptInMetricsRecorderFactory() = default;

KeyedService* ArcInitialOptInMetricsRecorderFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new ArcInitialOptInMetricsRecorder(browser_context);
}

}  // namespace arc
