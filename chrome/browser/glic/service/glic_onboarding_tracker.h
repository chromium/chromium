// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_TRACKER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_TRACKER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/service/glic_onboarding_status.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class PrefService;
class Profile;

namespace glic {

class GlicEnabling;

// Tracks profile onboarding milestones (Invoke, OptIn, and Prompt) and persists
// them in local profile preferences.
// TODO(crbug.com/545714879): Refactor GlicOnboardingTracker from a stateful
// class owned by GlicInstanceCoordinator to free-standing profile helper
// functions, and remove onboarding delegation from InstanceCoordinatorDelegate.
class GlicOnboardingTracker {
 public:
  GlicOnboardingTracker(Profile* profile, GlicEnabling* enabling);
  GlicOnboardingTracker(const GlicOnboardingTracker&) = delete;
  GlicOnboardingTracker& operator=(const GlicOnboardingTracker&) = delete;
  ~GlicOnboardingTracker();

  void OnInvoke(mojom::InvocationSource source, ukm::SourceId source_id);
  void OnFreOptInShown(ukm::SourceId source_id);
  void OnPrompt(ukm::SourceId source_id);

  OnboardingStatus GetStatus() const;

 private:
  void MigrateInitialOnboardingStatus(Profile* profile);
  void OnConsentChanged();
  void RecordFunnelStep(OnboardingFunnelStep step,
                        mojom::InvocationSource source,
                        ukm::SourceId source_id);

  GlicOnboardingStatus onboarding_status_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<GlicEnabling> enabling_;
  base::CallbackListSubscription consent_subscription_;

  mojom::InvocationSource last_invocation_source_ =
      mojom::InvocationSource::kUnsupported;
  ukm::SourceId last_source_id_ = ukm::kInvalidSourceId;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_ONBOARDING_TRACKER_H_
