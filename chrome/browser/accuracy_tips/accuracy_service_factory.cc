// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accuracy_tips/accuracy_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"
#include "components/accuracy_tips/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
accuracy_tips::AccuracyService* AccuracyServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<accuracy_tips::AccuracyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}
// static
AccuracyServiceFactory* AccuracyServiceFactory::GetInstance() {
  return base::Singleton<AccuracyServiceFactory>::get();
}

AccuracyServiceFactory::AccuracyServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AccuracyServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

AccuracyServiceFactory::~AccuracyServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* AccuracyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  DCHECK(base::FeatureList::IsEnabled(accuracy_tips::kAccuracyTipsFeature));
  // TODO(crbug.com/1210891): Implement UI.
  return new accuracy_tips::AccuracyService(nullptr);
}
