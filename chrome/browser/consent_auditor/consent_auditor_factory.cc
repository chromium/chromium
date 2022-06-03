// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/consent_auditor/consent_auditor_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/consent_auditor/consent_auditor_impl.h"
#include "components/consent_auditor/consent_sync_bridge.h"
#include "components/consent_auditor/consent_sync_bridge_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/version_info/version_info.h"

// static
ConsentAuditorFactory* ConsentAuditorFactory::GetInstance() {
  return base::Singleton<ConsentAuditorFactory>::get();
}

// static
consent_auditor::ConsentAuditor* ConsentAuditorFactory::GetForProfile(
    Profile* profile) {
  // Recording local consents in Incognito is not useful, as the record would
  // soon disappear. Consents tied to the user's Google account should retrieve
  // account information from the original profile. In both cases, there is no
  // reason to support Incognito.
  DCHECK(!profile->IsOffTheRecord());
  return static_cast<consent_auditor::ConsentAuditor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ConsentAuditorFactory::ConsentAuditorFactory()
    : BrowserContextKeyedServiceFactory(
          "ConsentAuditor",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

ConsentAuditorFactory::~ConsentAuditorFactory() {}

KeyedService* ConsentAuditorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  std::unique_ptr<consent_auditor::ConsentSyncBridge> consent_sync_bridge;
  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::USER_CONSENTS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  consent_sync_bridge =
      std::make_unique<consent_auditor::ConsentSyncBridgeImpl>(
          std::move(store_factory), std::move(change_processor));

  return new consent_auditor::ConsentAuditorImpl(
      profile->GetPrefs(), std::move(consent_sync_bridge),
      // The browser version and locale do not change runtime, so we can pass
      // them directly.
      version_info::GetVersionNumber(),
      g_browser_process->GetApplicationLocale(),
      base::DefaultClock::GetInstance());
}

// static
void ConsentAuditorFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  consent_auditor::ConsentAuditorImpl::RegisterProfilePrefs(registry);
}
