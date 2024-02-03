// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pending_extension_manager.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

using content::BrowserThread;
using extensions::mojom::ManifestLocation;

namespace {

// Install predicate used by AddFromExternalUpdateUrl().
bool AlwaysInstall(const extensions::Extension* extension,
                   content::BrowserContext* context) {
  return true;
}

std::string GetVersionString(const base::Version& version) {
  return version.IsValid() ? version.GetString() : "invalid";
}

}  // namespace

namespace extensions {

PendingExtensionManager::PendingExtensionManager(
    content::BrowserContext* context)
    : context_(context) {}

PendingExtensionManager::~PendingExtensionManager() {}

const PendingExtensionInfo* PendingExtensionManager::GetById(
    const std::string& id) const {
  auto it = pending_extensions_.find(id);
  if (it != pending_extensions_.end())
    return &it->second;

  return nullptr;
}

bool PendingExtensionManager::Remove(const std::string& id) {
  const bool removed = pending_extensions_.erase(id) > 0;
  if (removed) {
    for (Observer& observer : observers_) {
      observer.OnExtensionRemoved(id);
    }
  }
  return removed;
}

bool PendingExtensionManager::IsIdPending(const std::string& id) const {
  return GetById(id) != nullptr;
}

bool PendingExtensionManager::HasPendingExtensions() const {
  return !pending_extensions_.empty();
}

bool PendingExtensionManager::HasPendingExtensionFromSync() const {
  return base::ranges::any_of(
      pending_extensions_,
      [](const std::pair<const std::string, PendingExtensionInfo>& it) {
        return it.second.is_from_sync();
      });
}

bool PendingExtensionManager::HasHighPriorityPendingExtension() const {
  return base::ranges::any_of(
      pending_extensions_,
      [](const std::pair<const std::string, PendingExtensionInfo>& it) {
        return it.second.install_source() ==
                   ManifestLocation::kExternalPolicyDownload ||
               it.second.install_source() ==
                   ManifestLocation::kExternalComponent;
      });
}

bool PendingExtensionManager::AddFromSync(
    const std::string& id,
    const GURL& update_url,
    const base::Version& version,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool remote_install) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ExtensionRegistry::Get(context_)->GetExtensionById(
          id, ExtensionRegistry::EVERYTHING)) {
    LOG(ERROR) << "Trying to add pending extension " << id
               << " which already exists";
    return false;
  }

  // Make sure we don't ever try to install the CWS app, because even though
  // it is listed as a syncable app (because its values need to be synced) it
  // should already be installed on every instance.
  if (id == extensions::kWebStoreAppId) {
    return false;
  }

  EnsureMigratedDefaultChromeAppIdsCachePopulated();
  if (migrating_default_chrome_app_ids_cache_->contains(id)) {
    base::UmaHistogramBoolean("Extensions.SyncBlockedByDefaultWebAppMigration",
                              true);
    return false;
  }

  static const bool kIsFromSync = true;
  static const mojom::ManifestLocation kSyncLocation =
      mojom::ManifestLocation::kInternal;
  static const bool kMarkAcknowledged = false;

  return AddExtensionImpl(id,
                          std::string(),
                          update_url,
                          version,
                          should_allow_install,
                          kIsFromSync,
                          kSyncLocation,
                          Extension::NO_FLAGS,
                          kMarkAcknowledged,
                          remote_install);
}

bool PendingExtensionManager::AddFromExtensionImport(
    const std::string& id,
    const GURL& update_url,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ExtensionRegistry::Get(context_)->GetExtensionById(
          id, ExtensionRegistry::EVERYTHING)) {
    LOG(ERROR) << "Trying to add pending extension " << id
               << " which already exists";
    return false;
  }

  static const bool kIsFromSync = false;
  static const mojom::ManifestLocation kManifestLocation =
      mojom::ManifestLocation::kInternal;
  static const bool kMarkAcknowledged = false;
  static const bool kRemoteInstall = false;

  return AddExtensionImpl(id,
                          std::string(),
                          update_url,
                          base::Version(),
                          should_allow_install,
                          kIsFromSync,
                          kManifestLocation,
                          Extension::NO_FLAGS,
                          kMarkAcknowledged,
                          kRemoteInstall);
}

bool PendingExtensionManager::AddFromExternalUpdateUrl(
    const std::string& id,
    const std::string& install_parameter,
    const GURL& update_url,
    ManifestLocation location,
    int creation_flags,
    bool mark_acknowledged) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  static const bool kIsFromSync = false;
  static const bool kRemoteInstall = false;

  const Extension* extension = ExtensionRegistry::Get(context_)
      ->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
  if (extension && location == Manifest::GetHigherPriorityLocation(
                                   location, extension->location())) {
    // If the new location has higher priority than the location of an existing
    // extension, let the update process overwrite the existing extension.
  } else {
    // Skip the installation if the extension was removed by the user and it's
    // not specified to be force-installed through the policy.
    if (!Manifest::IsPolicyLocation(location) &&
        ExtensionPrefs::Get(context_)->IsExternalExtensionUninstalled(id)) {
      return false;
    }

    if (extension) {
      LOG(DFATAL) << "Trying to add extension " << id
                  << " by external update, but it is already installed.";
      return false;
    }
  }

  return AddExtensionImpl(id, install_parameter, update_url, base::Version(),
                          &AlwaysInstall, kIsFromSync, location, creation_flags,
                          mark_acknowledged, kRemoteInstall);
}

