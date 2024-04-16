// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/origin_trials/browser/leveldb_persistence_provider.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
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
    : ProfileKeyedServiceFactory(
          "OriginTrials",
          ProfileSelections::Builder()
              // Do not use for system and internal profiles
              // TODO(crbug.com/40247867): May need to enable Guest in the
              // future.
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

OriginTrialsFactory::~OriginTrialsFactory() noexcept = default;

std::unique_ptr<KeyedService>
OriginTrialsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return std::make_unique<origin_trials::OriginTrials>(
      std::make_unique<origin_trials::LevelDbPersistenceProvider>(
          context->GetPath(),
          context->GetDefaultStoragePartition()->GetProtoDatabaseProvider()),
      std::make_unique<blink::TrialTokenValidator>());
}
