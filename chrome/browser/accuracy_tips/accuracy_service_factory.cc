// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accuracy_tips/accuracy_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/page_info/chrome_accuracy_tip_ui.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

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
  DCHECK(base::FeatureList::IsEnabled(safe_browsing::kAccuracyTipsFeature));
  auto ui = std::make_unique<ChromeAccuracyTipUI>();
  auto sb_database =
      g_browser_process->safe_browsing_service()
          ? g_browser_process->safe_browsing_service()->database_manager()
          : nullptr;
  return new accuracy_tips::AccuracyService(
      std::move(ui), std::move(sb_database), content::GetUIThreadTaskRunner({}),
      content::GetIOThreadTaskRunner({}));
}