bool PendingExtensionManager::AddFromExternalFile(
    const std::string& id,
    mojom::ManifestLocation install_source,
    const base::Version& version,
    int creation_flags,
    bool mark_acknowledged) {
  // TODO(skerner): AddFromSync() checks to see if the extension is
  // installed, but this method assumes that the caller already
  // made sure it is not installed.  Make all AddFrom*() methods
  // consistent.
  const GURL& kUpdateUrl = GURL();
  static const bool kIsFromSync = false;
  static const bool kRemoteInstall = false;

  return AddExtensionImpl(id,
                          std::string(),
                          kUpdateUrl,
                          version,
                          &AlwaysInstall,
                          kIsFromSync,
                          install_source,
                          creation_flags,
                          mark_acknowledged,
                          kRemoteInstall);
}

std::list<std::string> PendingExtensionManager::GetPendingIdsForUpdateCheck()
    const {
  std::list<std::string> result;

  for (const auto& it : pending_extensions_) {
    ManifestLocation install_source = it.second.install_source();

    // Some install sources read a CRX from the filesystem.  They can
    // not be fetched from an update URL, so don't include them in the
    // set of ids.
    if (install_source == ManifestLocation::kExternalPref ||
        install_source == ManifestLocation::kExternalRegistry ||
        install_source == ManifestLocation::kExternalPolicy) {
      continue;
    }

    result.push_back(it.first);
  }

  return result;
}

void PendingExtensionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PendingExtensionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PendingExtensionManager::AddExtensionImpl(
    const std::string& id,
    const std::string& install_parameter,
    const GURL& update_url,
    const base::Version& version,
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
    bool is_from_sync,
    mojom::ManifestLocation install_source,
    int creation_flags,
    bool mark_acknowledged,
    bool remote_install) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PendingExtensionInfo info(id, install_parameter, update_url, version,
                            should_allow_install, is_from_sync, install_source,
                            creation_flags, mark_acknowledged, remote_install);

  auto it = pending_extensions_.find(id);
  if (it != pending_extensions_.end()) {
    const PendingExtensionInfo* pending = &(it->second);
    // Bugs in this code will manifest as sporadic incorrect extension
    // locations in situations where multiple install sources run at the
    // same time. For example, on first login to a chrome os machine, an
    // extension may be requested by sync and the default extension set.
    // The following logging will help diagnose such issues.
    VLOG(1) << "Extension id " << id
            << " was entered for update more than once."
            << "  old location: " << pending->install_source()
            << "  new location: " << install_source
            << "  old version: " << GetVersionString(pending->version())
            << "  new version: " << GetVersionString(version);

    // Never override an existing extension with an older version. Only
    // extensions from local CRX files have a known version; extensions from an
    // update URL will get the latest version.

    // If |pending| has the same or higher precedence than |info| then don't
    // install |info| over |pending|.
    if (pending->CompareTo(info) >= 0)
      return false;

    VLOG(1) << "Overwrite existing record.";

    it->second = std::move(info);
  } else {
    AddToMap(id, std::move(info));
  }

  return true;
}

void PendingExtensionManager::
    EnsureMigratedDefaultChromeAppIdsCachePopulated() {
  if (migrating_default_chrome_app_ids_cache_)
    return;

  std::vector<web_app::PreinstalledWebAppMigration> migrations =
      web_app::GetPreinstalledWebAppMigrations(
          *Profile::FromBrowserContext(context_.get()));

  std::vector<std::string> chrome_app_ids;
  chrome_app_ids.reserve(migrations.size());
  for (const web_app::PreinstalledWebAppMigration& migration : migrations)
    chrome_app_ids.push_back(migration.old_chrome_app_id);

  migrating_default_chrome_app_ids_cache_.emplace(std::move(chrome_app_ids));
}

void PendingExtensionManager::AddForTesting(
    PendingExtensionInfo pending_extension_info) {
  std::string id = pending_extension_info.id();
  AddToMap(id, std::move(pending_extension_info));
}

void PendingExtensionManager::AddToMap(const std::string& id,
                                       PendingExtensionInfo info) {
  pending_extensions_.emplace(id, std::move(info));
  for (Observer& observer : observers_) {
    observer.OnExtensionAdded(id);
  }
}

}  // namespace extensions
