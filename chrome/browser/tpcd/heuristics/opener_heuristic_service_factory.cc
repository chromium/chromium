// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_service_factory.h"

#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/dips/chrome_dips_delegate.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_service.h"
#include "components/content_settings/core/common/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

/* static */
OpenerHeuristicService* OpenerHeuristicServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OpenerHeuristicService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

OpenerHeuristicServiceFactory* OpenerHeuristicServiceFactory::GetInstance() {
  static base::NoDestructor<OpenerHeuristicServiceFactory> instance;
  return instance.get();
}

OpenerHeuristicServiceFactory::OpenerHeuristicServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OpenerHeuristicService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DIPSServiceFactory::GetInstance());
  DependsOn(TrackingProtectionSettingsFactory::GetInstance());
}

OpenerHeuristicServiceFactory::~OpenerHeuristicServiceFactory() = default;

content::BrowserContext* OpenerHeuristicServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          content_settings::features::kTpcdHeuristicsGrants)) {
    return nullptr;
  }

  // Enable the heuristic for the same profiles as DIPS -- profiles associated
  // with a human user.
  if (!ChromeDipsDelegate::Create()->ShouldEnableDips(context)) {
    return nullptr;
  }

  return context;
}

std::unique_ptr<KeyedService>
OpenerHeuristicServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<OpenerHeuristicService>(
      base::PassKey<OpenerHeuristicServiceFactory>(), context);
}
