// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sync/extension_sync_util.h"

#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/sync/account_extension_tracker.h"
#include "chrome/browser/extensions/sync/extension_sync_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/sync_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions::sync_util {

bool ShouldSync(content::BrowserContext* context, const Extension* extension) {
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(context);
  // Update URL is overridden only for non webstore extensions and offstore
  // extensions should not be synced.
  if (extension_management->IsUpdateUrlOverridden(extension->id())) {
    const GURL update_url =
        extension_management->GetEffectiveUpdateURL(*extension);
    DCHECK(!extension_urls::IsWebstoreUpdateUrl(update_url))
        << "Update URL cannot be overridden to be the webstore URL!";
    return false;
  }
  return sync_helper::IsSyncable(extension) &&
         !ExtensionPrefs::Get(context)->DoNotSync(extension->id()) &&
         !extensions::blocklist_prefs::IsExtensionBlocklisted(
             extension->id(), ExtensionPrefs::Get(context));
}

bool IsSyncingExtensionsEnabled(Profile* profile) {
  // TODO(crbug.com/388557898): If this method is called from
  // IdentityManagerObserver::OnPrimaryAccountChanged, then it could return the
  // wrong value since the sync service also piggybacks on that event to update
  // which data types are syncing.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return sync_service &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kExtensions);
}

bool IsSyncingExtensionsInTransportMode(Profile* profile) {
  // Prefer querying the IdentityManager for consent levels since it's the base
  // source of truth, and something like sync_service->HasSyncConsent() is a bit
  // slower to update since it observes IdentityManager.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return IsSyncingExtensionsEnabled(profile) &&
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
         !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
}

void UploadExtensionToAccount(content::BrowserContext* context,
                              const Extension& extension) {
  AccountExtensionTracker::Get(context)->OnAccountUploadInitiatedForExtension(
      extension.id());
  ExtensionSyncService::Get(context)->SyncExtensionChangeIfNeeded(extension);
}

}  // namespace extensions::sync_util
