// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace privacy_sandbox {
class TrackingProtectionOnboarding;
}
class PrefService;

namespace tpcd::experiment {

class ExperimentManager;

class EligibilityService : public KeyedService {
 public:
  explicit EligibilityService(Profile* profile,
                              ExperimentManager* experiment_manager);
  EligibilityService(const EligibilityService&) = delete;
  EligibilityService& operator=(const EligibilityService&) = delete;
  ~EligibilityService() override;

  static EligibilityService* Get(Profile* profile);

  void Shutdown() override;

 private:
  // So EligibilityServiceFactory::BuildServiceInstanceFor can call the
  // constructor.
  friend class EligibilityServiceFactory;
  friend class EligibilityServiceBrowserTest;

  // OnClientEligibilityChanged should only be called for currently loaded
  // profiles when the ExperimentManager computes a client eligibility.
  void OnClientEligibilityChanged(bool is_eligible);
  // MarkProfileEligibility should be called for all profiles to set their
  // eligibility, whether currently loaded or created later.
  void MarkProfileEligibility(bool is_eligible);

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
  // onboarding_service_ may be null for OTR and system profiles.
  raw_ptr<privacy_sandbox::TrackingProtectionOnboarding> onboarding_service_;
  raw_ptr<ExperimentManager> experiment_manager_;
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_
