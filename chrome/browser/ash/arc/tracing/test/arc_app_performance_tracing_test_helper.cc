// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/test/arc_app_performance_tracing_test_helper.h"

#include "ash/constants/ash_features.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/views/widget/widget.h"

namespace arc {

ArcAppPerformanceTracingTestHelper::ArcAppPerformanceTracingTestHelper() =
    default;
ArcAppPerformanceTracingTestHelper::~ArcAppPerformanceTracingTestHelper() =
    default;

void ArcAppPerformanceTracingTestHelper::SetUp(Profile* profile) {
  DCHECK(!profile_ && profile);
  DCHECK(IsArcAllowedForProfile(profile));
  profile_ = profile;
  wm_helper_ = std::make_unique<exo::WMHelper>();
  *ArcAppPerformanceTracing::ticks_now_callback() = base::BindRepeating(
      &ArcAppPerformanceTracingTestHelper::ticks_now, base::Unretained(this));
  // Make sure it is accessible in test.
  if (!GetTracing()) {
    ArcAppPerformanceTracing::GetForBrowserContextForTesting(profile_);
    DCHECK(GetTracing());
  }
}

void ArcAppPerformanceTracingTestHelper::TearDown() {
  DCHECK(profile_);
  exo::WMHelper::GetInstance()->RemoveActivationObserver(GetTracing());
  ArcAppPerformanceTracing::reset_ticks_now_callback();
  wm_helper_.reset();
  profile_ = nullptr;
}

ArcAppPerformanceTracing* ArcAppPerformanceTracingTestHelper::GetTracing() {
  DCHECK(profile_);
  return ArcAppPerformanceTracing::GetForBrowserContext(profile_);
}

ArcAppPerformanceTracingSession*
ArcAppPerformanceTracingTestHelper::GetTracingSession() {
  return GetTracing()->session();
}

void ArcAppPerformanceTracingTestHelper::FireTimerForTesting() {
  DCHECK(GetTracingSession());
  DCHECK(GetTracingSession()->tracing_active());
  DCHECK(GetTracingSession()->HasPresentFrames());
  GetTracingSession()->FireTimerForTesting();
}

void ArcAppPerformanceTracingTestHelper::PlaySequence(
    exo::Surface* surface,
    const std::vector<base::TimeDelta>& deltas) {
  DCHECK(GetTracingSession());
  DCHECK(GetTracingSession()->tracing_active());
  DCHECK(GetTracingSession()->HasPresentFrames());
  Commit(surface, PresentType::kSuccessful);
  for (const base::TimeDelta& delta : deltas) {
    ticks_now_ += delta;
    Commit(surface, PresentType::kSuccessful);
  }
}

void ArcAppPerformanceTracingTestHelper::PlayDefaultSequence(
    exo::Surface* surface) {
  const base::TimeDelta normal_interval = base::Seconds(1) / 60;
  const base::TimeDelta error1 = base::Microseconds(100);
  const base::TimeDelta error2 = base::Microseconds(200);
  const base::TimeDelta error3 = base::Microseconds(300);
  const std::vector<base::TimeDelta> sequence = {
      normal_interval + error1,
      normal_interval + error2,
      // One frame skip
      normal_interval * 2 + error3,
      normal_interval - error1,
      normal_interval - error2,
      // Two frames skip
      normal_interval * 3 - error3,
      normal_interval + error1,
      normal_interval + error2,
      normal_interval * 2 + error3,
      normal_interval - error1,
      normal_interval * 2 - error2,
      normal_interval - error3,
      normal_interval + error1,
      normal_interval + error2,
      normal_interval + error3,
  };
  PlaySequence(surface, sequence);
}

void ArcAppPerformanceTracingTestHelper::Commit(exo::Surface* surface,
                                                PresentType present) {
  surface->Commit();

  std::list<exo::Surface::FrameCallback> frame_callbacks;
  std::list<exo::Surface::PresentationCallback> presentation_callbacks;
  surface->AppendSurfaceHierarchyCallbacks(&frame_callbacks,
                                           &presentation_callbacks);

  // Null timestamp indicates a discarded present.
  gfx::PresentationFeedback feedback(
      present == PresentType::kSuccessful ? ticks_now_ : base::TimeTicks(),
      base::TimeDelta() /* interval */, 0 /* flags */);
  for (auto& cb : presentation_callbacks) {
    cb.Run(feedback);
  }
}

void ArcAppPerformanceTracingTestHelper::DisableAppSync() {
  DCHECK(profile_);
  syncer::SyncUserSettings* sync_user_settings =
      SyncServiceFactory::GetForProfile(profile_)->GetUserSettings();
  syncer::UserSelectableOsTypeSet selected_sync_types =
      sync_user_settings->GetSelectedOsTypes();
  selected_sync_types.Remove(syncer::UserSelectableOsType::kOsApps);
  sync_user_settings->SetSelectedOsTypes(
      /*sync_all_os_types=*/false, selected_sync_types);
}

void ArcAppPerformanceTracingTestHelper::AdvanceTickCount(
    base::TimeDelta delta) {
  ticks_now_ += delta;
}

}  // namespace arc
