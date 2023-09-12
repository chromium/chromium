// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service.h"

#include "base/feature_list.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"

namespace tpcd::experiment {

EligibilityService::EligibilityService(Profile* profile)
    : profile_(profile), pref_service_(profile->GetPrefs()) {
  DCHECK(base::FeatureList::IsEnabled(k3PCDModeBExperiment));
  DCHECK(pref_service_);
}

EligibilityService::~EligibilityService() = default;

// static
EligibilityService* EligibilityService::Get(Profile* profile) {
  return EligibilityServiceFactory::GetForProfile(profile);
}

}  // namespace tpcd::experiment
