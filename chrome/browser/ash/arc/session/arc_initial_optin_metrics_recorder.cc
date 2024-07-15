// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_initial_optin_metrics_recorder.h"

#include <string>

#include "ash/components/arc/arc_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {

class ArcInitialOptInMetricsRecorderFactory
    : public ProfileKeyedServiceFactory {
 public:
  ArcInitialOptInMetricsRecorderFactory();

  ArcInitialOptInMetricsRecorderFactory(
      const ArcInitialOptInMetricsRecorderFactory&) = delete;
  ArcInitialOptInMetricsRecorderFactory& operator=(
      const ArcInitialOptInMetricsRecorderFactory&) = delete;

  ~ArcInitialOptInMetricsRecorderFactory() override = default;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override {
    return new ArcInitialOptInMetricsRecorder(browser_context);
  }

  // static
  static ArcInitialOptInMetricsRecorder* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<ArcInitialOptInMetricsRecorder*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  // static
  static ArcInitialOptInMetricsRecorderFactory* GetInstance() {
    return base::Singleton<ArcInitialOptInMetricsRecorderFactory>::get();
  }
};

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

}  // anonymous namespace

// static
ArcInitialOptInMetricsRecorder* ArcInitialOptInMetricsRecorder::GetForProfile(
    Profile* profile) {
  return ArcInitialOptInMetricsRecorderFactory::GetForBrowserContext(profile);
}

ArcInitialOptInMetricsRecorder::ArcInitialOptInMetricsRecorder(
    content::BrowserContext* context) {
  ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager might not be set in tests.
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);
}

ArcInitialOptInMetricsRecorder::~ArcInitialOptInMetricsRecorder() {
  ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager may be released first.
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

void ArcInitialOptInMetricsRecorder::OnArcOptInUserAction() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_opt_in_time_ = base::TimeTicks::Now();
}

void ArcInitialOptInMetricsRecorder::OnArcInitialStart() {
  if (!IsArcPlayAutoInstallDisabled()) {
    return;
  }

  LOG(WARNING) << "kArcDisablePlayAutoInstall flag is set. Force Arc apps "
                  "loaded metric.";
  OnArcAppListReady();
}

void ArcInitialOptInMetricsRecorder::OnArcAppListReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (arc_app_list_ready_reported_) {
    return;
  }

  // |Ash.ArcAppInitialAppsInstallDuration| histogram is only reported for
  // the first user session after they opted into the ARC++.
  // |arc_opt_in_time_| will only have value if user opted in into the ARC++
  // in this session (in this browser instance).
  if (arc_opt_in_time_.has_value()) {
    const auto duration = base::TimeTicks::Now() - arc_opt_in_time_.value();
    UmaHistogramCustomTimes("Ash.ArcAppInitialAppsInstallDuration", duration,
                            base::Seconds(1) /* min */,
                            base::Hours(1) /* max */, 100 /* buckets */);
  }

  arc_app_list_ready_reported_ = true;
}

bool ArcInitialOptInMetricsRecorder::NeedReportArcAppListReady() const {
  return arc_opt_in_time_.has_value() && !arc_app_list_ready_reported_;
}

// static
void ArcInitialOptInMetricsRecorder::EnsureFactoryBuilt() {
  ArcInitialOptInMetricsRecorderFactory::GetInstance();
}

}  // namespace arc
