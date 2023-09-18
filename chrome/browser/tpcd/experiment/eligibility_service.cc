// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "content/public/browser/cookie_deprecation_label_manager.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

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

void EligibilityService::OnClientEligibilityChanged() {
  profile_->ForEachLoadedStoragePartition(
      base::BindRepeating([](content::StoragePartition* storage_partition) {
        auto* cookie_deprecation_label_manager =
            storage_partition->GetCookieDeprecationLabelManager();
        if (cookie_deprecation_label_manager) {
          storage_partition->GetNetworkContext()->SetCookieDeprecationLabel(
              cookie_deprecation_label_manager->GetValue());
        }
      }));
}

}  // namespace tpcd::experiment
