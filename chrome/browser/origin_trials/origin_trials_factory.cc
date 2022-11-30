// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/browser/prefservice_persistence_provider.h"
#include "components/origin_trials/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace {

base::LazyInstance<OriginTrialsFactory>::DestructorAtExit
    g_origin_trials_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
content::OriginTrialsControllerDelegate*
OriginTrialsFactory::GetForBrowserContext(content::BrowserContext* context) {
  if (origin_trials::features::IsPersistentOriginTrialsEnabled()) {
    return static_cast<origin_trials::OriginTrials*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  } else {
    return nullptr;
  }
}

// static
OriginTrialsFactory* OriginTrialsFactory::GetInstance() {
  return g_origin_trials_factory.Pointer();
}

OriginTrialsFactory::OriginTrialsFactory()
    : BrowserContextKeyedServiceFactory(
          "OriginTrials",
          BrowserContextDependencyManager::GetInstance()) {}

OriginTrialsFactory::~OriginTrialsFactory() noexcept = default;

KeyedService* OriginTrialsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return new origin_trials::OriginTrials(
      std::make_unique<origin_trials::PrefServicePersistenceProvider>(context),
      std::make_unique<blink::TrialTokenValidator>());
}
