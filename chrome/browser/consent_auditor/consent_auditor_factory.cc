// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/consent_auditor/consent_auditor_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/consent_auditor/consent_auditor_impl.h"
#include "components/consent_auditor/consent_sync_bridge.h"
#include "components/consent_auditor/consent_sync_bridge_impl.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"

// static
ConsentAuditorFactory* ConsentAuditorFactory::GetInstance() {
  static base::NoDestructor<ConsentAuditorFactory> instance;
  return instance.get();
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
    : ProfileKeyedServiceFactory(
          "ConsentAuditor",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

ConsentAuditorFactory::~ConsentAuditorFactory() = default;

std::unique_ptr<KeyedService>
ConsentAuditorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  std::unique_ptr<consent_auditor::ConsentSyncBridge> consent_sync_bridge;
  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::USER_CONSENTS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  consent_sync_bridge =
      std::make_unique<consent_auditor::ConsentSyncBridgeImpl>(
          std::move(store_factory), std::move(change_processor));

  return std::make_unique<consent_auditor::ConsentAuditorImpl>(
      std::move(consent_sync_bridge),
      // The locale doesn't change at runtime, so we can pass it directly.
      g_browser_process->GetApplicationLocale(),
      base::DefaultClock::GetInstance());
}
