// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_ai_model_cache_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/storage_partition.h"

namespace autofill {

// static
AutofillAiModelCache* AutofillAiModelCacheFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutofillAiModelCache*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutofillAiModelCacheFactory* AutofillAiModelCacheFactory::GetInstance() {
  static base::NoDestructor<AutofillAiModelCacheFactory> instance;
  return instance.get();
}

AutofillAiModelCacheFactory::AutofillAiModelCacheFactory()
    : ProfileKeyedServiceFactory(
          "AutofillAiModelCache",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

AutofillAiModelCacheFactory::~AutofillAiModelCacheFactory() = default;

std::unique_ptr<KeyedService>
AutofillAiModelCacheFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAiServerModel)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AutofillAiModelCacheImpl>(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider(),
      profile->GetPath(),
      autofill::features::kAutofillAiServerModelCacheSize.Get(),
      autofill::features::kAutofillAiServerModelCacheAge.Get());
}

bool AutofillAiModelCacheFactory::ServiceIsCreatedWithBrowserContext() const {
  return base::FeatureList::IsEnabled(features::kAutofillAiServerModel);
}

}  // namespace autofill
