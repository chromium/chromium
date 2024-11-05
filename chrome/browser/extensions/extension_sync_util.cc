// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_util.h"

#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/sync_helper.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "url/gurl.h"

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
         !ExtensionPrefs::Get(context)->DoNotSync(extension->id());
}

bool IsSyncingExtensionsEnabled(Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return sync_service &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kExtensions);
}

bool IsSyncingExtensionsInTransportMode(Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return IsSyncingExtensionsEnabled(profile) && !sync_service->HasSyncConsent();
}

bool IsExtensionsExplicitSigninEnabled() {
  // Explicit sign ins for extensions are enabled if extensions can be synced if
  // the user signs into transport mode.
  return switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
         base::FeatureList::IsEnabled(
             syncer::kSyncEnableExtensionsInTransportMode);
}

}  // namespace extensions::sync_util
